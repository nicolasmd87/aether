# Aether HTTP Library

`std.http` is the HTTP stack: a v2 client (request/response with full
header + status + body access), a routing-aware HTTP server, and a
record/replay VCR for testing HTTP-shaped code without the network.

The three submodules:

| Import                      | Purpose                                              |
|-----------------------------|------------------------------------------------------|
| `std.http`                  | v1 one-liners (`http.get`, `http.post`, ...) + server-side request/response accessors |
| `std.http.client`           | v2 request builder, full response surface            |
| `std.http.server.vcr`       | Servirtium-format record/replay for HTTP tests       |

The v1 one-liners stay supported. v2 is the API you reach for when
v1's "URL in, body out" shape stops fitting.

## v1 client — one-liners

```ae
import std.http

body, err = http.get("http://api.example.com/things")
if err != "" { /* transport-level failure */ }

body, err = http.post("http://api.example.com/things",
                     "{\"name\":\"thing\"}",
                     "application/json")
```

`http.get`, `http.post`, `http.put`, `http.delete` collapse the whole
exchange into a `(body, err)` tuple. Useful for "no auth, JSON in,
JSON out, 200-means-good" calls. Anything more nuanced — custom request
headers, response-header capture, status discrimination — wants v2.

## v2 client — request builder, full response

```ae
import std.http
import std.http.client

req = client.request("GET", "http://api.example.com/things/42")
client.set_header(req, "Authorization", "Bearer abc123")
client.set_header(req, "Accept", "application/json")
client.set_timeout(req, 30)

resp, err = client.send_request(req)
client.request_free(req)
if err != "" { /* transport-level failure (DNS, connect, TLS) */ }

status = client.response_status(resp)        // 200, 404, 403, ...
body   = client.response_body(resp)
etag   = client.response_header(resp, "ETag") // case-insensitive lookup
client.response_free(resp)
```

Key shape choices, with rationale (full design notes in
`http-client-improvement-plan.md` at the repo root):

