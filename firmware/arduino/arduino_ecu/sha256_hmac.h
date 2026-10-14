#ifndef SHA256_HMAC_H
#define SHA256_HMAC_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t state[8];
    uint64_t count;
    uint8_t  buffer[64];
    size_t   buffer_len;
} sha256_ctx_t;

void sha256_init(sha256_ctx_t *ctx);
void sha256_update(sha256_ctx_t *ctx, const uint8_t *data, size_t len);
void sha256_final(sha256_ctx_t *ctx, uint8_t digest[32]);

/**
 * @brief Computes standard RFC 2104 HMAC-SHA256 across message buffer.
 *
 * @param key Pointer to shared secret key bytes
 * @param key_len Length of key in bytes
 * @param data Pointer to input data payload (canonical buffer)
 * @param data_len Length of input data in bytes
 * @param digest32 Output buffer (must be at least 32 bytes)
 */
void hmac_sha256(
    const uint8_t *key, size_t key_len,
    const uint8_t *data, size_t data_len,
    uint8_t digest32[32]);

#ifdef __cplusplus
}
#endif

#endif // SHA256_HMAC_H
