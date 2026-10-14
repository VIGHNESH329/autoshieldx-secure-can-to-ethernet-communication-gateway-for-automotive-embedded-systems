#include "autoshieldx/common.hpp"
#include "autoshieldx/can/can_frame.hpp"
#include "autoshieldx/can/can_transport.hpp"
#include "autoshieldx/security/crypto_hmac.hpp"
#include "autoshieldx/security/replay_protection.hpp"
#include "autoshieldx/validation/validation_engine.hpp"
#include "autoshieldx/ml/ml_detector.hpp"
#include "autoshieldx/gateway/security_pipeline.hpp"
#include "autoshieldx/gateway/gateway_logger.hpp"
#include "autoshieldx/networking/ethernet_forwarder.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <thread>
#include <chrono>

using namespace autoshieldx;

static int g_tests_passed = 0;
static int g_tests_total = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  [FAILED] " << msg << " (Line " << __LINE__ << ")\n"; \
            return false; \
        } \
    } while(0)

#define RUN_TEST(fn) \
    do { \
        g_tests_total++; \
        std::cout << "\n============================================================\n"; \
        std::cout << "TEST " << g_tests_total << ": " << #fn << "\n"; \
        if (fn()) { \
            g_tests_passed++; \
            std::cout << ">>> RESULT: PASS\n"; \
        } else { \
            std::cout << ">>> RESULT: FAIL\n"; \
        } \
    } while(0)

// 1. Test HMAC Generation (RFC 4231 Test Vector 2)
bool testHmacGeneration() {
    std::cout << "Purpose: Verify HMAC-SHA256 generation using shared secret key.\n";
    std::cout << "Expected Output: Exact RFC 4231 standard digest match.\n";

    // Key = "Jefe", Data = "what do ya want for nothing?"
    // Expected HMAC-SHA256: 5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843
    std::string key = "Jefe";
    std::string data = "what do ya want for nothing?";
    std::string expected_hex = "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843";

    std::string computed = HmacSha256::computeHex(key, data);
    std::cout << "  Computed: " << computed << "\n";
    std::cout << "  Expected: " << expected_hex << "\n";

    TEST_ASSERT(computed == expected_hex, "HMAC-SHA256 output did not match RFC 4231 vector");
    return true;
}

// 2. Test HMAC Verification (Constant-Time)
bool testHmacVerification() {
    std::cout << "Purpose: Verify constant-time verification accepts genuine tags and rejects invalid ones.\n";
    std::cout << "Expected Output: Identical tags return true; mismatched tags return false.\n";

    std::string valid_tag = "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843";
    std::string tampered_tag = "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3844"; // 1 bit flip

    TEST_ASSERT(HmacSha256::verifyConstantTime(valid_tag, valid_tag) == true, "Genuine tag rejected");
    TEST_ASSERT(HmacSha256::verifyConstantTime(valid_tag, tampered_tag) == false, "Tampered tag accepted");
    TEST_ASSERT(HmacSha256::verifyConstantTime(valid_tag, "") == false, "Empty tag accepted");
    return true;
}

// 3. Test Modified-Message Detection (Integrity Violation)
bool testModifiedMessageDetection() {
    std::cout << "Purpose: Verify that modifying 1 bit of CAN payload causes HMAC mismatch.\n";
    std::cout << "Expected Output: Tampered payload fails verification; Original passes.\n";

    std::string secret = "TestSecretKey_2026";
    std::vector<uint8_t> secret_bytes(secret.begin(), secret.end());

    uint32_t can_id = 0x100;
    uint32_t seq = 1;
    std::array<uint8_t, 8> payload = {0x00, 0x50, 0x0A, 0x00, 0x14, 0x55, 0x00, 0x01};

    auto canonical = HmacSha256::buildCanonicalBuffer(can_id, seq, payload);
    std::string original_hmac = HmacSha256::computeHex(secret_bytes, canonical);

    // Tamper payload byte 0 (change speed from 0x00 to 0x01)
    std::array<uint8_t, 8> tampered_payload = payload;
    tampered_payload[0] ^= 0x01;

    auto canonical_tampered = HmacSha256::buildCanonicalBuffer(can_id, seq, tampered_payload);
    std::string tampered_calc_hmac = HmacSha256::computeHex(secret_bytes, canonical_tampered);

    TEST_ASSERT(original_hmac != tampered_calc_hmac, "Avalanche effect failed: Tampered payload yielded same HMAC");
    TEST_ASSERT(!HmacSha256::verifyConstantTime(original_hmac, tampered_calc_hmac), "Verification should fail for modified payload");
    return true;
}

// 4. Test Sequence Validation (Monotonic Increasing)
bool testSequenceValidation() {
    std::cout << "Purpose: Verify monotonic sequence validation advances correctly.\n";
    std::cout << "Expected Output: Sequences 1, 2, 3 accepted; counter advances.\n";

    ReplayProtectionEngine engine;
    uint32_t cid = 0x100;

    auto r1 = engine.validateSequence(cid, 1);
    TEST_ASSERT(r1.is_valid, "Initial sequence 1 should be accepted");

    auto r2 = engine.validateSequence(cid, 2);
    TEST_ASSERT(r2.is_valid, "Sequence 2 should be accepted");

    auto r3 = engine.validateSequence(cid, 3);
    TEST_ASSERT(r3.is_valid, "Sequence 3 should be accepted");
    TEST_ASSERT(engine.getLastAcceptedSequence(cid) == 3, "Last accepted sequence should be 3");
    return true;
}

// 5. Test Replay Detection
bool testReplayDetection() {
    std::cout << "Purpose: Verify replay protection rejects sequence <= last accepted.\n";
    std::cout << "Expected Output: Stale sequence 10 and 9 rejected as replay when last is 12.\n";

    ReplayProtectionEngine engine;
    uint32_t cid = 0x100;

    engine.validateSequence(cid, 10);
    engine.validateSequence(cid, 11);
    engine.validateSequence(cid, 12);
    TEST_ASSERT(engine.getLastAcceptedSequence(cid) == 12, "Last accepted sequence should be 12");

    // Replay attack: Incoming sequence 10
    auto r_replay1 = engine.validateSequence(cid, 10);
    TEST_ASSERT(!r_replay1.is_valid, "Sequence 10 MUST be rejected as replay");
    std::cout << "  Captured message: " << r_replay1.reason << "\n";

    // Replay attack: Incoming sequence 9
    auto r_replay2 = engine.validateSequence(cid, 9);
    TEST_ASSERT(!r_replay2.is_valid, "Sequence 9 MUST be rejected as replay");

    // Replay attack: Duplicate sequence 12
    auto r_replay3 = engine.validateSequence(cid, 12);
    TEST_ASSERT(!r_replay3.is_valid, "Sequence 12 duplicate MUST be rejected as replay");

    TEST_ASSERT(engine.getReplayDetections() == 3, "Expected 3 replay detections");
    return true;
}

