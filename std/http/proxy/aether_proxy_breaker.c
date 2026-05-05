/* aether_proxy_breaker.c — per-upstream circuit breaker.
 *
 * Three states. Transitions are explicit and CAS-based for the
 * OPEN→HALF_OPEN race so two threads simultaneously timing out
 * the open window don't both admit "the test request":
 *
 *   CLOSED ── consecutive_failures ≥ failure_threshold ─→ OPEN
 *   OPEN ── now - opened_at_ms ≥ open_duration_ms     ─→ HALF_OPEN
 *   HALF_OPEN ── any test request succeeds            ─→ CLOSED
 *   HALF_OPEN ── any test request fails               ─→ OPEN (reset opened_at)
 *
 * The hot path is `aether_proxy_breaker_admit` — a single relaxed
 * atomic load of cb_state on the closed path. `aether_proxy_breaker_record`
 * runs once per upstream call after classification (ok = 2xx/3xx/4xx;
 * !ok = 5xx + transport errors). 4xx is treated as ok because it's
 * client error, not upstream fault.
 *
 * When `pool->br_failure_threshold == 0` the breaker is disabled —
 * admit returns 1 unconditionally, record is a no-op.
 */

#include "aether_proxy_internal.h"

const char* aether_proxy_breaker_configure(AetherProxyPool* pool,
                                           int failure_threshold,
                                           int open_duration_ms,
                                           int half_open_max) {
    if (!pool) return "pool is null";
    if (failure_threshold < 0)  return "failure_threshold must be >= 0";
    if (open_duration_ms < 0)   return "open_duration_ms must be >= 0";
    if (half_open_max <= 0)     return "half_open_max must be > 0";

    pool->br_failure_threshold = failure_threshold;
    pool->br_open_duration_ms  = open_duration_ms;
    pool->br_half_open_max     = half_open_max;
    return "";
}

int aether_proxy_breaker_admit(AetherProxyPool* pool,
                               AetherUpstream* u,
                               long now_ms) {
    if (!pool || !u) return 0;
    if (pool->br_failure_threshold == 0) return 1;  /* disabled */

    int s = atomic_load_explicit(&u->cb_state, memory_order_acquire);
    if (s == AETHER_PROXY_CB_CLOSED) return 1;

    if (s == AETHER_PROXY_CB_OPEN) {
        long opened = atomic_load_explicit(&u->cb_opened_at_ms,
                                           memory_order_acquire);
        if (now_ms - opened < pool->br_open_duration_ms) return 0;

        /* Try to transition to HALF_OPEN. The CAS winner gets to
         * admit the first test request; losers re-check below. */
        int expected = AETHER_PROXY_CB_OPEN;
        if (atomic_compare_exchange_strong(&u->cb_state, &expected,
                                           AETHER_PROXY_CB_HALF_OPEN)) {
            atomic_store(&u->cb_half_open_inflight, 1);
            return 1;
        }
        s = AETHER_PROXY_CB_HALF_OPEN;
    }

    /* HALF_OPEN: admit only up to half_open_max concurrent tests. */
    int prev = atomic_fetch_add(&u->cb_half_open_inflight, 1);
    if (prev >= pool->br_half_open_max) {
        atomic_fetch_sub(&u->cb_half_open_inflight, 1);
        return 0;
    }
    return 1;
}

void aether_proxy_breaker_record(AetherProxyPool* pool,
                                 AetherUpstream* u,
                                 int ok) {
    if (!pool || !u) return;
    if (pool->br_failure_threshold == 0) return;  /* disabled */

    if (ok) {
        atomic_store(&u->cb_consecutive_failures, 0);
        int s = atomic_load(&u->cb_state);
        if (s == AETHER_PROXY_CB_HALF_OPEN) {
            int expected = AETHER_PROXY_CB_HALF_OPEN;
            atomic_compare_exchange_strong(&u->cb_state, &expected,
                                           AETHER_PROXY_CB_CLOSED);
            atomic_fetch_sub(&u->cb_half_open_inflight, 1);
        }
        return;
    }

    /* failure */
    int fails = atomic_fetch_add(&u->cb_consecutive_failures, 1) + 1;
    int s = atomic_load(&u->cb_state);

    if (s == AETHER_PROXY_CB_HALF_OPEN) {
        /* Test request failed — straight back to OPEN with a fresh
         * timer. The CAS narrows the race with concurrent record
         * calls landing on the same upstream. */
        int expected = AETHER_PROXY_CB_HALF_OPEN;
        if (atomic_compare_exchange_strong(&u->cb_state, &expected,
                                           AETHER_PROXY_CB_OPEN)) {
            atomic_store(&u->cb_opened_at_ms, aether_proxy_now_ms());
        }
        atomic_fetch_sub(&u->cb_half_open_inflight, 1);
        return;
    }

    if (s == AETHER_PROXY_CB_CLOSED && fails >= pool->br_failure_threshold) {
        int expected = AETHER_PROXY_CB_CLOSED;
        if (atomic_compare_exchange_strong(&u->cb_state, &expected,
                                           AETHER_PROXY_CB_OPEN)) {
            atomic_store(&u->cb_opened_at_ms, aether_proxy_now_ms());
        }
    }
}
