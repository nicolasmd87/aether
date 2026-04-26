/* Shim for the @c_callback regression test (#235). Each function
 * takes a function pointer (typed `void*` at the Aether boundary)
 * and dispatches to it. The Aether side defines the callbacks with
 * @c_callback so they're addressable as C function pointers; if the
 * annotation is ignored, the link step fails because the cloned
 * functions would be `static` (post-import) or the call-site
 * identifier would resolve to an unrelated symbol. */

typedef int (*IntFn1)(int);
typedef int (*IntFn2)(int, int);

int call_int1(void* fn, int x) {
    return ((IntFn1)fn)(x);
}

int call_int2(void* fn, int a, int b) {
    return ((IntFn2)fn)(a, b);
}

/* Pointer-equality check: same Aether function passed twice should
 * yield the same C function pointer — confirms the symbol is stable
 * across both reference sites, not a per-site thunk. */
int same_pointer(void* a, void* b) {
    return a == b ? 1 : 0;
}
