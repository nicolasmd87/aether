#include "aether_cryptography.h"
#include "../string/aether_string.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef AETHER_HAS_OPENSSL
#include <openssl/evp.h>
#endif

/* Unwrap the payload from a `data` argument that may be either an
 * AetherString* or a plain char*. Mirrors the helper in
 * std/fs/aether_fs.c — when callers pass a length-aware AetherString
 * (e.g. from fs.read_binary), the raw pointer is the struct, not
 * the bytes. Without this dispatch, we'd hash the struct header. */
static inline const unsigned char* cryptography_unwrap_bytes(const char* data, int length, size_t* out_len) {
    if (!data) { *out_len = 0; return NULL; }
    if (is_aether_string(data)) {
        const AetherString* s = (const AetherString*)data;
        *out_len = (length >= 0) ? (size_t)length : s->length;
        return (const unsigned char*)s->data;
    }
    *out_len = (length >= 0) ? (size_t)length : strlen(data);
    return (const unsigned char*)data;
}

static char* hex_encode(const unsigned char* digest, size_t digest_len) {
    /* Two hex chars per byte + trailing NUL. */
    char* hex = (char*)malloc(digest_len * 2 + 1);
    if (!hex) return NULL;
    static const char HEX[] = "0123456789abcdef";
    for (size_t i = 0; i < digest_len; i++) {
        hex[i * 2]     = HEX[(digest[i] >> 4) & 0x0F];
        hex[i * 2 + 1] = HEX[digest[i] & 0x0F];
    }
    hex[digest_len * 2] = '\0';
    return hex;
}

#ifdef AETHER_HAS_OPENSSL
static char* sha_hex(const EVP_MD* md, const char* data, int length) {
    if (length < 0) return NULL;
    size_t want;
    const unsigned char* bytes = cryptography_unwrap_bytes(data, length, &want);
    if (want > 0 && !bytes) return NULL;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return NULL;

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;

    if (EVP_DigestInit_ex(ctx, md, NULL) != 1 ||
        (want > 0 && EVP_DigestUpdate(ctx, bytes, want) != 1) ||
        EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return NULL;
    }
    EVP_MD_CTX_free(ctx);

    return hex_encode(digest, (size_t)digest_len);
}

char* cryptography_sha1_hex_raw(const char* data, int length) {
    return sha_hex(EVP_sha1(), data, length);
}

char* cryptography_sha256_hex_raw(const char* data, int length) {
    return sha_hex(EVP_sha256(), data, length);
}

#else /* !AETHER_HAS_OPENSSL */

char* cryptography_sha1_hex_raw(const char* data, int length) {
    (void)data; (void)length; return NULL;
}
char* cryptography_sha256_hex_raw(const char* data, int length) {
    (void)data; (void)length; return NULL;
}

#endif /* AETHER_HAS_OPENSSL */
