/* std.cryptography — cryptographic hash primitives + Base64 codec.
 *
 * Surface (in module.ae's wrapper layer):
 *   sha1_hex(data, length)       — one-shot SHA-1 → lowercase hex
 *   sha256_hex(data, length)     — one-shot SHA-256 → lowercase hex
 *   hash_hex(algo, data, length) — algorithm-by-name dispatcher
 *   hash_supported(algo)         — config-time probe; 1/0 answer
 *   base64_encode(data, length)  — RFC 4648 §4 standard alphabet, unpadded
 *   base64_decode(b64)           — bytes + length via TLS split-accessor
 *
 * HMAC, key derivation, symmetric ciphers, signing, streaming digests,
 * URL-safe Base64 (RFC 4648 §5), and PKCS#7 / PEM parsing are
 * deliberately out of scope. See docs/stdlib-vs-contrib.md for the
 * "one obvious shape" criterion.
 *
 * When the build has AETHER_HAS_OPENSSL (the default on every
 * platform where OpenSSL is available), the implementation is a
 * thin veneer over libcrypto. Without OpenSSL, every entry point
 * returns NULL / 0 so the Go-style Aether wrappers report
 * "openssl unavailable" cleanly.
 */

#ifndef AETHER_CRYPTOGRAPHY_H
#define AETHER_CRYPTOGRAPHY_H

/* Return a newly-allocated, NUL-terminated lowercase-hex digest
 * (40 chars for SHA-1, 64 chars for SHA-256) or NULL on failure.
 * Caller owns the returned buffer and frees it with free().
 *
 * `data` may be an AetherString* or a plain char*; `length` is the
 * explicit byte count (binary-safe, embedded NULs OK). A `length`
 * of 0 hashes the empty string — SHA-256 of "" is a well-defined
 * constant. */
char* cryptography_sha1_hex_raw(const char* data, int length);
char* cryptography_sha256_hex_raw(const char* data, int length);

/* Generic algorithm-by-name digest. `algo` is one of "sha1", "sha256",
 * or any other name OpenSSL's EVP_get_digestbyname() recognizes
 * ("sha384", "sha512", "sha3-256", ...). Returns the same
 * lowercase-hex shape as the per-algorithm functions, or NULL on
 * unknown algorithm / digest failure / no OpenSSL. Caller frees. */
char* cryptography_hash_hex_raw(const char* algo, const char* data, int length);

/* Probe whether the OpenSSL backend in this build can compute `algo`.
 * Returns 1 yes, 0 no. Always succeeds (never errors); callers use
 * this at config time to validate user-supplied algorithm names
 * before they hit hash_hex_raw. Returns 0 when built without
 * OpenSSL. */
int cryptography_hash_supported(const char* algo);

/* Base64 encode `length` bytes of `data` using the RFC 4648 §4
 * standard alphabet, unpadded (callers needing `=` padding append
 * `=` themselves to make the length a multiple of 4). Returns a
 * newly-allocated NUL-terminated string the caller frees, or NULL
 * on OOM / no OpenSSL. */
char* cryptography_base64_encode_raw(const char* data, int length);

/* Decode a Base64 string. Returns 1 on success, 0 on failure
 * (malformed input / OOM / no OpenSSL). On success, the decoded
 * bytes live in a thread-local buffer accessible via
 * cryptography_get_base64_decode() and ..._length(); they remain
 * valid until the next call to cryptography_base64_decode_raw()
 * on the same thread. Same TLS split-accessor pattern as
 * std.fs.read_binary. */
int cryptography_base64_decode_raw(const char* b64);
const char* cryptography_get_base64_decode(void);
int cryptography_get_base64_decode_length(void);
void cryptography_release_base64_decode(void);

#endif /* AETHER_CRYPTOGRAPHY_H */
