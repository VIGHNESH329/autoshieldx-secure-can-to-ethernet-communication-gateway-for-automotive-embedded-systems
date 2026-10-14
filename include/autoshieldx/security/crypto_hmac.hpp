#pragma once

/**
 * ============================================================================
 * AUTOSHIELDX CRYPTOGRAPHIC AUTHENTICATION ENGINE
 * Algorithm: HMAC-SHA256 (Keyed-Hash Message Authentication Code / RFC 2104)
 * ============================================================================
 *
 * CRITICAL CYBERSECURITY PROPERTY NOTICE:
 * ----------------------------------------------------------------------------
 * 1. AUTHENTICATION:
 *    HMAC-SHA256 guarantees that the message originated from a trusted entity
 *    possessing the authentic Pre-Shared Key (PSK). Unauthorized ECUs or
 *    injected spoofing nodes without the key cannot forge valid tags.
 *
 * 2. INTEGRITY:
 *    HMAC-SHA256 guarantees that the CAN ID, sequence number, and payload have
 *    NOT been tampered with or modified in transit. A 1-bit alteration results
 *    in an avalanche effect and tag verification failure.
 *
 * 3. NO CONFIDENTIALITY (PLAINTEXT PAYLOAD):
 *    HMAC does NOT provide confidentiality or encryption. The CAN telemetry
 *    payload remains readable in plaintext. In automotive control networks
 *    (AUTOSAR SecOC / ISO 21434), confidentiality is often deliberately omitted
 *    for safety-critical engine/brake signals to avoid decryption latency,
 *    key escrow risks in emergency limp-home modes, and to allow authorized
 *    read-only workshop diagnostic tap tools.
 * ============================================================================
 */

#include <string>
#include <vector>
#include <cstdint>
#include <array>

namespace autoshieldx {

class HmacSha256 {
public:
    /**
     * @brief Compute full 32-byte HMAC-SHA256 over input buffer.
     * @param key Pre-shared secret key bytes
     * @param data Input data buffer
     * @return 64-character lowercase hex string
     */
    static std::string computeHex(
        const std::vector<uint8_t>& key,
        const std::vector<uint8_t>& data);

    static std::string computeHex(
        const std::string& key_str,
        const std::string& data_str);

    /**
     * @brief Compute raw 32-byte HMAC-SHA256.
     */
    static std::vector<uint8_t> computeRaw(
        const std::vector<uint8_t>& key,
        const std::vector<uint8_t>& data);

    /**
     * @brief Compute truncated HMAC (e.g., 8 bytes / 64 bits) for CAN payload constraints.
     */
    static std::string computeTruncatedHex(
        const std::vector<uint8_t>& key,
        const std::vector<uint8_t>& data,
        size_t truncate_bytes = 8);

    /**
     * @brief Constant-time verification of HMAC tags to prevent timing side-channel attacks.
     * @param expected Computed authentic tag
     * @param received Tag received in message
     * @return true if authentic, false if tampered
     */
    static bool verifyConstantTime(
        const std::string& expected,
        const std::string& received);

    /**
     * @brief Construct the canonical authentication payload for a CAN message:
     * Canonical Format: [CAN_ID (4B BE)] | [Sequence (4B BE)] | [DLC (1B)] | [Payload (8B)]
     */
    static std::vector<uint8_t> buildCanonicalBuffer(
        uint32_t can_id,
        uint32_t sequence_number,
        const std::array<uint8_t, 8>& payload);

    // Hex string conversion utilities
    static std::string bytesToHex(const uint8_t* data, size_t len);
    static std::vector<uint8_t> hexToBytes(const std::string& hex);

    /**
     * @brief Parse key string into raw bytes.
     * If the input is a valid even-length hex string (e.g. 64 chars for 32 bytes),
     * it is decoded from hex. Otherwise, raw ASCII characters are used.
     */
    static std::vector<uint8_t> parseKey(const std::string& key_str);
};

} // namespace autoshieldx