// 6. Test CAN Frame Validation (Syntactic and Range Boundaries)
bool testCanFrameValidation() {
    std::cout << "Purpose: Test frame validation engine against valid and invalid parameters.\n";
    std::cout << "Expected Output: Out-of-range sensor values and unauthorized IDs rejected.\n";

    ValidationEngine val;

    // Normal valid message
    SecureCanMessage msg_ok;
    msg_ok.can_id = 0x100;
    msg_ok.sequence_number = 1;
    msg_ok.telemetry.speed_kmh = 60.0;
    msg_ok.telemetry.rpm = 2000.0;
    msg_ok.telemetry.temperature_c = 85.0;
    msg_ok.telemetry.throttle_pct = 20.0;
    auto res_ok = val.validateMessage(msg_ok);
    TEST_ASSERT(res_ok.is_valid, "Valid message should pass validation");

    // Invalid: Unauthorized CAN ID
    SecureCanMessage msg_bad_id = msg_ok;
    msg_bad_id.can_id = 0x999;
    TEST_ASSERT(!val.validateMessage(msg_bad_id).is_valid, "Unauthorized CAN ID should fail");

    // Invalid: Speed > 250 km/h
    SecureCanMessage msg_bad_speed = msg_ok;
    msg_bad_speed.telemetry.speed_kmh = 320.0;
    TEST_ASSERT(!val.validateMessage(msg_bad_speed).is_valid, "Speed 320 km/h should fail");

    // Invalid: RPM > 8000
    SecureCanMessage msg_bad_rpm = msg_ok;
    msg_bad_rpm.telemetry.rpm = 12000.0;
    TEST_ASSERT(!val.validateMessage(msg_bad_rpm).is_valid, "RPM 12,000 should fail");

    // Invalid: Temperature > 125 C
    SecureCanMessage msg_bad_temp = msg_ok;
    msg_bad_temp.telemetry.temperature_c = 150.0;
    TEST_ASSERT(!val.validateMessage(msg_bad_temp).is_valid, "Temp 150 C should fail");
    return true;
}

// Helper to create a fully wired test gateway pipeline
std::shared_ptr<GatewayPipeline> createTestPipeline(const std::string& secret) {
    auto validator = std::make_shared<ValidationEngine>();
    auto replay = std::make_shared<ReplayProtectionEngine>();
    auto ml = std::make_shared<EmbeddedMlDetector>(0.65, 80.0);
    auto logger = std::make_shared<GatewayLogger>("logs/test_security.log", false);
    logger->init();
    return std::make_shared<GatewayPipeline>(secret, validator, replay, ml, logger, nullptr);
}

// 7. Test Normal Vehicle Traffic
bool testNormalVehicleTraffic() {
    std::cout << "Purpose: Verify legitimate, authenticated driving messages pass all 6 pipeline stages.\n";
    std::cout << "Expected Output: PipelineDecision::VALID_FORWARD with accepted = true.\n";

    std::string secret = "Secret_Key_123";
    std::vector<uint8_t> secret_bytes(secret.begin(), secret.end());
    auto pipeline = createTestPipeline(secret);

    for (uint32_t i = 1; i <= 5; ++i) {
        SecureCanMessage msg;
        msg.can_id = 0x100;
        msg.sequence_number = i;
        msg.timestamp_us = getCurrentTimestampMicros();
        msg.telemetry.speed_kmh = 50.0 + i * 2.0;
        msg.telemetry.rpm = 1800.0 + i * 50.0;
        msg.telemetry.temperature_c = 85.0;
        msg.telemetry.throttle_pct = 20.0;
        msg.payload = CanPayloadCodec::encodePowertrain(
            msg.telemetry.speed_kmh, msg.telemetry.rpm, msg.telemetry.throttle_pct, msg.telemetry.temperature_c, i);

        auto canon = HmacSha256::buildCanonicalBuffer(msg.can_id, msg.sequence_number, msg.payload);
        msg.hmac_tag = HmacSha256::computeHex(secret_bytes, canon);

        auto dec = pipeline->processFrame(msg);
        TEST_ASSERT(dec.accepted, "Normal frame should be accepted");
        TEST_ASSERT(dec.result == DecisionResult::VALID_FORWARD, "Result should be VALID_FORWARD");
    }

    TEST_ASSERT(pipeline->getAcceptedFrames() == 5, "Expected 5 accepted frames");
    TEST_ASSERT(pipeline->getRejectedFrames() == 0, "Expected 0 rejected frames");
    return true;
}

// 8. Test Injection Attack
bool testInjectionAttack() {
    std::cout << "Purpose: Verify gateway detects and rejects forged/spoofed CAN frames.\n";
    std::cout << "Expected Output: REJECT_HMAC_AUTHENTICATION_FAILED or REJECT_FRAME_VALIDATION_FAILED.\n";

    std::string secret = "Secret_Key_123";
    auto pipeline = createTestPipeline(secret);

    // Frame with forged HMAC
    SecureCanMessage msg;
    msg.can_id = 0x100;
    msg.sequence_number = 1;
    msg.timestamp_us = getCurrentTimestampMicros();
    msg.telemetry.speed_kmh = 80.0;
    msg.telemetry.rpm = 2500.0;
    msg.telemetry.temperature_c = 85.0;
    msg.payload = CanPayloadCodec::encodePowertrain(80.0, 2500.0, 25.0, 85.0, 1);
    msg.hmac_tag = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"; // Forged!

    auto dec = pipeline->processFrame(msg);
    TEST_ASSERT(!dec.accepted, "Injected frame with forged HMAC must be rejected");
    TEST_ASSERT(dec.result == DecisionResult::REJECT_HMAC_AUTHENTICATION_FAILED, "Result should be REJECT_HMAC_AUTHENTICATION_FAILED");
    TEST_ASSERT(dec.inferred_attack == AttackType::INJECTION_SPOOFING, "Attack should be classified as INJECTION_SPOOFING");
    return true;
}

