/* The test exercises @extern("c_symbol") name(...) — the Aether-side name
 * lives in the module namespace, but the C symbol the linker resolves is the
 * one named in the annotation. Here probe_v2 is the only symbol exposed; if
 * the annotation is dropped (or the call site doesn't translate the
 * Aether-side name to the C symbol), the link will fail with "undefined
 * reference to ae_probe_compute". */

int probe_v2(int n) { return n + 41; }

int probe_const(void) { return 7; }
