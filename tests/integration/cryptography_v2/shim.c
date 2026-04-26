/* Shim for test_cryptography_v2. Returns a 10-byte binary buffer
 * (NUL at offset 3) as a plain const char* the Aether probe feeds
 * into base64_encode + base64_decode round-trips. The test verifies
 * round-trip integrity by hashing both halves and comparing the
 * digests — a length-blind decoder would lose bytes after the NUL
 * and the digests would diverge. */

#include <stdint.h>

static const unsigned char K_BIN[10] = {
    0x01, 0x02, 0x03, 0x00, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A
};

const char* cryptography_v2_probe_make_binary(void) {
    return (const char*)K_BIN;
}
