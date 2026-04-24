# `std.http.client` — design proposal

This doc proposes the shape of a client-side HTTP refactor. It is not
yet implemented — the intent is to reach alignment before code lands,
since anything under `std/` ships with a stability commitment and
getting the shape wrong is painful to undo.

**Status:** draft for review. Final API may differ.

## Problem

Today `std.http` exposes four client wrappers (`get` / `post` / `put` / `delete`)
that return just the response body on success. The raw extern layer
(`http_response_status`, `http_response_headers`, `http_response_body`)
already has more information than that — the wrappers throw it away.
Specifically:

- **Status codes are hidden.** `http.get(url)` returns an empty body
  and `"http error"` for any 3xx/4xx/5xx — callers can't tell a 404
  from a 500 or a 301. Auth flows that branch on 401 can't be written.
- **Response headers are hidden.** Callers can't read `Location`,
  `Content-Type`, `Set-Cookie`, etc. from the response.
- **Request headers can't be set.** No way to add `Authorization`,
  `User-Agent`, `Accept`, bearer tokens, ETags, `If-None-Match`, etc.
  The C layer's `http_post_raw(url, body, content_type)` signature
  takes Content-Type as a distinguished param precisely because there's
  no general header mechanism.
- **Redirects aren't handled.** 3xx arrives as `"http error"`.
- **JSON round-trip is DIY.** Every caller writes `json.parse(body,
  string.length(body))` after a GET; every POST hand-builds a JSON
  string + sets content-type. Fine at the raw layer, noisy in user code.
- **No timeouts, no retries, no follow-redirect flag.** Not v1-critical,
  but the new API should have room to grow them.

These can't be patched into the existing `http.get` — the return type
needs to widen — so we're taking the opportunity of a major-ish version
bump to do the right thing.

## Non-goals for v1

- **Streaming request/response bodies.** Bodies stay fully buffered
  strings. Server-sent events, chunked uploads, large downloads use
  the raw layer. Streaming is additive future work; the v1 shape
  should not preclude it.
- **Connection pooling / keep-alive.** Each request opens a fresh
  connection. Matches the current behaviour.
- **HTTP/2, HTTP/3.** Out of scope. The underlying C layer is libssl +
  plain socket; upgrading is a bigger project.
- **Cookie jars, auth managers, retry policies.** These are "framework"
  concerns, not "client" concerns. User code wires them up.
- **Removing the existing `std.net`/`std.http` `get/post/put/delete`.**
  They stay. No hard-cut — see "Migration" below.

## Proposed API

Lives in `std/http/module.ae` alongside the existing wrappers. Under
a `client.*` sub-namespace so `http.get(url)` and
`http.client.get(url, opts)` coexist.

### Shape

**One-shot with options map.** Not a builder DSL — builders compose
well with tinyweb's server-side handler-block pattern but feel heavy
for a client request that typically lives on one line. Options map is
compact, optional, and easy to pass around.

```aether
import std.http
import std.map

main() {
    // Minimal: GET with no options.
    resp, err = http.client.get("https://api.example.com/users", null)

    // With headers + timeout.
    opts = map.new()
    map.put(opts, "Authorization", "Bearer <token>")
    map.put(opts, "Accept",        "application/json")
    // Reserved keys the client layer consumes rather than sends as
    // headers: prefixed "_" so they can't collide with real header
    // names. (HTTP headers can't start with "_".)
    map.put(opts, "_timeout_ms",      "5000")
    map.put(opts, "_follow_redirects", "true")
    resp2, err2 = http.client.get("https://api.example.com/users", opts)

    // Response is a struct-ish ptr; the resp_* accessors pull fields.
    status  = http.client.resp_status(resp)     // 200
    body    = http.client.resp_body(resp)       // "{ \"users\": [...] }"
    ctype   = http.client.resp_header(resp, "Content-Type")  // "application/json; ..."
    http.client.resp_free(resp)
}
```

### Endpoints

All return `(resp, err)`. `resp` is an opaque ptr freed via
`http.client.resp_free`.

```aether
http.client.get(url, opts)            -> (resp, err)
http.client.post(url, body, opts)     -> (resp, err)
http.client.put(url, body, opts)      -> (resp, err)
http.client.patch(url, body, opts)    -> (resp, err)
http.client.delete(url, opts)         -> (resp, err)
http.client.head(url, opts)           -> (resp, err)

// Generic for anything else (CONNECT, OPTIONS, custom verbs).
http.client.request(method, url, body, opts) -> (resp, err)
```

`body` can be `null` or `""` for methods that don't carry one.
`opts` can be `null` for "no options".

### Response accessors

```aether
http.client.resp_status(resp)                  -> int
http.client.resp_body(resp)                    -> string
http.client.resp_body_bytes(resp)              -> (bytes, length)  // binary-safe
http.client.resp_header(resp, name)            -> string            // "" if absent
http.client.resp_headers(resp)                 -> ptr (map)         // all of them
http.client.resp_url(resp)                     -> string            // final URL after redirects
http.client.resp_free(resp)
```

### JSON convenience

Two patterns — either is explicit, no magic on content-type:

```aether
// Opt-in helper for JSON POST (marshals the body, sets content-type).
resp, err = http.client.post_json(url, json_value, opts)

// Opt-in helper for JSON response parse.
json_value, err = http.client.resp_body_json(resp)
```

### Error handling