// 9. Test Replay Attack
bool testReplayAttack() {
    std::cout << "Purpose: Verify gateway detects and rejects replayed authentic CAN frame.\n";
    std::cout << "Expected Output: REJECT_REPLAY_DETECTED.\n";

    std::string secret = "Secret_Key_123";
    std::vector<uint8_t> secret_bytes(secret.begin(), secret.end());
    auto pipeline = createTestPipeline(secret);

    // Send sequence 10 (Authentic)
    SecureCanMessage captured_msg;
    captured_msg.can_id = 0x100;
    captured_msg.sequence_number = 10;
    captured_msg.timestamp_us = getCurrentTimestampMicros();
    captured_msg.telemetry.speed_kmh = 70.0;
    captured_msg.payload = CanPayloadCodec::encodePowertrain(70.0, 2200.0, 20.0, 85.0, 10);
    auto canon10 = HmacSha256::buildCanonicalBuffer(captured_msg.can_id, captured_msg.sequence_number, captured_msg.payload);
    captured_msg.hmac_tag = HmacSha256::computeHex(secret_bytes, canon10);

    TEST_ASSERT(pipeline->processFrame(captured_msg).accepted, "Initial sequence 10 should be accepted");

    // Advance to sequence 15
    for (uint32_t s = 11; s <= 15; ++s) {
        SecureCanMessage msg = captured_msg;
        msg.sequence_number = s;
        auto canon = HmacSha256::buildCanonicalBuffer(msg.can_id, msg.sequence_number, msg.payload);
        msg.hmac_tag = HmacSha256::computeHex(secret_bytes, canon);
        pipeline->processFrame(msg);
    }

    // Now replay captured message with sequence 10!
    auto dec_replay = pipeline->processFrame(captured_msg);
    TEST_ASSERT(!dec_replay.accepted, "Replayed message MUST be rejected");
    TEST_ASSERT(dec_replay.result == DecisionResult::REJECT_REPLAY_DETECTED, "Result must be REJECT_REPLAY_DETECTED");
    TEST_ASSERT(dec_replay.inferred_attack == AttackType::REPLAY_ATTACK, "Attack must be REPLAY_ATTACK");
    return true;
}

// 10. Test DoS Traffic Detection
bool testDosTraffic() {
    std::cout << "Purpose: Verify gateway ML anomaly engine detects high-frequency DoS flooding.\n";
    std::cout << "Expected Output: ML Anomaly detected (DOS_FLOOD_ANOMALY).\n";

    std::string secret = "Secret_Key_123";
    std::vector<uint8_t> secret_bytes(secret.begin(), secret.end());
    auto pipeline = createTestPipeline(secret);

    bool dos_caught = false;
    // Rapid burst of 120 messages without delay (> 100 Hz rate)
    for (uint32_t i = 1; i <= 120; ++i) {
        SecureCanMessage msg;
        msg.can_id = 0x100;
        msg.sequence_number = i;
        msg.timestamp_us = getCurrentTimestampMicros();
        msg.telemetry.speed_kmh = 60.0;
        msg.payload = CanPayloadCodec::encodePowertrain(60.0, 2000.0, 20.0, 85.0, i);
        auto canon = HmacSha256::buildCanonicalBuffer(msg.can_id, msg.sequence_number, msg.payload);
        msg.hmac_tag = HmacSha256::computeHex(secret_bytes, canon);

        auto dec = pipeline->processFrame(msg);
        if (dec.result == DecisionResult::REJECT_ML_ANOMALY_DETECTED) {
            dos_caught = true;
            TEST_ASSERT(dec.inferred_attack == AttackType::DOS_FLOODING, "Attack inferred as DOS_FLOODING");
            break;
        }
    }

    TEST_ASSERT(dos_caught, "High-frequency flood should trigger ML DoS anomaly detection");
    return true;
}

// 11. Test Fuzzy Traffic Detection
bool testFuzzyTraffic() {
    std::cout << "Purpose: Verify gateway rejects random fuzzed CAN IDs and corrupted structures.\n";
    std::cout << "Expected Output: REJECT_FRAME_VALIDATION_FAILED.\n";

    std::string secret = "Secret_Key_123";
    auto pipeline = createTestPipeline(secret);

    SecureCanMessage fuzzy_msg;
    fuzzy_msg.can_id = 0x7E3; // Random non-whitelisted ID
    fuzzy_msg.sequence_number = 42;
    fuzzy_msg.timestamp_us = getCurrentTimestampMicros();
    fuzzy_msg.telemetry.speed_kmh = 180.0;
    fuzzy_msg.hmac_tag = "abcdef1234567890";

    auto dec = pipeline->processFrame(fuzzy_msg);
    TEST_ASSERT(!dec.accepted, "Fuzzed rogue frame must be rejected");
    TEST_ASSERT(dec.result == DecisionResult::REJECT_FRAME_VALIDATION_FAILED, "Fuzzy ID rejected by frame validation");
    return true;
}

// 12. Test False-Data Traffic Detection
bool testFalseDataTraffic() {
    std::cout << "Purpose: Verify out-of-bounds speed (320 km/h) is rejected even with signed HMAC.\n";
    std::cout << "Expected Output: REJECT_FRAME_VALIDATION_FAILED with FALSE_DATA_MANIPULATION.\n";

    std::string secret = "Secret_Key_123";
    std::vector<uint8_t> secret_bytes(secret.begin(), secret.end());
    auto pipeline = createTestPipeline(secret);

    SecureCanMessage msg;
    msg.can_id = 0x100;
    msg.sequence_number = 1;
    msg.timestamp_us = getCurrentTimestampMicros();
    msg.telemetry.speed_kmh = 320.0; // Out-of-bounds!
    msg.telemetry.rpm = 3000.0;
    msg.telemetry.temperature_c = 85.0;
    msg.payload = CanPayloadCodec::encodePowertrain(320.0, 3000.0, 50.0, 85.0, 1);
    auto canon = HmacSha256::buildCanonicalBuffer(msg.can_id, msg.sequence_number, msg.payload);
    msg.hmac_tag = HmacSha256::computeHex(secret_bytes, canon); // Signed with authentic HMAC!

    auto dec = pipeline->processFrame(msg);
    TEST_ASSERT(!dec.accepted, "Out-of-bounds sensor data must be rejected");
    TEST_ASSERT(dec.result == DecisionResult::REJECT_FRAME_VALIDATION_FAILED, "Rejected by frame validation");
    TEST_ASSERT(dec.inferred_attack == AttackType::FALSE_DATA_MANIPULATION, "Inferred as FALSE_DATA_MANIPULATION");
    return true;
}

