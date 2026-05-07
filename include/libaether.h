/* libaether.h — public C API for embedders of `--emit=lib` Aether
 * code.
 *
 * Stable C interface that host programs (Java/Python/Go embedders,
 * plugin systems, language gateways) link against to drive
 * Aether-generated `.so` / `.a` artifacts.
 *
 * Today this surface exposes the per-call resource caps from issue
 * #343 (memory ceiling + wall-clock deadline). Future additions
 * land as additive symbols — never break the existing names.
 *
 * The Aether build emits codegen-level calls into this surface
 * automatically when `--emit=lib` is set. Embedders that drive
 * those caps from outside (typical plugin host) include this
 * header and link `libaether.a` (or the platform shared variant).
 */

#ifndef LIBAETHER_H
#define LIBAETHER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Symbol-visibility export. Builds with `-fvisibility=hidden`
 * still expose these as default-visibility public symbols. */
#if defined(__GNUC__) || defined(__clang__)
#  define AETHER_API __attribute__((visibility("default")))
#elif defined(_MSC_VER)
#  define AETHER_API __declspec(dllexport)
#else
#  define AETHER_API
#endif

/* Process-wide memory cap. After this call, every subsequent
 * cap-aware allocator (string, list, map, arena, pool) refuses
 * allocations that would push current usage past `bytes` and
 * returns NULL / its error-string convention. Pass `0` to
 * disable (unlimited).
 *
 * Already-accounted allocations are unaffected — only new
 * allocations are gated. Existing ones drain via their normal
 * free paths and decrement the counter.
 *
 * Thread-safe: relaxed atomic store. */
AETHER_API void aether_set_memory_cap(uint64_t bytes);

/* Per-thread wall-clock deadline. After this call, every
 * codegen-emitted tripwire (at every loop head in `--emit=lib`
 * builds) checks `now >= deadline_at`. On positive trip the
 * sticky flag flips, and `aether_deadline_tripped()` returns 1
 * for every subsequent call until you arm a fresh deadline.
 *
 * Pass `0` to disable. Calling with a non-zero value clears the
 * sticky flag (a fresh deadline is a fresh chance). */
AETHER_API void aether_set_call_deadline(int64_t ms);

/* Codegen-tripwire target. Returns 1 when the per-thread deadline
 * has passed OR when `__aether_abort_call` was explicitly invoked
 * since the last `aether_set_call_deadline`. Returns 0 otherwise.
 *
 * Embedders typically don't call this directly — the codegen
 * emits checks at every loop head. The host's job is to set the
 * deadline before invoking the entry function and inspect any
 * error string the entry returns. */
AETHER_API int aether_deadline_tripped(void);

#ifdef __cplusplus
}
#endif

#endif  /* LIBAETHER_H */
