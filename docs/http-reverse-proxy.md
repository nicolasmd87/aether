# `std.http.proxy` — reverse proxy

nginx-class outbound HTTP forwarding for the Aether server.
Forward inbound requests to a pool of upstream HTTP servers, with
weighted load balancing, active health checks, in-memory LRU
response cache, per-upstream circuit breaker, and Hop-by-Hop
header handling per RFC 7230.

The proxy is a middleware plugged into the existing
`http_server_use_middleware` chain. It short-circuits the chain
(returns 0) and owns the response.

## Quick start — single upstream

```aether
import std.http
import std.http.proxy

main() {
    server = http.server_create(8080)
    err = proxy.use_simple_proxy(server, "/", "http://localhost:9000", 30)
    if err != "" { println("proxy: ${err}"); return }
    http.server_start(server)
}
```

`use_simple_proxy(server, path_prefix, upstream_url, timeout_sec)`
builds a single-upstream pool with round-robin (no other upstream
to balance against), default opts, and the given timeout. Mounts
the middleware on `path_prefix`. `"/"` forwards everything;
`"/api"` forwards just the `/api` subtree.

## Pool form — load balancing + health + cache + breaker

```aether
import std.http
import std.http.proxy

main() {
    server = http.server_create(8080)

    // Three upstreams, smooth weighted RR with 3:2:1 ratio.
    pool = proxy.upstream_pool_new("weighted_rr", 30, 0, 100)
    proxy.upstream_add(pool, "http://10.0.0.1:8080", 3)
    proxy.upstream_add(pool, "http://10.0.0.2:8080", 2)
    proxy.upstream_add(pool, "http://10.0.0.3:8080", 1)

    // Active health checks every 5s. Two consecutive 200s flips an
    // upstream up; three consecutive failures flips it down.
    proxy.health_checks_enable(pool, "/health", 200, 5000, 1000, 2, 3)

    // Per-upstream circuit breaker: 5 consecutive failures opens the
    // breaker for 30s; one half-open test request closes it on success.
    proxy.breaker_configure(pool, 5, 30000, 1)

    // Response cache: 1000 entries, 64 KiB body cap, 60s default TTL,
    // canonical key includes Vary headers from upstream.
    cache = proxy.cache_new(1000, 65536, 60, "method_url_vary")

    // Per-mount options.
    opts = proxy.opts_new()
    proxy.opts_bind_cache(opts, cache)
    proxy.opts_set_strip_prefix(opts, "/api")  // /api/users → /users upstream

    err = proxy.use_reverse_proxy(server, "/api", pool, opts)
    if err != "" { println("proxy: ${err}"); return }

    http.server_start(server)
}
```

## Load-balancing algorithms

| Algorithm | Selection rule | When to pick |
|---|---|---|
| `round_robin` | atomic counter mod N (skips ineligible upstreams via retry) | default — even distribution, no per-request decision cost |
| `least_conn` | iterate eligible upstreams; pick smallest in-flight count | upstream response times vary; long-running requests should avoid stacking on one upstream |
| `ip_hash` | FNV-1a over X-Forwarded-For (or X-Real-IP, or "anonymous") mod N | sticky sessions for clients without explicit cookies; soft sticky — falls back to RR if the natural pick is ineligible |
| `weighted_rr` | smooth weighted RR (the algorithm nginx uses) | mixed-capacity backends; produces interleaved sequences (3:1 → A,A,A,B,A,A,A,B) instead of batched |

"Eligible" means: `healthy && breaker not OPEN && inflight < max_inflight_per_up`.

## Health checks

One pthread per pool (not per upstream — N-thread blow-up). The
thread iterates the upstream list every `interval_ms`, fires
`GET probe_path` against each `base_url`, classifies the response,
and flips `_Atomic int healthy` once the threshold trips:

- `healthy_threshold` consecutive OK probes flip an upstream up.
- `unhealthy_threshold` consecutive failed probes flip it down.

`expect_status = 0` accepts any 2xx; non-zero is the exact status.
Down upstreams are skipped by the LB picker (every algorithm).

Configure once via `proxy.health_checks_enable(pool, ...)`; calling
again on the same pool stops the prior thread and starts a new one
with the new config. `proxy.upstream_pool_free` joins the thread.

## Circuit breaker

Per-upstream three-state machine:

```
   CLOSED ── consecutive_failures ≥ failure_threshold ──→ OPEN
                                                          │
   OPEN   ── now - opened_at ≥ open_duration_ms ─────────→ HALF_OPEN
                                                          │
   HALF_OPEN ── any test request succeeds ───────────────→ CLOSED
   HALF_OPEN ── any test request fails ──────────────────→ OPEN (reset opened_at)
```

The hot path is one atomic load per request (`cb_state`).
Transitions are CAS-based for the OPEN→HALF_OPEN race so two
threads simultaneously timing out the open window don't both admit
"the test request"; `half_open_max` caps concurrent test requests.

5xx responses and transport errors (DNS, connect, TLS, timeout)
count as failures. 4xx is treated as ok (client error, not upstream
fault).

`failure_threshold = 0` disables the breaker (default — opt in via
`proxy.breaker_configure`).

## Response cache

In-memory LRU with TTL eviction. Open-chained hash + doubly-linked
LRU list. Cacheability gates per RFC 7234 conservative subset:

- Methods: GET, HEAD only.
- Status codes: 200, 203, 204, 300, 301, 404, 410.
- Request `Cache-Control: no-store` → bypass.
- Response `Cache-Control: no-store` / `private` → bypass.
- Response `Vary: *` → uncacheable.
- Body length ≤ `max_body_bytes`.

TTL resolution: response `Cache-Control: max-age` (clamped to 1
hour for v1 conservatism) → `s-maxage` → `Expires` →
`default_ttl_sec`.

