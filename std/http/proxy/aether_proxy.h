/* std.http.proxy — nginx-class reverse-proxy stack.
 *
 * Closes #381. Adds the outbound half of the HTTP server: forward
 * inbound requests to a pool of upstream HTTP servers, with
 * weighted load balancing, active health checks, in-memory LRU
 * response cache, and a per-upstream circuit breaker.
 *
 * The proxy is a middleware (`int (HttpRequest*, HttpServerResponse*,
 * void* user_data) -> int`) that short-circuits the chain (returns 0)
 * after populating the response. It reuses std.http.client's
 * round-trip path (TLS, header lookup, timeouts, error
 * classification) for the upstream call rather than rolling new
 * sockets.
 *
 * Aether-side surface in std/http/proxy/module.ae. Implementation
 * split per concern across:
 *
 *   aether_proxy_pool.c        AetherUpstream + AetherProxyPool lifecycle
 *   aether_proxy_lb.c          RR / least-conn / ip-hash / weighted-RR
 *   aether_proxy_health.c      health-check thread (one per pool)
 *   aether_proxy_breaker.c     circuit-breaker per-upstream state machine
 *   aether_proxy_cache.c       LRU response cache + TTL eviction
 *   aether_proxy_middleware.c  glue: pick → cache → breaker → forward
 *
 * Threading uses runtime/utils/aether_thread.h (pthread shim that
 * works on Windows-MinGW too) — never <pthread.h> directly.
 */
#ifndef AETHER_PROXY_H
#define AETHER_PROXY_H

#include "../../net/aether_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ----- Upstream pool + load balancing ----- */

typedef struct AetherProxyPool AetherProxyPool;
typedef struct AetherProxyOpts AetherProxyOpts;
typedef struct AetherProxyCache AetherProxyCache;

/* Load-balancer algorithm. Strings are accepted at the Aether
 * boundary; aether_proxy_lb_algo_from_string maps to the enum. */
typedef enum {
    AETHER_PROXY_LB_ROUND_ROBIN  = 0,
    AETHER_PROXY_LB_LEAST_CONN   = 1,
    AETHER_PROXY_LB_IP_HASH      = 2,
    AETHER_PROXY_LB_WEIGHTED_RR  = 3,
} AetherProxyLbAlgo;

/* Parse "round_robin" | "least_conn" | "ip_hash" | "weighted_rr".
 * Returns -1 on unknown algorithm. */
int aether_proxy_lb_algo_from_string(const char* name);

/* Allocate an empty pool. Pool refcount starts at 1 (the caller).
 * `request_timeout_sec` and `dial_timeout_ms` apply to every
 * upstream. `max_inflight_per_up` caps concurrent in-flight
 * requests per upstream (over-cap upstreams are skipped by the
 * picker). 0 disables the cap.
 *
 * Returns NULL on bad inputs (negative timeouts, unknown algo) or
 * OOM. */
AetherProxyPool* aether_proxy_pool_new(AetherProxyLbAlgo algo,
                                       int request_timeout_sec,
                                       int dial_timeout_ms,
                                       int max_inflight_per_up);

/* Decrement refcount; free on zero. Joins the health-check thread
 * if one was started. NULL-safe. */
void aether_proxy_pool_free(AetherProxyPool* pool);

/* Add an upstream by base URL. weight is meaningful only under
 * weighted_rr (default 1 elsewhere). Duplicate URLs produce a
 * non-empty error string. */
const char* aether_proxy_upstream_add(AetherProxyPool* pool,
                                      const char* base_url,
                                      int weight);

/* Remove a previously-added upstream. Idempotent (returns "" if
 * absent). */
const char* aether_proxy_upstream_remove(AetherProxyPool* pool,
                                         const char* base_url);

/* ----- Health checks (background pthread per pool) ----- */

/* Configure + start the health-check thread for the pool. The
 * thread fires `GET probe_path` against each upstream every
 * `interval_ms` (with `timeout_ms` per probe), tallies wins/
 * losses, and flips the upstream's healthy state once the
 * threshold is crossed. healthy=0 makes the LB picker skip the
 * upstream until it recovers.
 *
 *   expect_status     — 0 = any 2xx; otherwise the exact status.
 *   interval_ms       — between probes.
 *   timeout_ms        — per-probe HTTP timeout.
 *   healthy_threshold — N consecutive OK probes to flip up.
 *   unhealthy_threshold — N consecutive failed probes to flip down.
 *
 * Returns "" on success, error string on bad inputs / thread
 * spawn failure. Calling twice on the same pool stops the prior
 * thread and starts a new one with the new config. */