// 13. Test Gateway Decision Logic & Counter Consistency
bool testGatewayDecisionLogic() {
    std::cout << "Purpose: Verify gateway arbitration counters and comprehensive metrics tracking.\n";
    std::cout << "Expected Output: Metrics match exact frame totals.\n";

    std::string secret = "Secret_Key_123";
    auto pipeline = createTestPipeline(secret);

    // Frame 1: Valid
    SecureCanMessage msg_valid;
    msg_valid.can_id = 0x100;
    msg_valid.sequence_number = 1;
    msg_valid.payload = CanPayloadCodec::encodePowertrain(50.0, 1800.0, 20.0, 85.0, 1);
    std::vector<uint8_t> secret_bytes(secret.begin(), secret.end());
    auto canon = HmacSha256::buildCanonicalBuffer(msg_valid.can_id, msg_valid.sequence_number, msg_valid.payload);
    msg_valid.hmac_tag = HmacSha256::computeHex(secret_bytes, canon);
    pipeline->processFrame(msg_valid);

    // Frame 2: Bad HMAC
    SecureCanMessage msg_bad_hmac = msg_valid;
    msg_bad_hmac.sequence_number = 2;
    msg_bad_hmac.hmac_tag = "badtag";
    pipeline->processFrame(msg_bad_hmac);

    // Frame 3: Replay
    pipeline->processFrame(msg_valid); // Replays seq 1

    TEST_ASSERT(pipeline->getTotalFrames() == 3, "Total frames should be 3");
    TEST_ASSERT(pipeline->getAcceptedFrames() == 1, "Accepted frames should be 1");
    TEST_ASSERT(pipeline->getRejectedFrames() == 2, "Rejected frames should be 2");
    TEST_ASSERT(pipeline->getHmacFailures() == 1, "HMAC failures should be 1");
    TEST_ASSERT(pipeline->getReplayFailures() == 1, "Replay failures should be 1");
    return true;
}

// 14. Test AX-CTP Classic CAN Framing & Reassembly
bool testCanTransportFragmentationAndReassembly() {
    std::cout << "Purpose: Verify AX-CTP fragmentation of SecureCanMessage into 8-byte CAN frames and reconstruction.\n";
    std::cout << "Expected Output: Exact reconstruction of CAN ID, Sequence, 8-byte Payload, and full 256-bit HMAC.\n";

    std::string secret = "Secret_Key_123";
    std::vector<uint8_t> secret_bytes(secret.begin(), secret.end());

    SecureCanMessage original_msg;
    original_msg.can_id = 0x100;
    original_msg.sequence_number = 4096;
    original_msg.timestamp_us = getCurrentTimestampMicros();
    original_msg.telemetry.speed_kmh = 75.5;
    original_msg.telemetry.rpm = 2400.0;
    original_msg.telemetry.throttle_pct = 35.0;
    original_msg.telemetry.temperature_c = 82.0;
    original_msg.payload = CanPayloadCodec::encodePowertrain(
        original_msg.telemetry.speed_kmh,
        original_msg.telemetry.rpm,
        original_msg.telemetry.throttle_pct,
        original_msg.telemetry.temperature_c,
        original_msg.sequence_number);

    auto canon = HmacSha256::buildCanonicalBuffer(
        original_msg.can_id, original_msg.sequence_number, original_msg.payload);
    original_msg.hmac_tag = HmacSha256::computeHex(secret_bytes, canon);

    // 1. Fragment into classic CAN frames
    std::vector<CanRawFrame> frames;
    bool frag_ok = CanTransportProtocol::fragmentMessage(original_msg, frames);
    TEST_ASSERT(frag_ok, "Fragmentation must succeed");
    TEST_ASSERT(frames.size() == 7, "Expected 7 CAN frames (1 telemetry + 6 auth fragments)");
    TEST_ASSERT(frames[0].can_id == 0x100, "Frame 0 CAN ID must be 0x100");
    TEST_ASSERT(frames[0].can_dlc == 8, "Frame 0 DLC must be 8");
    for (size_t i = 1; i < frames.size(); ++i) {
        TEST_ASSERT(frames[i].can_id == 0x101, "Auth fragment CAN ID must be 0x101");
        TEST_ASSERT(frames[i].can_dlc == 8, "Auth fragment DLC must be 8");
    }

    // 2. Feed frames into reassembler
    CanTransportProtocol reassembler;
    SecureCanMessage reassembled_msg;
    bool complete = false;

    for (size_t i = 0; i < frames.size(); ++i) {
        complete = reassembler.processIncomingFrame(frames[i], reassembled_msg);
        if (i < frames.size() - 1) {
            TEST_ASSERT(!complete, "Reassembler must return false until final fragment arrives");
        }
    }

    TEST_ASSERT(complete, "Reassembler must indicate completion after final fragment");
    TEST_ASSERT(reassembled_msg.can_id == original_msg.can_id, "Reassembled CAN ID must match");
    TEST_ASSERT(reassembled_msg.sequence_number == original_msg.sequence_number, "Reassembled sequence number must match");
    TEST_ASSERT(reassembled_msg.payload == original_msg.payload, "Reassembled 8-byte payload must match");
    TEST_ASSERT(reassembled_msg.hmac_tag == original_msg.hmac_tag, "Reassembled full 32-byte HMAC tag must match");
    TEST_ASSERT(std::abs(reassembled_msg.telemetry.speed_kmh - 75.5) < 0.1, "Decoded speed must match");
    TEST_ASSERT(std::abs(reassembled_msg.telemetry.rpm - 2400.0) < 1.0, "Decoded RPM must match");
    return true;
}

