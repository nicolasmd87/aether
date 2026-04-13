# TinyWeb for Aether

An Aether port of the [Tiny](https://github.com/phamm/tiny) Java web framework — a DSL-style HTTP server.

## Side-by-Side Comparison

### Java (Tiny)

```java
Tiny.WebServer server = new Tiny.WebServer(
    Tiny.Config.create().withHostAndWebPort("localhost", 8080)
) {{
    path("/foo", () -> {
        filter(GET, "/.*", (req, res, ctx) -> {
            if (req.getHeaders().containsKey("sucks")) {
                res.write("Access Denied", 403);
                return STOP;
            }
            return CONTINUE;
        });
        endPoint(GET, "/bar", (req, res, ctx) -> {
            res.write("Hello, World!");
        });
    });

    endPoint(GET, "/users/(\\w+)", (req, res, ctx) -> {
        res.write("User profile: " + ctx.getParam("1"));
    });

    path("/api", () -> {
        endPoint(GET, "/test/(\\w+)", (req, res, ctx) -> {
            res.write("Parameter: " + ctx.getParam("1"));
        });
    });
}}.start();
```

### Aether (TinyWeb)

```aether
server = web_server_host("localhost", 8080) {

    path("/foo") {
        filter(GET, "/.*") |req, res, ctx| {
            if str_eq(request_get_header(req, "sucks"), "") == 0 {
                response_write_status(res, "Access Denied", 403)
                return STOP
            }
            return CONTINUE
        }
        end_point(GET, "/bar") |req, res, ctx| {
            response_write(res, "Hello, World!")
        }
    }

    end_point(GET, "/users/(\\w+)") |req, res, ctx| {
        response_write(res, "User profile: ${request_get_path_param(ctx, \"1\")}")
    }

    path("/api") {
        end_point(GET, "/test/(\\w+)") |req, res, ctx| {
            response_write(res, "Parameter: ${request_get_path_param(ctx, \"1\")}")
        }
    }
}
server_start(server)
```

## WebSocket Side-by-Side

### Java (Tiny)

```java
new Tiny.WebServer(Config.create()
    .withHostAndWebPort("localhost", 8080)
    .withWebSocketPort(8081)) {{

    path("/foo", () -> {
        endPoint(GET, "/bar", (req, res, ctx) -> {
            res.write("Hello, World!");
        });
        webSocket("/eee", (message, sender, context) -> {
            sender.sendBytesFrame(toBytes("Echo: " + bytesToString(message)));
        });
    });
}}.start();
```

### Aether (TinyWeb)

```aether
server = web_server_with_ws("localhost", 8080, 8081) {

    path("/foo") {
        end_point(GET, "/bar") |req, res, ctx| {
            response_write(res, "Hello, World!")
        }
        web_socket("/eee") |message, sender, ctx| {
            ws_send_frame(sender, "Echo: ${message}")
        }
    }
}
server_start(server)
```

### WebSocket Client

```java
// Java
try (WebSocketClient client = new WebSocketClient("ws://localhost:8081/echo", "http://localhost:8080")) {
    client.performHandshake();
    client.sendMessage("Hello");
    client.receiveMessages("stop", msg -> { System.out.println(msg); return true; });
}
```

```aether
// Aether
client = ws_client_connect("localhost", 8081, "/echo", "http://localhost:8080")
ws_client_handshake(client)
ws_client_send(client, "Hello")
ws_client_receive(client, "stop") |msg: string| { println(msg) }
ws_client_close(client)
```

## How It Maps

| Java Tiny                            | Aether TinyWeb                     |
|--------------------------------------|------------------------------------|
| `new Tiny.WebServer(config) {{ }}` | `web_server(port) { }`           |
| `path("/x", () -> { })`           | `path("/x") { }`                |
| `endPoint(GET, "/x", lambda)`      | `end_point(GET, "/x") \|r,s,c\| { }` |
| `filter(GET, "/x", lambda)`        | `filter(GET, "/x") \|r,s,c\| { }` |
| `webSocket("/x", handler)`          | `web_socket("/x") \|msg,sender,ctx\| { }` |
| `sender.sendBytesFrame(bytes)`       | `ws_send_frame(sender, text)`    |
| `new WebSocketClient(url, origin)`   | `ws_client_connect(host, port, path, origin)` |
| `client.performHandshake()`          | `ws_client_handshake(client)`    |
| `client.sendMessage(msg)`            | `ws_client_send(client, msg)`    |
| `client.receiveMessages(stop, fn)`   | `ws_client_receive(client, stop) \|msg\| { }` |
| `client.close()`                     | `ws_client_close(client)`        |
| `new ServerComposition(svr) {{ }}` | `compose(svr) { }`              |
| `Config.withWebSocketPort(p)`        | `web_server_with_ws(host, http, ws)` |
| Double-brace initialization         | Trailing blocks + `_ctx` injection |
| `FilterAction.CONTINUE / STOP`      | `CONTINUE / STOP` constants      |
| `serveStaticFilesAsync(path, dir)`   | `serve_static(path, dir)`        |
| `res.sendResponseChunked(c, code)`   | `chunked_start(s,code)` + `write_chunk(s,c)` |
| `res.writeChunk(out, chunk)`         | `write_chunk(sock, data)`        |
| SSE: `res.getResponseBody()`         | `sse_endpoint("/e") \|stream\| { }` |
| `out.write("data: ...\n\n")`        | `sse_send(stream, data)`         |
| `req.getCookie("name")`              | `request_get_cookie(req, "name")` |
| `ctx.setAttribute("k", v)`          | `ctx_set_attribute(ctx, "k", v)` |
| `ctx.getAttribute("k")`             | `ctx_get_attribute(ctx, "k")`    |
| `req.getBody()`                      | `request_get_body(req)`           |
| `res.write("text", 200)`            | `response_write_status(res, "text", 200)` |
| `ctx.getParam("1")`                 | `request_get_path_param(ctx, "1")`|
| `Config.withSocketTimeoutMillis(n)` | `with_timeout(srv, n)`           |
| `Config.withWebBacklog(n)`          | `with_backlog(srv, n)`           |
| `Config.withWsBacklog(n)`           | `with_ws_backlog(srv, n)`        |
| `Config.withWebKeepAlive(b)`        | `with_keep_alive(srv, 1\|0)`    |
| `exceptionDuringHandling(e, x)`     | `on_error(srv) \|path,msg\| { }` |
| `serverException(e)`                | `on_server_error(srv) \|msg\| { }` |
| `webSocketTimeout(path, addr, e)`   | `on_ws_timeout(srv) \|path,msg\| { }` |
| `webSocketIoException(path, addr, e)` | `on_ws_error(srv) \|path,msg\| { }` |
| `recordStatistics(path, stats)`     | `on_stats(srv) \|stats\| { }`   |
| `endPoint(PATCH, "/x", lambda)`     | `end_point(PATCH, "/x") \|r,s,c\| { }` |
| `endPoint(HEAD, "/x", lambda)`      | `end_point(HEAD, "/x") \|r,s,c\| { }` |
| All 24 `HttpMethods` values          | All 25 constants (ALL..ACL)      |

## Key Design Decisions

- **Trailing blocks as DSL** — Aether's `path("/api") { ... }` is an immediate
  trailing block (runs inline), directly analogous to Java's `path("/api", () -> { })`.
  The `_ctx: ptr` auto-injection wires parent context to child calls automatically.

- **Closure blocks for handlers** — `end_point(GET, "/x") |req, res, ctx| { ... }`
  uses Aether's closure trailing block syntax to create a real closure that is stored
  and invoked later when a request arrives — matching Java's lambda endpoints.

- **Composition via `compose()`** — mirrors `new ServerComposition(server) {{ }}`,
  allowing modular route registration from separate code blocks.

- **Error handling via closures** — Java uses anonymous class overrides
  (`@Override protected void exceptionDuringHandling(...)`). Aether uses registered
  closures: `on_error(server) |path, msg| { ... }`. Same extensibility, idiomatic fit.

- **Statistics via closures** — Java's `recordStatistics(path, Map)` override becomes
  `on_stats(server) |stats| { ... }`. Stats records use maps with the same keys
  (path, endpoint, status, duration, filters) and `FilterStat` equivalents.

- **All HTTP methods** — All 25 methods from Tiny's `HttpMethods` enum are supported
  (GET through ACL). Routes are registered via the generic `http_server_add_route`
  C function rather than per-method functions.

## Static File Serving

```java
// Java
serveStaticFilesAsync("/static", new File(".").getAbsolutePath());
```

```aether
// Aether
serve_static("/static", ".")
```

Delegates to the C `http_serve_static()` function which provides:
- Automatic MIME type detection (html, css, js, images, fonts, etc.)
- Directory traversal protection (rejects `..`, encoded sequences like `%2e`)
- `index.html` fallback for directory roots
- 404 for missing files, 403 for traversal attempts

## Server-Sent Events (SSE) and Chunked Responses

```java
// Java SSE
endPoint(GET, "/events", (req, res, ctx) -> {
    res.setHeader("Content-Type", "text/event-stream");
    res.sendResponseHeaders(200, 0);
    OutputStream out = res.getResponseBody();
    out.write("data: Event 1\n\n".getBytes());
    out.flush();
});
```

```aether
// Aether SSE
sse_endpoint("/events") |stream| {
    sse_send(stream, "Event 1")
    sse_send_event(stream, "tick", "42")
    sse_comment(stream, "keep-alive")
}
```

```java
// Java Chunked
res.setHeader("Transfer-Encoding", "chunked");
res.sendResponseHeaders(200, 0);
res.writeChunk(out, data.getBytes());
res.writeChunk(out, new byte[0]);
```

```aether
// Aether Chunked
chunked_start(sock, 200)
write_chunk(sock, data)
chunked_end(sock)
```

SSE endpoints run on a separate port (like WebSockets) because the C HTTP
server closes connections after handler return. In Tiny's Java, the JDK
`HttpServer` keeps the connection open while the handler holds the
`OutputStream` — Aether achieves the same via raw TCP.

## Architecture

- **Separate ports** — HTTP, WebSocket, and SSE each listen on their own port,
  mirroring Tiny's `HttpServer` + `ServerSocket` split. In production, run
  each accept loop from a separate Aether actor.
- **Raw TCP framing** — WebSocket frames and SSE events are read/written over
  raw TCP sockets, matching Tiny's manual frame parsing
- **RFC 6455 handshake** — SHA-1 + Base64 accept key generation via a small C
  helper (`ws_handshake.c`)
- **C static file handler** — `http_serve_static()` provides MIME detection and
  directory traversal protection, matching Tiny's `serveStaticFilesAsync`
- **Handler dispatch** — all handler types (endpoints, filters, WebSocket, SSE)
  are registered by path and invoked via Aether closures

## Files

- `tinyweb.ae` — The library (HTTP DSL, WebSocket, SSE, static files, chunked, lifecycle)
- `ws_client.ae` — WebSocket client (port of `Tiny.WebSocketClient`)
- `ws_handshake.c` — SHA-1 + Base64 for WebSocket accept key (C extern)
- `example_app.ae` — Port of Java's ExampleApp (HTTP only)
- `example_composition.ae` — Composition, nested paths, auth filters
- `example_websocket.ae` — HTTP + WebSocket server with echo handler
- `example_static.ae` — Static file serving with MIME detection
- `example_sse.ae` — Server-Sent Events + chunked transfer encoding
- `example_auth.ae` — Cookie parsing + filter-to-endpoint attributes (auth pattern)
