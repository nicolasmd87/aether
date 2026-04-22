/* Shim that writes a 10-byte binary file with an embedded NUL at
 * offset 3. We can't easily construct a binary payload from Aether
 * today (string literals can't contain NULs, fs.write goes through
 * strlen-based helpers), so the setup lives here in C. The test
 * itself reads via fs.read_binary and inspects each byte through
 * string.char_at. */

#include <stdio.h>

int rb_probe_write_binary(const char* path) {
    if (!path) return 0;
    unsigned char buf[10] = { 0x01, 0x02, 0x03, 0x00, 0x05, 0x06,
                              0x07, 0x08, 0x09, 0x0A };
    FILE* f = fopen(path, "wb");
    if (!f) return 0;
    size_t wrote = fwrite(buf, 1, sizeof(buf), f);
    fclose(f);
    return (wrote == sizeof(buf)) ? 1 : 0;
}