### Key strategies

| Strategy | Key | Use when |
|---|---|---|
| `url` | `path?query` | method-agnostic — POST/GET share entries (rare; usually wrong) |
| `method_url` | `METHOD path?query` | safe default — GET and HEAD don't share entries with each other or with POST |
| `method_url_vary` | as above + `\0name=value` for each header in upstream's `Vary:` | the response varies on `Accept-Encoding` / `Accept-Language` / etc. |

## Hop-by-Hop headers (RFC 7230 §6.1)

Stripped on **both** directions:

```
Connection, Keep-Alive, Proxy-Authenticate, Proxy-Authorization,
TE, Trailer, Transfer-Encoding, Upgrade, Proxy-Connection
```

Plus any header listed in the request's `Connection:` value.

## Headers added by the proxy

| Header | Default | Toggle |
|---|---|---|
| `X-Forwarded-For` | client IP appended (preserves prior comma-separated values) | `proxy.opts_set_xforwarded(opts, xff=0, ...)` |
| `X-Forwarded-Proto` | `http` | `xfp=0` |
| `X-Forwarded-Host` | client's `Host:` header | `xfh=0` |
| `Via` | `1.1 aether-proxy` (appended to existing) | always on |
| `Host` | rewritten to upstream's `host:port` | `proxy.opts_set_preserve_host(opts, 1)` to forward client Host: instead |

Client IP is resolved via the same chain `middleware.use_real_ip`
uses: `X-Forwarded-For` (leftmost) → `X-Real-IP` → `"unknown"`.
Mount `middleware.use_real_ip` before the proxy if you want
clients behind a trusted edge to be visible via XFF.

## Error responses

The middleware emits one of these and short-circuits the chain.
Each carries an `X-Aether-Proxy-Error` header for log aggregators:

| Status | Trigger | `X-Aether-Proxy-Error` |
|--:|---|---|
| 502 | upstream connect / DNS / TLS handshake failure | `upstream_transport` |
| 502 | request body exceeds `opts.max_body_bytes` | `request_body_too_large` |
| 502 | upstream response exceeds `opts.max_body_bytes` | `response_too_large` |
| 502 | request carries `Upgrade:` (WebSocket / h2 upstream — v2 follow-up) | `upgrade_unsupported` |
| 503 | LB picker found no eligible upstream (all unhealthy or breaker open). Includes `Retry-After: 1`. | `no_upstream` |
| 504 | `request_timeout_sec` elapsed before upstream responded | `upstream_timeout` |
| 5xx upstream | passed through verbatim — operators see the actual error | (none — body is upstream's) |

## Limitations (v1)

- **Buffered bodies.** Request and response bodies are buffered in
  memory up to `opts.max_body_bytes` (default 8 MiB). Streaming
  pass-through is the next major feature.
- **No WebSocket / SSE upstreams.** Requests with `Upgrade:`
  headers are refused with 502. The upstream can serve
  Server-Sent Events to clients via the existing `std.http`
  surface; just don't put the proxy in front of those routes.
- **HTTP/1.1 upstreams only.** The proxy calls upstreams via
  `std.http.client` which negotiates HTTP/1.1. Inbound HTTP/2
  works (the inbound side is independent); upstream HTTP/2 is
  v2 follow-up.
- **In-memory cache only.** No disk-backed cache, no shared
  cache across processes.
- **Sharded cache lock is a v2 follow-up.** Single mutex around
  the whole cache works under typical workloads; revisit if
  contention shows in profiling.

## Performance

| Cost | Threshold |
|---|---|
| Single-upstream RR throughput | within 50% of direct `std.http.client` at 100 concurrent connections |
| Cache hit lookup | ≤1 µs warm |
| Health-check thread | <0.1% CPU at 1 Hz × 1 upstream |
| Circuit-breaker hot-path read | single relaxed atomic load |
| WRR pick | O(N) under pool mutex; <100 ns at N=10 |

## Reference

```aether
import std.http.proxy

// Pool
proxy.upstream_pool_new(lb_algo, request_timeout_sec, dial_timeout_ms, max_inflight_per_up) -> ptr
proxy.upstream_pool_free(pool)
proxy.upstream_add(pool, base_url, weight) -> string
proxy.upstream_remove(pool, base_url) -> string

// Health checks
proxy.health_checks_enable(pool, probe_path, expect_status,
    interval_ms, timeout_ms, healthy_threshold, unhealthy_threshold) -> string

// Circuit breaker
proxy.breaker_configure(pool, failure_threshold, open_duration_ms, half_open_max) -> string

// Cache
proxy.cache_new(max_entries, max_body_bytes, default_ttl_sec, key_strategy) -> ptr
proxy.cache_free(cache)

// Per-mount opts
proxy.opts_new() -> ptr
proxy.opts_free(opts)
proxy.opts_set_strip_prefix(opts, prefix) -> string
proxy.opts_set_preserve_host(opts, on) -> string
proxy.opts_set_xforwarded(opts, xff, xfp, xfh) -> string
proxy.opts_bind_cache(opts, cache) -> string
proxy.opts_set_body_cap(opts, max_body_bytes) -> string

// Install
proxy.use_reverse_proxy(server, path_prefix, pool, opts) -> string
proxy.use_simple_proxy(server, path_prefix, upstream_url, request_timeout_sec) -> string
```

All `string`-returning functions are Go-style: `""` is success;
non-empty is an error message.

## See also

- [`docs/http-server.md`](http-server.md) — the inbound side
  (routes, middleware, TLS, h2, WS, SSE, health probes,
  metrics, graceful shutdown).
- [`examples/stdlib/http-reverse-proxy.ae`](../examples/stdlib/http-reverse-proxy.ae)
  — minimal single-upstream demo.
