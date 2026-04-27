# `std.http` HTTP Server

Aether's built-in HTTP server. Routes, request/response, middleware
chain, TLS termination, HTTP/1.1 keep-alive, per-connection actor
dispatch, structured logging, Prometheus metrics, graceful shutdown,
health probes, and Server-Sent Events — all in `std.http` and
`std.http.middleware`.

This document covers the surface that ships in the round-2
issue-pack. HTTP/2 and WebSocket land in follow-up PRs (see
[`docs/next-steps.md`](next-steps.md) → "remaining Tier 2 protocols").

---

## Quickstart

```aether
import std.http

handle_root(req: ptr, res: ptr, ud: ptr) {
    http.response_set_status(res, 200)
    http.response_set_header(res, "Content-Type", "text/plain")
    http.response_set_body(res, "hello")
}

main() {
    server = http.server_create(8080)
    http.server_get(server, "/", handle_root, 0)
    http.http_server_start_raw(server)  // blocks
}
```

Run it: `ae run server.ae`. Hit `http://127.0.0.1:8080/` from any client.

---

## Routing

```aether
http.server_get   (server, "/users",      handle_list,   0)
http.server_get   (server, "/users/:id",  handle_get,    0)
http.server_post  (server, "/users",      handle_create, 0)
http.server_put   (server, "/users/:id",  handle_update, 0)
http.server_delete(server, "/users/:id",  handle_delete, 0)
http.server_add_route(server, "PATCH", "/users/:id", handle_patch, 0)
```

Path parameters (`:id`) are extracted and reachable via
`http.get_path_param(req, "id")`. Query parameters via
`http.get_query_param(req, "key")`.

---

## TLS termination

```aether
err = http.server_set_tls(server, "/etc/ssl/cert.pem", "/etc/ssl/key.pem")
if err != "" { println("TLS setup: ${err}"); exit(1) }
```

After this call every accepted connection completes a TLS handshake
(`TLS 1.2+`, compression and renegotiation disabled) before the HTTP
parse runs. Plaintext clients hitting the TLS port get rejected at
handshake time. The cert and key must be PEM-encoded; the server
validates they match.

When the build does not include OpenSSL (`AETHER_HAS_OPENSSL`
undefined), `server_set_tls` returns `"TLS unavailable: built without
OpenSSL"`.

---

## HTTP/1.1 keep-alive

```aether
http.server_set_keepalive(server, 1, 100, 30000)
//                              ^   ^    ^
//                       enabled max  idle_ms
```

Loop terminates when:
- client sends `Connection: close`,
- HTTP/1.0 default,
- `max_requests` reached (0 = unlimited),
- `idle_ms` elapses with no new bytes (0 = use default 30s),
- response status mandates close (408, 426).

Server emits `Connection: keep-alive` and `Keep-Alive: timeout=N,
max=M` headers per response.

---

## Per-connection actor dispatch

```aether
// User actor step function — replaces the thread-pool worker path
@c_callback worker_step(msg_ptr: ptr) {
    msg = unwrap_msg_http_connection(msg_ptr)
    http.http_server_drain_connection(g_server, msg.client_fd)
}
```

`http.http_server_drain_connection(server, client_fd)` is the public
helper that runs the full per-connection lifecycle (TLS handshake,
keep-alive request loop, route dispatch, response emission, socket
close). User actor step functions registered via
`http_server_set_actor_handler` should call this on the
`MSG_HTTP_CONNECTION` message's client_fd to get identical behaviour
to the thread-pool worker path.

---

## Middleware

Eight production middleware in `std.http.middleware`. All registered
via the existing function-pointer chain — hot path stays C function
pointers, no closure indirection.

```aether
import std.http.middleware

// CORS — open it up to one origin
middleware.use_cors(server,
    "https://example.com",
    "GET, POST, OPTIONS",
    "Content-Type, Authorization",
    0,        // allow_credentials
    3600)     // max_age (seconds)

// Per-IP token-bucket rate limit: 100 req per 60s
middleware.use_rate_limit(server, 100, 60000)

// Virtual host gate
middleware.use_vhost(server, "api.example.com,app.example.com")

// Basic auth (verifier is a @c_callback Aether function)
middleware.use_basic_auth(server, "Restricted", verify_creds_cb, null)

// Response-side gzip (skips bodies < min_size, skips when client
// did not advertise Accept-Encoding: gzip, etc)
middleware.use_gzip(server, 1024, 6)

// Static files
middleware.use_static_files(server, "/assets", "/var/www/static")

// URL rewriting
opts = aether_rewrite_opts_new()
middleware.rewrite_add_rule(opts, "/old/", "/new/")
middleware.use_rewrite(server, opts)

// Custom error pages
ep = aether_error_pages_opts_new()
middleware.error_pages_register(ep, 404, "<h1>Not found</h1>", "text/html")
middleware.error_pages_register(ep, 500, "<h1>Server error</h1>", "text/html")
middleware.use_error_pages(server, ep)
```

Middleware shape: each is a function-pointer chain entry. Pre-handler
middleware (cors / basic_auth / rate_limit / vhost / static_files /
rewrite) run before route dispatch and can short-circuit; response
transformers (gzip / error_pages) run after the route handler emits
the response. Order is registration order.