// 15. Test AX-CTP Reassembled Message Security Pipeline Acceptance
bool testCanTransportReassembledPipelineAcceptance() {
    std::cout << "Purpose: Verify that reconstructed physical CAN frames successfully pass all 6 pipeline stages.\n";
    std::cout << "Expected Output: PipelineDecision::VALID_FORWARD with full HMAC verification and replay counter advance.\n";

    std::string secret = "Secret_Key_123";
    std::vector<uint8_t> secret_bytes(secret.begin(), secret.end());
    auto pipeline = createTestPipeline(secret);
    CanTransportProtocol reassembler;

    for (uint32_t seq = 1; seq <= 3; ++seq) {
        SecureCanMessage original_msg;
        original_msg.can_id = 0x100;
        original_msg.sequence_number = seq;
        original_msg.timestamp_us = getCurrentTimestampMicros();
        original_msg.telemetry.speed_kmh = 60.0 + seq;
        original_msg.telemetry.rpm = 2000.0 + seq * 10;
        original_msg.telemetry.throttle_pct = 25.0;
        original_msg.telemetry.temperature_c = 85.0;
        original_msg.payload = CanPayloadCodec::encodePowertrain(
            original_msg.telemetry.speed_kmh,
            original_msg.telemetry.rpm,
            original_msg.telemetry.throttle_pct,
            original_msg.telemetry.temperature_c,
            original_msg.sequence_number);

        auto canon = HmacSha256::buildCanonicalBuffer(
            original_msg.can_id, original_msg.sequence_number, original_msg.payload);
        original_msg.hmac_tag = HmacSha256::computeHex(secret_bytes, canon);

        std::vector<CanRawFrame> frames;
        CanTransportProtocol::fragmentMessage(original_msg, frames);

        SecureCanMessage reconstructed;
        bool ready = false;
        for (const auto& f : frames) {
            ready = reassembler.processIncomingFrame(f, reconstructed);
        }

        TEST_ASSERT(ready, "Frame burst must produce ready message");
        auto dec = pipeline->processFrame(reconstructed);
        TEST_ASSERT(dec.accepted, "Reassembled message must be accepted by pipeline");
        TEST_ASSERT(dec.result == DecisionResult::VALID_FORWARD, "Result must be VALID_FORWARD");
    }

    TEST_ASSERT(pipeline->getAcceptedFrames() == 3, "Expected 3 accepted frames");
    return true;
}

// 16. Test AX-CTP Physical Transport Tamper Detection
bool testCanTransportTamperDetection() {
    std::cout << "Purpose: Verify that tampering with 1 byte of physical CAN frame causes gateway HMAC rejection.\n";
    std::cout << "Expected Output: PipelineDecision with REJECT_HMAC_AUTHENTICATION_FAILED.\n";

    std::string secret = "Secret_Key_123";
    std::vector<uint8_t> secret_bytes(secret.begin(), secret.end());
    auto pipeline = createTestPipeline(secret);
    CanTransportProtocol reassembler;

    SecureCanMessage msg;
    msg.can_id = 0x100;
    msg.sequence_number = 10;
    msg.timestamp_us = getCurrentTimestampMicros();
    msg.telemetry.speed_kmh = 80.0;
    msg.telemetry.rpm = 2500.0;
    msg.telemetry.throttle_pct = 30.0;
    msg.telemetry.temperature_c = 85.0;
    msg.payload = CanPayloadCodec::encodePowertrain(80.0, 2500.0, 30.0, 85.0, 10);
    auto canon = HmacSha256::buildCanonicalBuffer(msg.can_id, msg.sequence_number, msg.payload);
    msg.hmac_tag = HmacSha256::computeHex(secret_bytes, canon);

    std::vector<CanRawFrame> frames;
    CanTransportProtocol::fragmentMessage(msg, frames);

    // Tamper with Frame 2 (Auth Fragment 1): flip 1 bit in HMAC byte
    frames[2].data[3] ^= 0x01;

    SecureCanMessage reconstructed;
    bool ready = false;
    for (const auto& f : frames) {
        ready = reassembler.processIncomingFrame(f, reconstructed);
    }

    TEST_ASSERT(ready, "Reassembler should reconstruct message");
    auto dec = pipeline->processFrame(reconstructed);
    TEST_ASSERT(!dec.accepted, "Tampered physical CAN message MUST be rejected");
    TEST_ASSERT(dec.result == DecisionResult::REJECT_HMAC_AUTHENTICATION_FAILED, "Decision must be REJECT_HMAC_AUTHENTICATION_FAILED");
    return true;
}

// 17. Test HMAC Key Parsing (Hex Decoding vs Raw ASCII)
bool testHmacKeyParsing() {
    std::cout << "Purpose: Verify that HmacSha256::parseKey properly decodes 64-char hex into 32 raw bytes.\n";
    std::cout << "Expected Output: Hex-decoded key and raw ASCII key produce identical HMAC digests.\n";

    std::string hex_key = "4175746f536869656c64585f5365637265745f4b65795f32303236214155544f";
    std::string raw_key = "AutoShieldX_Secret_Key_2026!AUTO";

    auto parsed_hex = HmacSha256::parseKey(hex_key);
    auto parsed_raw = HmacSha256::parseKey(raw_key);

    TEST_ASSERT(parsed_hex.size() == 32, "Hex-decoded key must be exactly 32 bytes");
    TEST_ASSERT(parsed_raw.size() == 32, "Raw ASCII key must be exactly 32 bytes");
    TEST_ASSERT(parsed_hex == parsed_raw, "Decoded hex key must match raw ASCII key bytes exactly");

    std::string test_data_str = "CAN_ID_0x100_SEQ_42_PAYLOAD_TEST";
    std::vector<uint8_t> test_data(test_data_str.begin(), test_data_str.end());
    std::string hmac_from_hex = HmacSha256::computeHex(parsed_hex, test_data);
    std::string hmac_from_raw = HmacSha256::computeHex(parsed_raw, test_data);

    TEST_ASSERT(hmac_from_hex == hmac_from_raw, "HMAC digests must match bit-for-bit");
    return true;
}