`err != ""` signals **transport-level** failures only: DNS, connect,
TLS handshake, timeout, malformed response. **Non-2xx status is NOT an
error** — the caller gets `(resp, "")` and must check
`resp_status(resp)`. This is Go's `net/http` convention and the right
one — auth flows, 3xx handling, and retry logic all want to branch on
status, not on a thrown error.

The existing `std.http.get`'s "return error on 3xx/4xx/5xx" behaviour
is preserved for backward compatibility of that wrapper. The new
`client.get` is distinct.

## Reserved options keys (v1)

All prefixed with `_` so they can't collide with real HTTP header
names (HTTP headers can't begin with `_`).

| Key | Type | Default | Meaning |
|---|---|---|---|
| `_timeout_ms`       | int-string | "30000" | Per-request timeout in ms. 0 = no timeout. |
| `_follow_redirects` | "true"/"false" | "false" | Follow 3xx up to `_max_redirects`. |
| `_max_redirects`    | int-string | "10"    | Hard cap on redirect chain. |
| `_user_agent`       | string     | "aether/VERSION" | Override default UA. |

Future keys (out of scope for v1 but reserved in the namespace):
`_verify_tls`, `_ca_bundle`, `_client_cert`, `_proxy`, `_keepalive`.

## How this composes with `contrib/tinyweb/`

`tinyweb` is the server-side DSL in contrib/. The two intentionally
mirror but don't overlap:

| | `std.http.client` (new) | `contrib/tinyweb/` (existing) |
|---|---|---|
| Direction | Outbound request | Inbound request |
| Request shape | `(url, body, opts)` one-shot | Handler blocks with `req`/`res`/`ctx` |
| Response shape | `resp` ptr with accessors | `res` ptr with setters |
| Headers | Map in `opts`, `resp_header(r, n)` | `request_get_header(r, n)`, `response_set_header(r, n, v)` |
| JSON | `post_json` / `resp_body_json` | `response_json(res, body)` |
| Server-mode | N/A | `server_host()` + handler tree |

A tinyweb handler proxying to an upstream service uses both:

```aether
end_point(GET, "/users") |req, res, ctx| {
    upstream, err = http.client.get("https://api.example.com/users",
                                    forwarded_headers(req))
    if err != "" { response_write_status(res, err, 502); return }
    status = http.client.resp_status(upstream)
    response_set_header(res, "Content-Type",
                        http.client.resp_header(upstream, "Content-Type"))
    response_write_status(res, http.client.resp_body(upstream), status)
    http.client.resp_free(upstream)
}
```

Tinyweb's README already positions it as a high-value adjacent
library; anything that makes it easier to write "glue" services —
a tinyweb front-end that fans out to HTTP/SQLite back-ends — is the
feel we want.

## Migration

Nothing is removed. The existing `http.get/post/put/delete` (in
`std/http/module.ae` and their `net`-namespaced aliases in
`std/net/module.ae`) stay callable unchanged. The new surface is
additive under the `http.client.*` namespace.

When the new API lands, the reference example in
`docs/stdlib-reference.md` shifts to demonstrate `http.client.get` for
anything beyond a one-line body-only fetch. The old wrappers move to a
"convenience shortcuts" subsection documented as: "for the 80% case of
'just give me the body and fail hard on anything else'; use
`http.client.get` when you need headers / status / redirects."

If a future release decides to deprecate the old wrappers, that's a
separate decision made with a full release cycle of deprecation
warnings. Not planned for v1 of the client refactor.

## Open questions

Reviewers — please weigh in on any of these before code lands.

1. **Should `opts` be a `map<string, string>` (flat, what's proposed
   above) or a purpose-built `RequestOptions` struct?** The map form
   is simpler to extend but has stringly-typed fields (`"5000"` vs
   `5000`) for non-string options. A struct is typed but adds a
   shape to the stdlib API.

2. **Auto-redirect default: off (as proposed) or on?** Off is the
   principle-of-least-surprise choice — follow-redirects has security
   implications (redirect-to-file, redirect-to-internal). On matches
   curl's default and what casual users probably expect. I lean "off,
   explicit on via `_follow_redirects=true`". Counter-argument welcome.

3. **`resp_body_bytes(resp) -> (bytes, length)` — worth adding in v1
   or punt?** Most HTTP bodies aren't binary, but images / gzipped
   responses / binary protocol over HTTP are real. Without it the
   response body truncates at the first NUL (strlen-based). Punting
   means adding it later requires callers to switch away from
   `resp_body` — mild migration cost. I lean "include in v1".

4. **JSON convenience — `post_json(url, JsonValue, opts)` requiring
   `std.json` parsing of the input, or `post_json(url, string, opts)`
   that just sets Content-Type?** The former is more Aether-idiomatic
   (you're already building a `JsonValue` from `json.create_*`); the
   latter is simpler. I lean toward the former + keep the latter as
   `post(url, body, opts_with_content_type_set)`.

5. **Error format — "code: message" ("dns: no such host") or just
   "no such host"?** Consistent machine-readable prefixes help
   callers branch on `err`; human strings are easier to read but
   harder to test for. I lean toward prefix-tagged errors:
   `"dns: ..."`, `"connect: ..."`, `"tls: ..."`, `"timeout: ..."`.

6. **`resp_url(resp)` — useful?** Only meaningful when redirects are
   followed. Cost is one extra accessor to maintain. Low signal;
   punt if trivial implementation isn't trivial.
