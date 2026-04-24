/* Regression shim for new_defects.md section 1 — AetherString vs
 * const char* ABI.
 *
 * The shim models a downstream C consumer that reads a file via an
 * Aether-provided extern typed as `-> string`. If the shim naively
 * treats the returned pointer as `const char*` for memcpy/strlen, it
 * reads into the 24-byte struct header (AetherString's magic + ref
 * count + lengths + data pointer) and gets garbage.
 *
 * The fix on the C side is to use the public helpers
 *   aether_string_data(s)    — returns the byte pointer
 *   aether_string_length(s)  — returns the exact length (binary-safe)
 * which accept either an AetherString* (ref-counted string returned
 * from std.string, std.fs, etc.) or a raw char* (legacy TLS-arena
 * externs). This test drives that path end-to-end.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "aether_string.h"

/* Step 1: setup — write a 10-byte file with an embedded NUL at
 * offset 3. The Aether side reads it via fs.read_binary. */
int ffi_shim_write_binary(const char* path) {
    if (!path) return 0;
    unsigned char buf[10] = { 0x41, 0x42, 0x43, 0x00, 0x45, 0x46,
                              0x47, 0x48, 0x49, 0x4A };
    FILE* f = fopen(path, "wb");
    if (!f) return 0;
    size_t wrote = fwrite(buf, 1, sizeof(buf), f);
    fclose(f);
    return (wrote == sizeof(buf)) ? 1 : 0;
}

/* Step 2: consume an AetherString-typed return through the public
 * helpers. `s` is the result of an Aether expression whose declared
 * type is `string`. Copy its bytes into `out` (caller-provided) and
 * return the byte count actually written. Fills `out` with exactly
 * `cap` bytes of zero-padding if the source is too short.
 *
 * If `aether_string_data` / `aether_string_length` are working, this
 * returns 10 and `out` contains {41,42,43,00,45,46,47,48,49,4A}.
 * If the shim instead did memcpy(out, (const char*)s, strlen(s)) it
 * would read the struct header — the first four bytes would be the
 * magic (0xDE 0xC0 0x57 0xAE on little-endian) and the rest of the
 * buffer would be zero-count, capacity, ref count, etc.
 */
int ffi_shim_copy_bytes(const void* s, unsigned char* out, int cap) {
    if (!s || !out || cap <= 0) return 0;
    const char* data = aether_string_data(s);
    size_t len = aether_string_length(s);
    if ((int)len > cap) len = (size_t)cap;
    memcpy(out, data, len);
    for (size_t i = len; i < (size_t)cap; i++) out[i] = 0;
    return (int)len;
}

/* Step 3: byte-level spot check so the test can call this from Aether
 * (exposing `unsigned char[]` across the FFI boundary is awkward). */
int ffi_shim_check_bytes(const void* s) {
    unsigned char buf[16] = {0};
    int n = ffi_shim_copy_bytes(s, buf, sizeof(buf));
    if (n != 10) return -1;
    static const unsigned char expect[10] = {
        0x41, 0x42, 0x43, 0x00, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A
    };
    for (int i = 0; i < 10; i++) {
        if (buf[i] != expect[i]) {
            return 100 + i;  /* offset where mismatch happened */
        }
    }
    return 0;
}