- **`method: string`** rather than an enum. WebDAV (PROPFIND, MKCOL),
  DeltaV (REPORT, CHECKOUT), HTTP PATCH, and project-specific verbs
  all need to ride through. The cost of a string-typed method is
  zero (libcurl's `CURLOPT_CUSTOMREQUEST` already takes a string).
- **Non-2xx is not an error.** `send_request` returns the response
  cleanly; the caller decides what to do with status 404. Transport-
  level failures (refused connection, TLS handshake fail, timeout)
  are what populate the `err` slot.
- **Case-insensitive response-header lookup.** HTTP header names
  are case-insensitive on the wire and forcing callers to remember
  whether a server returned `X-Node-Hash` or `x-node-hash` is
  needless friction.
- **Binary-safe bodies.** `response_body` returns an AetherString
  (length-prefixed, NUL-clean). Callers that know they're getting
  JSON treat it as text; callers handling blobs reach for
  `aether_string_data` / `aether_string_length`.
- **Function named `send_request`** rather than `send` because
  `send` is reserved for actor messaging (issue #233 tracks the
  codegen-side fix that would let us shorten this).

### v2.1 sugar wrappers

`client.get_with_headers(url, header_pairs)` and
`client.post_with_status(url, body, ct)` are pre-built helpers for
the common "GET with auth headers" and "POST and inspect status"
shapes. They're pure Aether on top of the v2 builder — same
expressiveness, fewer call-site lines.

JSON round-trip helpers compose the same way:

```ae
import std.http.client
import std.json

payload = json_create_object()
json_object_set_raw(payload, "name", json_create_string("alice"))

resp, err = client.post_json("https://api.example.com/users", payload)
if err == "" {
    parsed, perr = client.response_body_json(resp)
    if perr == "" {
        // walk parsed; caller owns it.
        json_free(parsed)
    }
    client.response_free(resp)
}
json_free(payload)
```

`post_json` marshals the value via `json.stringify`, sets
`Content-Type` and `Accept` to `application/json`, and returns the
same `(resp, err)` shape as `send_request` — non-2xx status is still
the caller's call. `response_body_json` wraps `response_body` +
`json.parse`, returning `(value, "")` on success or `(null, error)`
when the body isn't valid JSON.

## Server — routing + actor wiring

```ae
import std.http
import std.http.client  // for the smoke test you'll write

extern http_server_start_raw(server: ptr) -> int

handle_thing(req: ptr, res: ptr, ud: ptr) {
    id = http.get_path_param(req, "id")
    http.response_set_status(res, 200)
    http.response_set_header(res, "Content-Type", "application/json")
    http.response_set_body(res, "{\"id\":\"${id}\"}")
}

main() {
    raw = http.server_create(8080)
    http.server_get(raw, "/things/:id", handle_thing, 0)
    http_server_start_raw(raw)
}
```

The server supports path parameters (`:id`), wildcards (`*`),
middleware, and the full set of HTTP verbs via `http.server_add_route`.

The integration tests under `tests/integration/test_http_client_v2.ae`
double as a worked example: they spin up an in-process echo server
in an actor, then drive client calls against it on the main thread —
both halves of the test in the same Aether binary, no shell wrapper,
no subprocess management.

## VCR — Servirtium record/replay

`std.http.server.vcr` is Aether's implementation of
[Servirtium](https://servirtium.dev), a cross-language HTTP
record/replay framework. Tapes are markdown documents — the same
format Servirtium implementations in Java, Kotlin, Python, Go, etc.
use, so a tape recorded by any of them is replayable here.

The metaphor: a VCR is the device, a tape is the medium, and
record / replay are the operations the device performs on the medium.

### Why you'd use it

- **Test against a real API once, then forever offline.** First run
  records every request/response into a tape file. Subsequent runs
  replay from the tape — no network, no flakiness, no rate limits,
  no upstream-changed-overnight surprises.
- **Pin upstream behavior.** The tape becomes a contract: if the
  upstream service changes its response shape, the re-record check
  catches the byte-level diff (`vcr.flush_or_check`).
- **Run UI tests offline.** Static-content mounts let
  Selenium/Cypress/Playwright pull CSS/JS/images straight from disk
  without each one polluting the tape.
- **Scrub secrets before committing the tape.** Authorization
  tokens, session cookies, server-issued ids — register a redaction
  and the on-disk tape is clean while the in-memory capture (and
  thus the live SUT) sees the real bytes.

### Quick start (replay)

```ae
import std.http.server.vcr
import std.http
import std.http.client

extern http_server_start_raw(server: ptr) -> int

message StartVCR { raw: ptr }
actor VCRActor { state s = 0
    receive { StartVCR(raw) -> { s = raw; http_server_start_raw(raw) } } }

main() {
    raw = vcr.load("tests/tapes/my.tape", 18099)
    a = spawn(VCRActor())
    a ! StartVCR { raw: raw }
    sleep(500)

    // SUT now drives http://127.0.0.1:18099 — every call served
    // from the tape.
    body, err = http.get("http://127.0.0.1:18099/things/42")

    vcr.eject(raw)
}
```

### Tape format

Canonical Servirtium markdown. Each interaction is a `## Interaction N:`
heading followed by four `### ...` sections (request headers,
request body, response headers, response body). The response body
section's heading carries the status and content-type:

```
## Interaction 0: GET /ok

### Request headers recorded for playback:

```
Host: 127.0.0.1:18091
```

### Request body recorded for playback ():

```
```

### Response headers recorded for playback:

```
Content-Type: text/plain
```

### Response body recorded for playback (200: text/plain):

```
ok-body
```
```

(That literal ```` ``` ```` is part of the tape, not markdown
escaping — fenced code blocks live inside the document.)

### Surface (Aether-side, by feature)

| Feature                           | Functions                                   |
|-----------------------------------|---------------------------------------------|
| **Replay** (load + serve)         | `vcr.load`, `vcr.eject`, `vcr.tape_length`  |
| **Record** (capture + flush)      | `vcr.record`, `vcr.record_full`, `vcr.flush` |
| **Re-record check**               | `vcr.flush_or_check`                        |
| **Notes** (per-interaction)       | `vcr.note`                                  |
| **Redactions** (scrub at flush)   | `vcr.redact`, `vcr.clear_redactions`, `vcr.FIELD_PATH`, `vcr.FIELD_RESPONSE_BODY` |
| **Strict request matching**       | `vcr.last_error`, `vcr.clear_last_error`    |
| **Static-content mounts**         | `vcr.static_content`, `vcr.clear_static_content` |
| **Optional markdown formats**     | `vcr.emphasize_http_verbs`, `vcr.indent_code_blocks`, `vcr.clear_format_options` |

### Servirtium roadmap status

The Servirtium project documents a 16-step methodical path for new
implementations at https://servirtium.dev. Aether's `std.http.server.vcr`
covers steps 1–12:

| Step | Feature                                       | Status |
|------|-----------------------------------------------|:------:|
| 1    | Climate API client + real-network tests       | ✓ |
| 2    | Replay via VCR                                | ✓ |
| 3    | Record-then-replay                            | ✓ |
| 4    | Re-record byte-diff check                     | ✓ |
| 5    | Multiple-interaction handling                 | ✓ |
| 6    | Library extracted to its own module           | ✓ |
| 7    | Other HTTP verbs (POST/PUT/DELETE/PATCH)      | ✓ |
| 8    | Mutation operations (redactions)              | ✓ |
| 9    | Per-interaction Notes                         | ✓ |
| 10   | Strict request matching + last-error surface  | ✓ |
| 11   | Static-content serving                        | ✓ |
| 12   | Optional markdown format toggles              | ✓ |
| 13   | Publish to package land                       | — |
| 14   | Proxy server mode                             | — |
| 15   | Pass the cross-impl compatibility suite       | — |
| 16   | Cross-implementation interop                  | — |

The hostile-tape fixtures from servirtium/README's
`broken_recordings/` are checked in under
`tests/integration/tapes/` and exercised by the strict-match
integration tests.

### What's next on the Servirtium list

The roadmap is methodical and pausable — a future contributor can
pick up exactly where steps 1–12 left off:

- **Step 13 — Publish to package land.** Package the VCR module
  under whatever Aether's distribution story turns out to be
  (`aether.toml` package metadata, registry publish flow). The
  module already lives in its own subtree (`std/http/server/vcr/`)
  with self-contained surface, so this is mostly packaging plumbing
  rather than redesign.
- **Step 14 — Proxy server mode.** A second mode of operation
  alongside record/replay: VCR sits *between* the SUT and a real
  upstream during normal operation (no tape involved), letting
  developers point browsers / Selenium / Postman at it for ad-hoc
  exploration. The recorder-side proxy plumbing from step 3 is
  a starting point — making it persistent rather than test-scoped
  is the new work.
- **Step 15 — Pass the cross-impl compatibility suite.** Servirtium
  upstream maintains a canonical TCK that any conforming
  implementation must pass. We've already absorbed the
  `broken_recordings/` fixtures into the strict-match tests; the
  full suite goes further (assertions about Notes positioning,
  redaction order, format-toggle interop, etc.).
- **Step 16 — Cross-implementation interop.** Hand a tape recorded
  by Java/Kotlin/Python Servirtium to Aether and confirm replay
  works (and vice versa). The format is shared by design, so the
  test is essentially an integration test against tapes from those
  other repos.

Steps 13 and 14 are the substantive ones. Steps 15 and 16 are
verification — if the implementation is right, they should pass
without further changes (and if they don't, the failures pinpoint
where this implementation drifted from the spec).

### When NOT to reach for VCR

- **You're testing the HTTP server itself.** VCR replaces the
  *upstream* in your test, not your server-under-test. For
  testing your own request handlers, use the in-process actor
  pattern shown above.
- **The exchanges are inherently non-deterministic.** Tapes are a
  poor fit for endpoints that return server-current timestamps,
  unique request ids, or randomized payloads. Redactions can
  paper over a few of these; if it's most of the response, VCR
  isn't the right tool.

## Test coverage

| Test file                                              | What it exercises |
|--------------------------------------------------------|-------------------|
| `tests/integration/test_http_client_v2.ae`             | v2 client surface (headers, status, timeout, transport failure) |
| `contrib/climate_http_tests/test_climate_real.ae`      | Live network — real WorldBank climate API |
| `contrib/climate_http_tests/test_climate_via_vcr.ae`   | Replay path — same 5 assertions, no network |
| `contrib/climate_http_tests/test_climate_record_then_replay.ae` | Record-then-replay loop, including AETHER_VCR_RECORD re-record check |
| `tests/integration/test_vcr_redactions.ae`             | Step 8 — flush-time scrubbing of body / path |
| `tests/integration/test_vcr_notes.ae`                  | Step 9 — per-interaction `[Note]` blocks |
| `tests/integration/test_vcr_strict_match.ae`           | Step 10 — header mismatch surfaces via `last_error` |
| `tests/integration/test_vcr_strict_match_body.ae`      | Step 10 — body mismatch surfaces via `last_error` |
| `tests/integration/test_vcr_static_content.ae`         | Step 11 — `/api/*` from tape, `/assets/*` from disk |
| `tests/integration/test_vcr_format_options.ae`         | Step 12 — `*GET*` + indented code blocks round-trip |

## Further reading

- `http-client-improvement-plan.md` (repo root) — design notes and
  rationale for the v1 → v2 client shape, including what's
  intentionally out of scope (connection pooling, multipart, cookies,
  proxy/TLS config).
- `https://servirtium.dev` — upstream Servirtium project, including
  the canonical 16-step "starting a new implementation" guide and
  cross-language compatibility fixtures.
