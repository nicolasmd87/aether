/* Shim for test_cryptography_sha. Writes a 10-byte binary file with
 * NUL at offset 3 so the Aether probe can fs.read_binary it into a
 * length-aware AetherString and feed it to cryptography.sha256_hex /
 * cryptography.sha1_hex. This exercises the AetherString-unwrap path
 * inside aether_cryptography.c (without the unwrap, the hash would
 * include the struct header, producing garbage). */

#include <stdio.h>

int cryptography_probe_write_binary(const char* path) {
    if (!path) return 0;
    unsigned char buf[10] = { 0x01, 0x02, 0x03, 0x00, 0x05, 0x06,
                              0x07, 0x08, 0x09, 0x0A };
    FILE* f = fopen(path, "wb");
    if (!f) return 0;
    size_t wrote = fwrite(buf, 1, sizeof(buf), f);
    fclose(f);
    return (wrote == sizeof(buf)) ? 1 : 0;
}
