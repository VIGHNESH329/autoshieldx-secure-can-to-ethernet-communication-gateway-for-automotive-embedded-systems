#include "autoshieldx/security/crypto_hmac.hpp"
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>

#if defined(__has_include)
  #if __has_include(<openssl/hmac.h>) && __has_include(<openssl/evp.h>)
    #define AUTOSHIELDX_HAS_OPENSSL 1
    #include <openssl/hmac.h>
    #include <openssl/evp.h>
  #endif
#endif

namespace autoshieldx {

namespace detail {

// Portable, self-contained standard SHA-256 implementation (FIPS PUB 180-4)
// Used directly or as deterministic fallback
class PortableSha256 {
public:
    PortableSha256() { reset(); }

    void reset() {
        state_[0] = 0x6a09e667;
        state_[1] = 0xbb67ae85;
        state_[2] = 0x3c6ef372;
        state_[3] = 0xa54ff53a;
        state_[4] = 0x510e527f;
        state_[5] = 0x9b05688c;
        state_[6] = 0x1f83d9ab;
        state_[7] = 0x5be0cd19;
        count_ = 0;
        buffer_len_ = 0;
    }

    void update(const uint8_t* data, size_t len) {
        for (size_t i = 0; i < len; ++i) {
            buffer_[buffer_len_++] = data[i];
            if (buffer_len_ == 64) {
                transform(buffer_);
                count_ += 512;
                buffer_len_ = 0;
            }
        }
    }

    std::array<uint8_t, 32> finalize() {
        count_ += buffer_len_ * 8;
        buffer_[buffer_len_++] = 0x80;
        if (buffer_len_ > 56) {
            while (buffer_len_ < 64) buffer_[buffer_len_++] = 0;
            transform(buffer_);
            buffer_len_ = 0;
        }
        while (buffer_len_ < 56) buffer_[buffer_len_++] = 0;
        for (int i = 7; i >= 0; --i) {
            buffer_[buffer_len_++] = static_cast<uint8_t>((count_ >> (i * 8)) & 0xFF);
        }
        transform(buffer_);

        std::array<uint8_t, 32> digest;
        for (size_t i = 0; i < 8; ++i) {
            digest[i * 4 + 0] = static_cast<uint8_t>((state_[i] >> 24) & 0xFF);
            digest[i * 4 + 1] = static_cast<uint8_t>((state_[i] >> 16) & 0xFF);
            digest[i * 4 + 2] = static_cast<uint8_t>((state_[i] >> 8) & 0xFF);
            digest[i * 4 + 3] = static_cast<uint8_t>(state_[i] & 0xFF);
        }
        return digest;
    }

private:
    static inline uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
    static inline uint32_t choose(uint32_t e, uint32_t f, uint32_t g) { return (e & f) ^ (~e & g); }
    static inline uint32_t majority(uint32_t a, uint32_t b, uint32_t c) { return (a & b) ^ (a & c) ^ (b & c); }
    static inline uint32_t sig0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
    static inline uint32_t sig1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
    static inline uint32_t theta0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
    static inline uint32_t theta1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

