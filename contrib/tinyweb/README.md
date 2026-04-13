# TinyWeb for Aether

A DSL-style HTTP server library. Port of [Tiny](https://github.com/phamm/tiny).

## Usage

```aether
server = web_server_host("localhost", 8080) {

    path("/foo") {
        filter(GET, "/.*") |req, res, ctx| {
            return CONTINUE
        }
        end_point(GET, "/bar") |req, res, ctx| {
            response_write(res, "Hello, World!")
        }
        web_socket("/eee") |msg, sender, ctx| {
            ws_send_frame(sender, "Echo: ${msg}")
        }
    }

    serve_static("/static", ".")

    end_point(GET, "/users/(\\w+)") |req, res, ctx| {
        response_write(res, "User: ${request_get_path_param(ctx, \"1\")}")
    }
}
server_start(server)
```

## Features

- **HTTP endpoints** — GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS, and 18 extended methods
- **Path nesting** — `path("/api") { path("/v1") { end_point(...) } }`
- **Filters** — `filter(GET, "/.*") |req, res, ctx| { CONTINUE / STOP }`
- **Composition** — `compose(server) { ... }` to add routes after construction
- **WebSocket** — server + client with RFC 6455 handshake
- **SSE** — `sse_endpoint("/events") |stream| { sse_send(stream, data) }`
- **Static files** — MIME detection, traversal protection, index.html fallback
- **Chunked transfer** — `chunked_start`, `write_chunk`, `chunked_end`
- **Cookies** — `request_get_cookie(req, "name")`
- **Attributes** — `ctx_set_attribute` / `ctx_get_attribute` for filter-to-endpoint data
- **Error callbacks** — `on_error`, `on_server_error`, `on_ws_timeout`, `on_ws_error`
- **Statistics** — `on_stats(server) |stats| { ... }`
- **Config** — `with_timeout`, `with_backlog`, `with_ws_backlog`, `with_keep_alive`

## Files

- `module.ae` — The library
- `ws_client.ae` — WebSocket client
- `ws_handshake.c` — SHA-1 + Base64 for WebSocket accept key (C extern)
- `example_app.ae` — HTTP endpoints with filters and nested paths
- `example_composition.ae` — Modular composition, deep nesting, auth filters
- `example_websocket.ae` — HTTP + WebSocket server with echo handler
- `example_static.ae` — Static file serving
- `example_sse.ae` — Server-Sent Events + chunked transfer
- `example_auth.ae` — Cookie parsing + filter-to-endpoint attributes
- `test_spec.ae` — DSL registration unit tests
- `test_integration.ae` — HTTP round-trip integration tests
