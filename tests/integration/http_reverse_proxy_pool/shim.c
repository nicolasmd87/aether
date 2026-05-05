/* Per-process counter + admin-state shim for the reverse-proxy
 * pool integration test. Each upstream binary maintains a small
 * set of process-local globals via these getters/setters; the
 * Aether-side handlers read/write them through @c_callback-shaped
 * accessors.
 *
 * Static visibility keeps each upstream's state isolated — the
 * test runner spawns three upstream processes plus one proxy,
 * each with its own counter. */

#include <stdatomic.h>
#include <string.h>
#include <stdlib.h>

static _Atomic int counter = 0;          /* /echo invocation count */
static _Atomic int force_503 = 0;        /* upstream_b admin flag */
static _Atomic int eta_ms = 0;           /* upstream_b /slow sleep */

int  proxy_test_counter_get(void)        { return atomic_load(&counter); }
int  proxy_test_counter_inc(void)        { return atomic_fetch_add(&counter, 1) + 1; }
void proxy_test_counter_reset(void)      { atomic_store(&counter, 0); }

int  proxy_test_force_503_get(void)      { return atomic_load(&force_503); }
void proxy_test_force_503_set(int v)     { atomic_store(&force_503, v); }

int  proxy_test_eta_ms_get(void)         { return atomic_load(&eta_ms); }
void proxy_test_eta_ms_set(int v)        { atomic_store(&eta_ms, v); }

/* String equality: returns 1 if both args point to the same
 * NUL-terminated bytes. The Aether-side `==` between strings
 * already works, but the @c_callback verifier signature receives
 * `const char*` (an opaque ptr in Aether), so wrapping strcmp
 * for that boundary is the cleanest path. */
int proxy_test_str_eq(const char* a, const char* b) {
    if (!a || !b) return 0;
    return strcmp(a, b) == 0 ? 1 : 0;
}

/* atoi wrapper (libc has it; Aether stdlib doesn't expose it
 * directly). */
int proxy_test_atoi(const char* s) {
    if (!s) return 0;
    return atoi(s);
}