const char* aether_proxy_health_checks_enable(AetherProxyPool* pool,
                                              const char* probe_path,
                                              int expect_status,
                                              int interval_ms,
                                              int timeout_ms,
                                              int healthy_threshold,
                                              int unhealthy_threshold);

/* ----- Circuit breaker (per-upstream state, per-pool config) ----- */

const char* aether_proxy_breaker_configure(AetherProxyPool* pool,
                                           int failure_threshold,
                                           int open_duration_ms,
                                           int half_open_max);

/* ----- Response cache (in-memory LRU + TTL) ----- */

typedef enum {
    AETHER_PROXY_CACHE_KEY_URL              = 0,  /* method-agnostic */
    AETHER_PROXY_CACHE_KEY_METHOD_URL       = 1,
    AETHER_PROXY_CACHE_KEY_METHOD_URL_VARY  = 2,
} AetherProxyCacheKeyStrategy;

int aether_proxy_cache_key_strategy_from_string(const char* name);

AetherProxyCache* aether_proxy_cache_new(int max_entries,
                                         int max_body_bytes,
                                         int default_ttl_sec,
                                         AetherProxyCacheKeyStrategy key_strategy);
void aether_proxy_cache_free(AetherProxyCache* cache);

/* ----- Per-mount options ----- */

AetherProxyOpts* aether_proxy_opts_new(void);
void aether_proxy_opts_free(AetherProxyOpts* opts);

/* When non-empty, chop this prefix from `req->path` before
 * forwarding. Empty string disables the chop. */
const char* aether_proxy_opts_set_strip_prefix(AetherProxyOpts* opts,
                                               const char* prefix);

/* preserve_host=0 → rewrite Host: to upstream's host (default).
 * preserve_host=1 → forward client Host: verbatim. */
const char* aether_proxy_opts_set_preserve_host(AetherProxyOpts* opts, int on);

/* X-Forwarded-* injection toggles. Each non-zero arg enables that
 * header; defaults are all on (xff=1, xfp=1, xfh=1). */
const char* aether_proxy_opts_set_xforwarded(AetherProxyOpts* opts,
                                             int xff, int xfp, int xfh);

const char* aether_proxy_opts_bind_cache(AetherProxyOpts* opts,
                                         AetherProxyCache* cache);

/* Default 8 MiB. Larger requests/responses hit a 502 Bad Gateway
 * (proxy buffers the body; oversize is rejected before allocation
 * to avoid OOM under load). */
const char* aether_proxy_opts_set_body_cap(AetherProxyOpts* opts,
                                           int max_body_bytes);

/* ----- Install on server ----- */

/* Mount the proxy under `path_prefix` ("/" forwards everything;
 * "/api" forwards just the /api subtree). The opts' refcount on
 * the pool is bumped (and decremented on opts_free). The middleware
 * is registered with the existing http_server_use_middleware
 * chain — order with other middleware (real_ip, gzip, etc.) is
 * registration order. */
const char* aether_proxy_use_reverse_proxy(HttpServer* server,
                                           const char* path_prefix,
                                           AetherProxyPool* pool,
                                           AetherProxyOpts* opts);

/* One-upstream convenience. Builds a pool with one upstream + RR +
 * default opts, mounts the middleware, and returns "". The pool
 * is owned by the middleware and freed alongside the server. */
const char* aether_proxy_use_simple_proxy(HttpServer* server,
                                          const char* path_prefix,
                                          const char* upstream_url,
                                          int request_timeout_sec);

/* Internal — exposed only because the middleware needs it. The
 * function pointer to register with http_server_use_middleware. */
int aether_middleware_reverse_proxy(HttpRequest* req,
                                    HttpServerResponse* res,
                                    void* user_data);

#ifdef __cplusplus
}
#endif

#endif  /* AETHER_PROXY_H */
