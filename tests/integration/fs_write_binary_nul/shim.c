/* Shim for test_fs_write_binary_nul.
 *
 * Writes a 10-byte binary file (with NUL at offset 3) for the probe
 * to pick up via fs.read_binary — which returns a length-aware
 * AetherString. The probe then passes that AetherString into
 * fs.write_binary to verify the write path also preserves NULs.
 * This avoids trying to return a ref-counted AetherString from C. */

#include <stdio.h>

int wb_probe_write_seed(const char* path) {
    if (!path) return 0;
    unsigned char buf[10] = { 0x01, 0x02, 0x03, 0x00, 0x05, 0x06,
                              0x07, 0x08, 0x09, 0x0A };
    FILE* f = fopen(path, "wb");
    if (!f) return 0;
    size_t wrote = fwrite(buf, 1, sizeof(buf), f);
    fclose(f);
    return (wrote == sizeof(buf)) ? 1 : 0;
}
