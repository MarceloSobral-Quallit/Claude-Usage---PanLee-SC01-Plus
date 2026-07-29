#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CRYPTO_TOKEN_MAX_LEN 256

typedef struct {
    uint8_t salt[6];     /* MAC efuse (esp_efuse_mac_get_default) */
    uint8_t iv[12];      /* GCM nonce aleatorio */
    uint8_t tag[16];     /* GCM auth tag */
    uint8_t ciphertext[CRYPTO_TOKEN_MAX_LEN];
    size_t len;
} EncryptedBlob;

void deriveKey(const char *pin, const uint8_t *salt, size_t saltLen, uint8_t *keyOut32);
bool encryptToken(const char *plaintext, const char *pin, EncryptedBlob *blob);
bool decryptToken(const EncryptedBlob *blob, const char *pin, char *plainOut, size_t plainMaxLen);

#ifdef __cplusplus
}
#endif
