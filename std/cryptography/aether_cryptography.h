/* std.cryptography — cryptographic hash primitives.
 *
 * v1 exposes SHA-1 and SHA-256 as pure one-shot functions:
 *   bytes in, lowercase-hex digest out, no streaming API.
 * HMAC, key derivation, symmetric ciphers, and streaming digests are
 * deliberately out of scope for v1 — see docs/stdlib-vs-contrib.md
 * for the "one obvious shape" criterion.
 *
 * When the build has AETHER_HAS_OPENSSL (the default on every
 * platform where OpenSSL is available), the implementation is a
 * thin veneer over libcrypto's SHA-1 / SHA-256 routines. Without
 * OpenSSL, the wrappers return NULL so the Go-style Aether wrappers
 * in module.ae report "openssl unavailable" cleanly.
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

#endif /* AETHER_CRYPTOGRAPHY_H */
