/* Probe shim — reads the first 4 bytes of `content` as a
 * little-endian uint32. Naive C consumer: declared as
 * `const char*`, does no AetherString magic-byte dispatch.
 *
 * If the auto-unwrap fired at the call site, `content` points at
 * the payload bytes ("AB\0\0..." for a 2-byte AetherString) and
 * v == 0x4241.
 *
 * If the auto-unwrap was skipped, `content` points at the
 * AetherString struct header. The first 4 bytes are the magic
 * (0xAE57C0DE little-endian = 0xDE 0xC0 0x57 0xAE) and v ==
 * 0xAE57C0DE = 2925242078 (low 16 bits = 0xC0DE = 49374).
 *
 * Either way the function returns; the caller compares the value
 * against the expected payload result. */
int ffi_check_first4(const char* content) {
    unsigned int v = 0;
    v |= (unsigned char)content[0];
    v |= ((unsigned int)(unsigned char)content[1]) << 8;
    v |= ((unsigned int)(unsigned char)content[2]) << 16;
    v |= ((unsigned int)(unsigned char)content[3]) << 24;
    return (int)v;
}