---

## Server-Sent Events

```aether
@c_callback events_handler(req: ptr, sse: ptr, ud: ptr) {
    http.sse_send(sse, "tick", "1")
    http.sse_send(sse, "tick", "2")
    http.sse_send_id(sse, "tick", "3", "evt-3")
    // Falling out of the handler closes the connection.
}

http.server_sse(server, "/events", events_handler, null)
```

The handler owns the connection lifetime. Server emits the standard
SSE response head (`200 OK`, `Content-Type: text/event-stream`,
`Cache-Control: no-cache`, `Connection: close`) before invoking the
handler. `sse_send` returns -1 on transport error so the handler can
stop emitting on disconnect.

Multi-line `data:` payloads are split on `\n` per the spec. Optional
`id:` field via `sse_send_id` lets clients reconnect with
`Last-Event-ID`.

---

## Structured access logging

```aether
http.server_set_access_log(server, "json", "/var/log/api.log")
//                                  ^         ^
//                            "json" or       file path,
//                            "combined"      "-" for stderr,
//                            (NCSA fmt)      "" disables
```

JSON format example output:

```
{"ts":"2026-04-27T11:32:14Z","method":"GET","path":"/api/users",
 "status":200,"bytes":342,"dur_us":1247,"ua":"curl/8.4.0","ref":""}
```

NCSA Combined Log Format is the Apache default. File is opened
`"ab"` so process restarts append rather than truncate.

---

## Prometheus metrics

```aether
http.server_set_metrics(server, "/metrics")
```

Exposes per-route counters and latency histograms on the configured
endpoint:

```
# TYPE aether_http_requests_total counter
# TYPE aether_http_errors_total counter
# TYPE aether_http_4xx_total counter
# TYPE aether_http_request_duration_seconds histogram
aether_http_requests_total{method="GET",path="/api/users"} 4239
aether_http_errors_total{method="GET",path="/api/users"} 12
aether_http_request_duration_seconds_bucket{method="GET",path="/api/users",le="0.005"} 3801
aether_http_request_duration_seconds_bucket{method="GET",path="/api/users",le="0.025"} 4220
...
aether_http_request_duration_seconds_sum{method="GET",path="/api/users"} 47.392011
aether_http_request_duration_seconds_count{method="GET",path="/api/users"} 4239
```

Histogram buckets: 5ms / 25ms / 100ms / 500ms / 2s / 10s / +Inf.
Counters are atomic; insertion-time mutex only.

Custom hooks: `http.http_server_use_request_hook(server, hook, ud)`
accepts user code that does its own thing (OpenTelemetry, distributed
tracing, etc.). Multiple hooks may register; they fire in registration
order after the response transformer chain.

---

## Graceful shutdown

```aether
err = http.server_shutdown_graceful(server, 5000)  // 5-second deadline
if err != "" { println("warning: ${err}") }       // "timeout" if not drained
```

Stops accepting new connections (closes the listen socket), then
waits up to `timeout_ms` for in-flight ones to drain. Backed by an
atomic `inflight_connections` counter accurate across both
thread-pool and actor-dispatch modes. Typically wired into a
SIGTERM handler.

---

## Lifecycle hooks

```aether
@c_callback on_started(server: ptr, ud: ptr) {
    println("server is ready")
}
@c_callback on_stopped(server: ptr, ud: ptr) {
    println("server stopped")
}

http.server_on_start(server, on_started, null)
http.server_on_stop (server, on_stopped, null)
```

`on_start` fires once after the listen socket is bound, before the
accept loop runs. `on_stop` fires after the accept loop exits, before
sockets close.

---

## Health probes

```aether
@c_callback ready_check(ud: ptr) -> int {
    return database_is_healthy()
}

http.server_set_health_probes(server, "/healthz", "/readyz", ready_check, null)
```

Registers two routes:
- `/healthz` always 200 "ok" (process is up)
- `/readyz` calls `ready_check` (1 → 200 "ready", 0 → 503 "not ready")

Either path may be `""` to skip; `ready_check` may be null. Mirrors
Kubernetes / Docker probe conventions.

---

## Performance tuning checklist

- TLS termination: `AETHER_HAS_OPENSSL` enables it; cert + key must
  be PEM. SNI / ALPN advertising for HTTP/2 lands in the follow-up
  HTTP/2 PR.
- Keep-alive: turn it on for any server fronting a chatty client
  (browsers, internal microservices). `max_requests` of 100–1000 +
  `idle_ms` of 30–60s is the standard tuning.
- Per-connection actor dispatch: only matters for keep-alive
  sessions and SSE/WebSocket. Default thread-pool path is fine for
  short-lived requests.
- gzip: turn on for text responses ≥ 1KB. Skip for already-compressed
  content (images, video) — the middleware skips automatically based
  on the absence of the `Content-Encoding` header from the handler.
- Response transformer chain: keep transformers cheap. They run on
  every response; expensive transforms (template rendering, image
  manipulation) belong in route handlers, not transformers.