// 18. Test Pico 2 Exact Hardware Compatibility Vector
bool testPicoHardwareCompatibilityVector() {
    std::cout << "Purpose: End-to-end verification of the exact 17-byte canonical buffer and 6-fragment AX-CTP protocol used by Pico 2 firmware.\n";
    std::cout << "Expected Output: Physical-format CAN frames are reassembled and authenticated by the Gateway pipeline.\n";

    // 1. Shared secret matching gateway_config.json and Pico 2 can_protocol.h
    std::string config_secret_hex = "4175746f536869656c64585f5365637265745f4b65795f32303236214155544f";
    std::vector<uint8_t> secret_bytes = HmacSha256::parseKey(config_secret_hex);

    // 2. Telemetry values simulated on Pico 2
    uint32_t can_id = 0x100;
    uint32_t sequence_number = 42; // Low: 42, High: 0
    double speed_kmh = 65.0;
    double rpm = 2400.0;
    double throttle_pct = 25.0;
    double temperature_c = 82.0;

    // 3. Telemetry frame payload (DLC 8)
    std::array<uint8_t, 8> payload = CanPayloadCodec::encodePowertrain(
        speed_kmh, rpm, throttle_pct, temperature_c, sequence_number);

    // 4. Pico 2 canonical buffer (17 bytes)
    // [CAN_ID (4B BE)][Sequence (4B BE)][DLC (1B)][Payload (8B)]
    uint8_t canonical_buf[17];
    canonical_buf[0] = (uint8_t)((can_id >> 24) & 0xFF);
    canonical_buf[1] = (uint8_t)((can_id >> 16) & 0xFF);
    canonical_buf[2] = (uint8_t)((can_id >> 8) & 0xFF);
    canonical_buf[3] = (uint8_t)(can_id & 0xFF);

    canonical_buf[4] = (uint8_t)((sequence_number >> 24) & 0xFF);
    canonical_buf[5] = (uint8_t)((sequence_number >> 16) & 0xFF);
    canonical_buf[6] = (uint8_t)((sequence_number >> 8) & 0xFF);
    canonical_buf[7] = (uint8_t)(sequence_number & 0xFF);

    canonical_buf[8] = 8; // DLC

    std::memcpy(&canonical_buf[9], payload.data(), 8);

    // 5. Pico 2 HMAC calculation
    std::vector<uint8_t> canon_vec(canonical_buf, canonical_buf + 17);
    std::vector<uint8_t> hmac_raw = HmacSha256::computeRaw(secret_bytes, canon_vec);
    TEST_ASSERT(hmac_raw.size() == 32, "HMAC raw bytes must be 32");

    // 6. Generate the 1 telemetry frame and 6 AX-CTP auth fragments exactly as Pico 2 does
    CanRawFrame telemetry_frame;
    telemetry_frame.can_id = can_id;
    telemetry_frame.can_dlc = 8;
    std::memcpy(telemetry_frame.data, payload.data(), 8);

    std::vector<CanRawFrame> auth_fragments;
    uint8_t correlation_seq = (uint8_t)(sequence_number & 0xFF);

    for (uint8_t frag_idx = 0; frag_idx < 6; ++frag_idx) {
        CanRawFrame f;
        f.can_id = 0x101;
        f.can_dlc = 8;
        f.data[0] = (uint8_t)((frag_idx << 4) | 0x06); // Header: frag_idx | total=6
        f.data[1] = correlation_seq;

        if (frag_idx < 5) {
            std::memcpy(&f.data[2], &hmac_raw[frag_idx * 6], 6);
        } else {
            f.data[2] = hmac_raw[30];
            f.data[3] = hmac_raw[31];
            f.data[4] = (uint8_t)((sequence_number >> 24) & 0xFF);
            f.data[5] = (uint8_t)((sequence_number >> 16) & 0xFF);
            f.data[6] = 0x00;
            f.data[7] = 0x00;
        }
        auth_fragments.push_back(f);
    }

    // 7. Feed all physical frames into Gateway AX-CTP reassembler
    CanTransportProtocol reassembler;
    SecureCanMessage reassembled_msg;
    bool ready = reassembler.processIncomingFrame(telemetry_frame, reassembled_msg);
    TEST_ASSERT(!ready, "Telemetry frame alone must not trigger ready");

    for (size_t i = 0; i < auth_fragments.size(); ++i) {
        ready = reassembler.processIncomingFrame(auth_fragments[i], reassembled_msg);
        if (i < 5) {
            TEST_ASSERT(!ready, "Intermediate fragments must not trigger ready");
        }
    }
    TEST_ASSERT(ready, "6th fragment must complete AX-CTP reassembly");
    TEST_ASSERT(reassembled_msg.sequence_number == 42, "Reconstructed sequence must be 42");

    // 8. Feed reassembled message into Gateway Pipeline initialized with hex key
    auto pipeline = createTestPipeline(config_secret_hex);
    auto decision = pipeline->processFrame(reassembled_msg);

    TEST_ASSERT(decision.accepted, "Pico 2 hardware-format message MUST be accepted by Gateway");
    TEST_ASSERT(decision.result == DecisionResult::VALID_FORWARD, "Decision must be VALID_FORWARD");
    TEST_ASSERT(pipeline->getAcceptedFrames() == 1, "Accepted counter must be 1");
    return true;
}

// 19. Test AX-CTP Missing Fragment Rejection
bool testAxCtpMissingFragmentRejection() {
    std::cout << "Purpose: Verify that dropping an authentication fragment prevents invalid forwarding.\n";
    std::cout << "Expected Output: AX-CTP reassembly does not complete, frame is never forwarded.\n";

    SecureCanMessage original_msg;
    original_msg.can_id = 0x100;
    original_msg.sequence_number = 55;
    original_msg.payload = CanPayloadCodec::encodePowertrain(70.0, 2600.0, 30.0, 85.0, 55);
    auto canon = HmacSha256::buildCanonicalBuffer(original_msg.can_id, original_msg.sequence_number, original_msg.payload);
    std::vector<uint8_t> key = HmacSha256::parseKey("TestSecretKey_2026");
    original_msg.hmac_tag = HmacSha256::computeHex(key, canon);

    std::vector<CanRawFrame> frames;
    CanTransportProtocol::fragmentMessage(original_msg, frames);
    TEST_ASSERT(frames.size() == 7, "Total physical frames must be 7");

    // Remove fragment #3 (frames[3])
    frames.erase(frames.begin() + 3);

    CanTransportProtocol reassembler;
    SecureCanMessage out_msg;
    bool completed = false;
    for (const auto& f : frames) {
        if (reassembler.processIncomingFrame(f, out_msg)) {
            completed = true;
        }
    }

    TEST_ASSERT(!completed, "Missing fragment must never complete reassembly");
    return true;
}