    void transform(const uint8_t* chunk) {
        static const uint32_t K[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
        };

        uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = (chunk[i * 4 + 0] << 24) | (chunk[i * 4 + 1] << 16) |
                   (chunk[i * 4 + 2] << 8)  | (chunk[i * 4 + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            w[i] = theta1(w[i - 2]) + w[i - 7] + theta0(w[i - 15]) + w[i - 16];
        }

        uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
        uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];

        for (int i = 0; i < 64; ++i) {
            uint32_t t1 = h + sig1(e) + choose(e, f, g) + K[i] + w[i];
            uint32_t t2 = sig0(a) + majority(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }

        state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
        state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
    }

    uint32_t state_[8];
    uint64_t count_;
    uint8_t buffer_[64];
    size_t buffer_len_;
};

// RFC 2104 Portable HMAC-SHA256
std::vector<uint8_t> portableHmacSha256(const std::vector<uint8_t>& key, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> k = key;
    if (k.size() > 64) {
        PortableSha256 sha;
        sha.update(k.data(), k.size());
        auto d = sha.finalize();
        k.assign(d.begin(), d.end());
    }
    if (k.size() < 64) {
        k.resize(64, 0x00);
    }

    std::vector<uint8_t> k_ipad(64);
    std::vector<uint8_t> k_opad(64);
    for (size_t i = 0; i < 64; ++i) {
        k_ipad[i] = k[i] ^ 0x36;
        k_opad[i] = k[i] ^ 0x5c;
    }

    // Inner hash: H(K_ipad || data)
    PortableSha256 inner_sha;
    inner_sha.update(k_ipad.data(), k_ipad.size());
    inner_sha.update(data.data(), data.size());
    auto inner_digest = inner_sha.finalize();

    // Outer hash: H(K_opad || inner_digest)
    PortableSha256 outer_sha;
    outer_sha.update(k_opad.data(), k_opad.size());
    outer_sha.update(inner_digest.data(), inner_digest.size());
    auto final_digest = outer_sha.finalize();

    return std::vector<uint8_t>(final_digest.begin(), final_digest.end());
}

} // namespace detail

std::vector<uint8_t> HmacSha256::computeRaw(
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& data)
{
#if defined(AUTOSHIELDX_HAS_OPENSSL)
    unsigned int md_len = 32;
    std::vector<uint8_t> result(32);
    unsigned char* res = HMAC(
        EVP_sha256(),
        key.data(), static_cast<int>(key.size()),
        data.data(), data.size(),
        result.data(), &md_len);
    if (res != nullptr) {
        result.resize(md_len);
        return result;
    }
#endif
    // Fallback to built-in exact RFC 2104 implementation
    return detail::portableHmacSha256(key, data);
}

std::string HmacSha256::computeHex(
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& data)
{
    auto raw = computeRaw(key, data);
    return bytesToHex(raw.data(), raw.size());
}

std::string HmacSha256::computeHex(
    const std::string& key_str,
    const std::string& data_str)
{
    std::vector<uint8_t> key(key_str.begin(), key_str.end());
    std::vector<uint8_t> data(data_str.begin(), data_str.end());
    return computeHex(key, data);
}

std::string HmacSha256::computeTruncatedHex(
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& data,
    size_t truncate_bytes)
{
    auto raw = computeRaw(key, data);
    size_t len = std::min(truncate_bytes, raw.size());
    return bytesToHex(raw.data(), len);
}

bool HmacSha256::verifyConstantTime(
    const std::string& expected,
    const std::string& received)
{
    if (expected.size() != received.size()) {
        return false;
    }

    // Constant-time comparison to prevent side-channel timing attacks
    volatile unsigned char diff = 0;
    for (size_t i = 0; i < expected.size(); ++i) {
        diff |= static_cast<unsigned char>(expected[i] ^ received[i]);
    }
    return diff == 0;
}

std::vector<uint8_t> HmacSha256::buildCanonicalBuffer(
    uint32_t can_id,
    uint32_t sequence_number,
    const std::array<uint8_t, 8>& payload)
{
    std::vector<uint8_t> buf(4 + 4 + 1 + 8);

    // Big-endian CAN ID
    buf[0] = static_cast<uint8_t>((can_id >> 24) & 0xFF);
    buf[1] = static_cast<uint8_t>((can_id >> 16) & 0xFF);
    buf[2] = static_cast<uint8_t>((can_id >> 8) & 0xFF);
    buf[3] = static_cast<uint8_t>(can_id & 0xFF);

    // Big-endian Sequence Counter
    buf[4] = static_cast<uint8_t>((sequence_number >> 24) & 0xFF);
    buf[5] = static_cast<uint8_t>((sequence_number >> 16) & 0xFF);
    buf[6] = static_cast<uint8_t>((sequence_number >> 8) & 0xFF);
    buf[7] = static_cast<uint8_t>(sequence_number & 0xFF);

    // DLC (Payload Length = 8)
    buf[8] = 8;

    // 8-byte Payload
    std::memcpy(&buf[9], payload.data(), 8);

    return buf;
}

std::string HmacSha256::bytesToHex(const uint8_t* data, size_t len) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        oss << std::setw(2) << static_cast<int>(data[i]);
    }
    return oss.str();
}

std::vector<uint8_t> HmacSha256::hexToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i + 1 < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(strtol(byteString.c_str(), nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

std::vector<uint8_t> HmacSha256::parseKey(const std::string& key_str) {
    if (key_str.length() >= 2 && key_str.length() % 2 == 0) {
        bool all_hex = true;
        for (char c : key_str) {
            if (!std::isxdigit(static_cast<unsigned char>(c))) {
                all_hex = false;
                break;
            }
        }
        // If it's a valid hex string of typical key lengths (e.g. 64 chars = 32 bytes, 32 chars = 16 bytes)
        if (all_hex && (key_str.length() == 64 || key_str.length() == 32 || key_str.length() == 40 || key_str.length() == 128)) {
            return hexToBytes(key_str);
        }
    }
    return std::vector<uint8_t>(key_str.begin(), key_str.end());
}

} // namespace autoshieldx
