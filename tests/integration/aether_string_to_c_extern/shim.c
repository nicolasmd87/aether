/* Probe for #297 — confirms the codegen unwraps an Aether-side
 * `string` value (which may be backed by an AetherString*) before
 * passing it to a naive C extern declared `const char*`.
 *
 * The function reads the first 4 bytes the C side sees. Pre-fix it
 * saw the AetherString magic header `0xAE57C0DE`; post-fix it sees
 * the actual payload bytes. */

#include <string.h>

int read_first4_as_int(const char* content) {
    /* Read 4 bytes as a little-endian uint32 so the test can
     * compare against a known constant. */
    unsigned int v = 0;
    v |= (unsigned char)content[0];
    v |= ((unsigned int)(unsigned char)content[1]) << 8;
    v |= ((unsigned int)(unsigned char)content[2]) << 16;
    v |= ((unsigned int)(unsigned char)content[3]) << 24;
    return (int)v;
}

/* The bug-pattern repro from the issue: memcpy the first `n` bytes
 * into a caller-provided buffer. Pre-fix this would copy the magic
 * header into the buffer; post-fix it copies the payload. */
int memcpy_into(char* dst, const char* src, int n) {
    memcpy(dst, src, (size_t)n);
    return n;
}