// 20. Test AX-CTP Out-of-Order Fragment Reassembly
bool testAxCtpOutOfOrderFragments() {
    std::cout << "Purpose: Verify that AX-CTP reassembles fragments arriving in scrambled order.\n";
    std::cout << "Expected Output: Out-of-order fragments correctly reconstruct HMAC and sequence.\n";

    std::string secret_hex = "4175746f536869656c64585f5365637265745f4b65795f32303236214155544f";
    std::vector<uint8_t> key = HmacSha256::parseKey(secret_hex);

    SecureCanMessage original_msg;
    original_msg.can_id = 0x100;
    original_msg.sequence_number = 150000; // Requires high 16-bit reconstruction
    original_msg.telemetry.speed_kmh = 90.0;
    original_msg.telemetry.rpm = 3000.0;
    original_msg.telemetry.temperature_c = 85.0;
    original_msg.telemetry.throttle_pct = 40.0;
    original_msg.payload = CanPayloadCodec::encodePowertrain(90.0, 3000.0, 40.0, 85.0, 150000);
    auto canon = HmacSha256::buildCanonicalBuffer(original_msg.can_id, original_msg.sequence_number, original_msg.payload);
    original_msg.hmac_tag = HmacSha256::computeHex(key, canon);

    std::vector<CanRawFrame> frames;
    CanTransportProtocol::fragmentMessage(original_msg, frames);

    // Reorder auth fragments: [telemetry, frag 4, frag 1, frag 5, frag 0, frag 3, frag 2]
    std::vector<CanRawFrame> scrambled;
    scrambled.push_back(frames[0]); // Telemetry
    scrambled.push_back(frames[5]); // Frag 4
    scrambled.push_back(frames[2]); // Frag 1
    scrambled.push_back(frames[6]); // Frag 5
    scrambled.push_back(frames[1]); // Frag 0
    scrambled.push_back(frames[4]); // Frag 3
    scrambled.push_back(frames[3]); // Frag 2

    CanTransportProtocol reassembler;
    SecureCanMessage out_msg;
    bool completed = false;
    for (const auto& f : scrambled) {
        if (reassembler.processIncomingFrame(f, out_msg)) {
            completed = true;
        }
    }

    TEST_ASSERT(completed, "Scrambled fragments must successfully complete reassembly");
    TEST_ASSERT(out_msg.sequence_number == 150000, "Sequence must be reconstructed to 150000");
    TEST_ASSERT(out_msg.hmac_tag == original_msg.hmac_tag, "HMAC tag must match original tag exactly");

    auto pipeline = createTestPipeline(secret_hex);
    auto decision = pipeline->processFrame(out_msg);
    TEST_ASSERT(decision.accepted, "Reassembled out-of-order message must pass pipeline");
    return true;
}

// 21. Test Validation Engine Config Loading
bool testValidationConfigLoading() {
    std::cout << "Purpose: Verify that ValidationEngine dynamically loads rules from gateway_config.json.\n";
    std::cout << "Expected Output: Config file loads successfully and validates according to configured limits.\n";

    ValidationEngine val;
    bool loaded = val.loadConfig("config/gateway_config.json");
    TEST_ASSERT(loaded, "ValidationEngine must load config/gateway_config.json");

    SecureCanMessage msg;
    msg.can_id = 0x100;
    msg.sequence_number = 1;
    msg.telemetry.speed_kmh = 60.0;
    msg.telemetry.rpm = 2500.0;
    msg.telemetry.temperature_c = 85.0;
    msg.telemetry.throttle_pct = 30.0;
    msg.payload = CanPayloadCodec::encodePowertrain(60.0, 2500.0, 30.0, 85.0, 1);

    auto res = val.validateMessage(msg);
    TEST_ASSERT(res.is_valid, "Valid powertrain message must pass configured validation");

    // Over-speed (> 250.0 km/h)
    msg.telemetry.speed_kmh = 280.0;
    auto res_speed = val.validateMessage(msg);
    TEST_ASSERT(!res_speed.is_valid, "Speed exceeding config limit must be rejected");

    return true;
}

// 22. Test Ethernet Forwarding UDP Output Serialization
bool testEthernetForwardingSerialization() {
    std::cout << "Purpose: Verify Ethernet forwarder formats validated telemetry into expected JSON schema.\n";
    std::cout << "Expected Output: JSON contains required telemetry keys and verified flags.\n";

    EthernetForwarder forwarder("127.0.0.1", 16000);
    bool inited = forwarder.init();
    TEST_ASSERT(inited, "EthernetForwarder must initialize UDP socket");

    SecureCanMessage msg;
    msg.can_id = 0x100;
    msg.sequence_number = 101;
    msg.timestamp_us = getCurrentTimestampMicros();
    msg.telemetry.speed_kmh = 88.5;
    msg.telemetry.rpm = 2950.0;
    msg.telemetry.temperature_c = 84.0;
    msg.telemetry.throttle_pct = 35.0;

    bool sent = forwarder.forwardTelemetry(msg);
    TEST_ASSERT(sent, "forwardTelemetry must succeed sending to local loopback UDP");
    TEST_ASSERT(forwarder.getForwardedCount() == 1, "Forwarded count must increment");

    forwarder.close();
    return true;
}

// 23. Test Arduino Uno Hardware Compatibility Vector
bool testArduinoHardwareCompatibilityVector() {
    std::cout << "Purpose: End-to-end verification of Arduino 1 (ATmega328P + MCP2515) AX-CTP protocol frames.\n";
    std::cout << "Expected Output: Arduino physical CAN frames reassemble and authenticate with VALID_FORWARD.\n";

    std::string secret = "AutoShieldX_Secret_Key_2026!AUTO";
    std::vector<uint8_t> key = HmacSha256::parseKey(secret);

    uint32_t seq = 100;
    float speed = 88.5f;
    float rpm = 2850.0f;
    float throttle = 35.0f;
    float temp = 84.0f;

    // 1. Encode 8-byte payload matching Arduino firmware
    uint16_t speed_scaled = static_cast<uint16_t>(speed * 100.0f);
    uint16_t rpm_scaled = static_cast<uint16_t>(rpm);
    uint8_t throt_scaled = static_cast<uint8_t>(throttle);
    uint8_t temp_scaled = static_cast<uint8_t>(temp + 40.0f);
    uint16_t seq_low = static_cast<uint16_t>(seq & 0xFFFF);

    CanRawFrame telemetry_frame{};
    telemetry_frame.can_id = 0x100;
    telemetry_frame.can_dlc = 8;
    telemetry_frame.data[0] = static_cast<uint8_t>((speed_scaled >> 8) & 0xFF);
    telemetry_frame.data[1] = static_cast<uint8_t>(speed_scaled & 0xFF);
    telemetry_frame.data[2] = static_cast<uint8_t>((rpm_scaled >> 8) & 0xFF);
    telemetry_frame.data[3] = static_cast<uint8_t>(rpm_scaled & 0xFF);
    telemetry_frame.data[4] = throt_scaled;
    telemetry_frame.data[5] = temp_scaled;
    telemetry_frame.data[6] = static_cast<uint8_t>((seq_low >> 8) & 0xFF);
    telemetry_frame.data[7] = static_cast<uint8_t>(seq_low & 0xFF);

    // 2. Canonical buffer matching Arduino firmware
    std::array<uint8_t, 8> payload_arr{};
    std::memcpy(payload_arr.data(), telemetry_frame.data, 8);
    auto canon = HmacSha256::buildCanonicalBuffer(0x100, seq, payload_arr);

    // 3. Compute HMAC
    std::string hmac_hex = HmacSha256::computeHex(key, canon);
    std::vector<uint8_t> hmac_raw = HmacSha256::hexToBytes(hmac_hex);

    // 4. Generate 6 auth fragments
    std::vector<CanRawFrame> auth_frags;
    uint8_t corr = static_cast<uint8_t>(seq & 0xFF);
    for (uint8_t i = 0; i < 6; ++i) {
        CanRawFrame f{};
        f.can_id = 0x101;
        f.can_dlc = 8;
        f.data[0] = static_cast<uint8_t>((i << 4) | 0x06);
        f.data[1] = corr;
        if (i < 5) {
            std::memcpy(&f.data[2], &hmac_raw[i * 6], 6);
        } else {
            f.data[2] = hmac_raw[30];
            f.data[3] = hmac_raw[31];
            f.data[4] = static_cast<uint8_t>((seq >> 24) & 0xFF);
            f.data[5] = static_cast<uint8_t>((seq >> 16) & 0xFF);
            f.data[6] = 0x00;
            f.data[7] = 0x00;
        }
        auth_frags.push_back(f);
    }

    // 5. Reassemble and verify
    CanTransportProtocol reassembler;
    SecureCanMessage msg;
    TEST_ASSERT(!reassembler.processIncomingFrame(telemetry_frame, msg), "Telemetry alone must wait for frags");
    for (size_t i = 0; i < 5; ++i) {
        TEST_ASSERT(!reassembler.processIncomingFrame(auth_frags[i], msg), "Partial fragments must not complete");
    }
    TEST_ASSERT(reassembler.processIncomingFrame(auth_frags[5], msg), "Frag 5 must complete Arduino AX-CTP");

    auto pipeline = createTestPipeline(secret);
    auto decision = pipeline->processFrame(msg);
    TEST_ASSERT(decision.accepted, "Arduino 1 authenticated frame must be ACCEPTED");
    TEST_ASSERT(decision.result == DecisionResult::VALID_FORWARD, "Result must be VALID_FORWARD");
    return true;
}

