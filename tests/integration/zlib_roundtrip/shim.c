/* Shim for test_zlib_roundtrip. Seeds a 32-byte binary file with
 * NUL bytes scattered throughout so the probe can fs.read_binary it
 * into a length-aware AetherString, compress via zlib.deflate,
 * decompress via zlib.inflate, and verify every byte round-trips.
 * The NUL bytes exercise the AetherString-unwrap path inside
 * aether_zlib.c — without it the compressed stream would contain
 * the struct header instead of the payload. */

#include <stdio.h>

int zlib_probe_write_binary(const char* path) {
    if (!path) return 0;
    /* 32 bytes, pattern chosen so that every fourth byte is NUL. */
    unsigned char buf[32];
    for (int i = 0; i < 32; i++) {
        buf[i] = (unsigned char)((i % 4 == 0) ? 0 : (i + 1));
    }
    FILE* f = fopen(path, "wb");
    if (!f) return 0;
    size_t wrote = fwrite(buf, 1, sizeof(buf), f);
    fclose(f);
    return (wrote == sizeof(buf)) ? 1 : 0;
}
