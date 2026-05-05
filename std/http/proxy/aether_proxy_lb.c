/* aether_proxy_lb.c — load-balancer pickers for the proxy pool.
 *
 * Four algorithms; the active one is set on the pool at creation
 * (pool->algo) and never changes after.
 *
 *   ROUND_ROBIN   atomic counter mod N. Skips ineligible upstreams
 *                 by retrying up to N times.
 *   LEAST_CONN    iterate, pick the eligible upstream with smallest
 *                 atomic_load(&u->inflight). O(N) but N is small.
 *   IP_HASH       FNV-1a over client IP, mod N. Falls back to RR
 *                 when picked is ineligible (soft sticky).
 *   WEIGHTED_RR   smooth weighted RR (the algorithm nginx uses):
 *                 each upstream's current_weight += effective_weight;
 *                 pick max current_weight; subtract total_effective
 *                 from picked's current_weight. Produces interleaved
 *                 sequences (3:1 → A,A,A,B,A,A,A,B…) instead of
 *                 batched (AAAB AAAB).
 *
 * "Eligible" means: healthy && breaker not OPEN && inflight <
 * pool->max_inflight_per_up (when capped). The picker calls
 * `aether_proxy_breaker_admit` which handles the OPEN→HALF_OPEN
 * timeout transition; HALF_OPEN admits up to half_open_max
 * concurrent test requests.
 */

#include "aether_proxy_internal.h"

#include <string.h>
#include <stdint.h>

/* ----- helpers ----- */

static int upstream_eligible(AetherProxyPool* pool, AetherUpstream* u, long now_ms) {
    if (!atomic_load(&u->healthy)) return 0;
    if (pool->max_inflight_per_up > 0 &&
        atomic_load(&u->inflight) >= pool->max_inflight_per_up) return 0;
    if (!aether_proxy_breaker_admit(pool, u, now_ms)) return 0;
    return 1;
}

static AetherUpstream* claim(AetherUpstream* u) {
    /* Mark in-flight before returning so concurrent pickers see the
     * load. Caller must release via aether_proxy_inflight_dec. */
    atomic_fetch_add(&u->inflight, 1);
    return u;
}

/* FNV-1a 64-bit. Used by IP_HASH for client identity hashing — not
 * cryptographic; just a stable, fast spread. */
static uint64_t fnv1a_64(const char* s) {
    uint64_t h = 0xcbf29ce484222325ULL;
    if (!s) return h;
    for (; *s; s++) {
        h ^= (unsigned char)*s;
        h *= 0x100000001b3ULL;
    }
    return h;
}

/* Resolve the client identity for IP_HASH. Mirror the rate-limit
 * middleware's resolution chain so policy is uniform across the
 * stack:  X-Forwarded-For (leftmost) → X-Real-IP → "anonymous". */
static const char* client_identity(HttpRequest* req) {
    const char* xff = http_get_header(req, "X-Forwarded-For");
    static __thread char buf[64];
    if (xff && *xff) {
        size_t i = 0;
        while (xff[i] == ' ' || xff[i] == '\t') i++;
        size_t j = 0;
        while (xff[i] && xff[i] != ',' && j + 1 < sizeof(buf)) {
            buf[j++] = xff[i++];
        }
        /* Trim trailing whitespace. */
        while (j > 0 && (buf[j - 1] == ' ' || buf[j - 1] == '\t')) j--;
        if (j > 0) { buf[j] = '\0'; return buf; }
    }
    const char* xri = http_get_header(req, "X-Real-IP");
    if (xri && *xri) return xri;
    return "anonymous";
}

/* ----- per-algorithm pickers ----- */

static AetherUpstream* pick_round_robin(AetherProxyPool* pool, long now_ms) {
    int n = pool->upstream_count;
    if (n == 0) return NULL;
    for (int tries = 0; tries < n; tries++) {
        int i = (int)((unsigned int)atomic_fetch_add(&pool->rr_cursor, 1) % (unsigned int)n);
        AetherUpstream* u = pool->upstreams[i];
        if (upstream_eligible(pool, u, now_ms)) return claim(u);
    }
    return NULL;
}

static AetherUpstream* pick_least_conn(AetherProxyPool* pool, long now_ms) {
    int n = pool->upstream_count;
    if (n == 0) return NULL;
    AetherUpstream* best = NULL;
    int best_inflight = 0;
    for (int i = 0; i < n; i++) {
        AetherUpstream* u = pool->upstreams[i];
        if (!upstream_eligible(pool, u, now_ms)) continue;
        int cur = atomic_load(&u->inflight);
        if (!best || cur < best_inflight) {
            best = u;
            best_inflight = cur;
        }
    }
    return best ? claim(best) : NULL;
}

static AetherUpstream* pick_ip_hash(AetherProxyPool* pool, HttpRequest* req, long now_ms) {
    int n = pool->upstream_count;
    if (n == 0) return NULL;
    uint64_t h = fnv1a_64(client_identity(req));
    int start = (int)(h % (unsigned int)n);
    AetherUpstream* picked = pool->upstreams[start];
    if (upstream_eligible(pool, picked, now_ms)) return claim(picked);

    /* Soft sticky: when the natural pick is ineligible, fall back to
     * RR-like progress so the request goes somewhere. Documented. */
    for (int tries = 1; tries < n; tries++) {
        AetherUpstream* u = pool->upstreams[(start + tries) % n];
        if (upstream_eligible(pool, u, now_ms)) return claim(u);
    }
    return NULL;
}

static AetherUpstream* pick_weighted_rr(AetherProxyPool* pool, long now_ms) {
    /* Smooth weighted RR (nginx's algorithm).
     *
     * For each upstream:    current_weight += effective_weight
     * Pick the upstream with the largest current_weight.
     * Subtract total_effective_weight from picked.current_weight.
     *
     * Produces interleaved sequences for non-trivial weights and
     * recovers gracefully when an upstream fails (effective_weight
     * shrinks toward 0; recovers additively per success). */

    int n = pool->upstream_count;
    if (n == 0) return NULL;

    AetherUpstream* best = NULL;
    int total = 0;

    for (int i = 0; i < n; i++) {
        AetherUpstream* u = pool->upstreams[i];
        if (!upstream_eligible(pool, u, now_ms)) continue;

        u->current_weight += u->effective_weight;
        total += u->effective_weight;

        if (!best || u->current_weight > best->current_weight) {
            best = u;
        }
    }

    if (!best) return NULL;
    best->current_weight -= total;
    return claim(best);
}

/* ----- public picker ----- */

AetherUpstream* aether_proxy_lb_pick(AetherProxyPool* pool, HttpRequest* req) {
    if (!pool) return NULL;
    long now_ms = aether_proxy_now_ms();

    pthread_mutex_lock(&pool->lock);
    AetherUpstream* u = NULL;
    switch (pool->algo) {
    case AETHER_PROXY_LB_ROUND_ROBIN:
        u = pick_round_robin(pool, now_ms);
        break;
    case AETHER_PROXY_LB_LEAST_CONN:
        u = pick_least_conn(pool, now_ms);
        break;
    case AETHER_PROXY_LB_IP_HASH:
        u = pick_ip_hash(pool, req, now_ms);
        break;
    case AETHER_PROXY_LB_WEIGHTED_RR:
        u = pick_weighted_rr(pool, now_ms);
        break;
    }
    pthread_mutex_unlock(&pool->lock);
    return u;
}