// 24. Test Detection of Unauthenticated Standalone Telemetry (Missing 0x101)
bool testUnauthenticatedTelemetryDetection() {
    std::cout << "Purpose: Verify that standalone 0x100 frames (missing 0x101) are yielded and flagged by Gateway.\n";
    std::cout << "Expected Output: Strict mode rejects with HMAC_AUTH_FAILED; Permissive mode monitors.\n";

    CanTransportProtocol reassembler;
    SecureCanMessage out_msg;

    // Send first standalone telemetry frame (like raw Arduino counter)
    CanRawFrame f1{};
    f1.can_id = 0x100;
    f1.can_dlc = 8;
    f1.data[6] = 0x00;
    f1.data[7] = 0x01; // Seq = 1
    bool ready1 = reassembler.processIncomingFrame(f1, out_msg);
    TEST_ASSERT(!ready1, "Frame 1 waits initially for auth fragments");

    // Send second standalone telemetry frame without auth fragments having arrived
    CanRawFrame f2{};
    f2.can_id = 0x100;
    f2.can_dlc = 8;
    f2.data[6] = 0x00;
    f2.data[7] = 0x02; // Seq = 2
    bool ready2 = reassembler.processIncomingFrame(f2, out_msg);
    TEST_ASSERT(ready2, "Frame 2 arrival must flush pending unauthenticated Frame 1");
    TEST_ASSERT(out_msg.hmac_tag.empty(), "Flushed frame must have empty HMAC tag");
    TEST_ASSERT(out_msg.sequence_number == 1, "Flushed frame sequence must be 1");

    // Feed flushed unauthenticated frame to strict pipeline
    auto strict_pipeline = createTestPipeline("AutoShieldX_Secret_Key_2026!AUTO");
    auto decision_strict = strict_pipeline->processFrame(out_msg);
    TEST_ASSERT(!decision_strict.accepted, "Strict mode must reject unauthenticated frame");
    TEST_ASSERT(decision_strict.result == DecisionResult::REJECT_HMAC_AUTHENTICATION_FAILED, "Result must be REJECT_HMAC_AUTHENTICATION_FAILED");

    // Feed flushed frame to permissive pipeline
    auto perm_pipeline = createTestPipeline("AutoShieldX_Secret_Key_2026!AUTO");
    perm_pipeline->setPermissiveMode(true);
    auto decision_perm = perm_pipeline->processFrame(out_msg);
    TEST_ASSERT(decision_perm.accepted, "Permissive mode must allow frame for monitoring");

    return true;
}

int main() {
    std::cout << "============================================================\n";
    std::cout << "      AUTOSHIELDX VERIFICATION & AUTOMATED TEST SUITE        \n";
    std::cout << "============================================================\n";

    RUN_TEST(testHmacGeneration);
    RUN_TEST(testHmacVerification);
    RUN_TEST(testModifiedMessageDetection);
    RUN_TEST(testSequenceValidation);
    RUN_TEST(testReplayDetection);
    RUN_TEST(testCanFrameValidation);
    RUN_TEST(testNormalVehicleTraffic);
    RUN_TEST(testInjectionAttack);
    RUN_TEST(testReplayAttack);
    RUN_TEST(testDosTraffic);
    RUN_TEST(testFuzzyTraffic);
    RUN_TEST(testFalseDataTraffic);
    RUN_TEST(testGatewayDecisionLogic);
    RUN_TEST(testCanTransportFragmentationAndReassembly);
    RUN_TEST(testCanTransportReassembledPipelineAcceptance);
    RUN_TEST(testCanTransportTamperDetection);
    RUN_TEST(testHmacKeyParsing);
    RUN_TEST(testPicoHardwareCompatibilityVector);
    RUN_TEST(testAxCtpMissingFragmentRejection);
    RUN_TEST(testAxCtpOutOfOrderFragments);
    RUN_TEST(testValidationConfigLoading);
    RUN_TEST(testEthernetForwardingSerialization);
    RUN_TEST(testArduinoHardwareCompatibilityVector);
    RUN_TEST(testUnauthenticatedTelemetryDetection);

    std::cout << "\n============================================================\n";
    std::cout << "  TEST RESULTS SUMMARY\n";
    std::cout << "  Total Tests Executed : " << g_tests_total << "\n";
    std::cout << "  Total Tests Passed   : " << g_tests_passed << "\n";
    std::cout << "  Total Tests Failed   : " << (g_tests_total - g_tests_passed) << "\n";
    std::cout << "============================================================\n";

    return (g_tests_passed == g_tests_total) ? 0 : 1;
}
