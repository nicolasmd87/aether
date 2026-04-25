#ifndef AETHER_HTTP_H
#define AETHER_HTTP_H

#include "../string/aether_string.h"

typedef struct {
    int status_code;
    AetherString* body;
    AetherString* headers;
    AetherString* error;
} HttpResponse;

// ---------------------------------------------------------------------------
// v1 one-liners — present from day one, kept callable for backward compat.
// Internally re-implemented as thin wrappers over the v2 builder below.
// They preserve the original behaviour of "no per-request timeout — block
// forever" by handing the v2 path a 0 timeout (the explicit "no timeout"
// sentinel).
// ---------------------------------------------------------------------------

HttpResponse* http_get_raw(const char* url);
HttpResponse* http_post_raw(const char* url, const char* body, const char* content_type);
HttpResponse* http_put_raw(const char* url, const char* body, const char* content_type);
HttpResponse* http_delete_raw(const char* url);
void http_response_free(HttpResponse* response);

// Response field accessors. All are NULL-safe: passing NULL or a freed
// response returns a sensible default (0 or "") rather than crashing.
// Returned const char* pointers are owned by the response struct and
// valid until http_response_free() is called.
int http_response_status(HttpResponse* response);
const char* http_response_body(HttpResponse* response);
const char* http_response_headers(HttpResponse* response);
const char* http_response_error(HttpResponse* response);

// Convenience: returns 1 if the request succeeded (no transport error
// AND HTTP status is in the 2xx range), 0 otherwise. Use this for the
// common "did it work?" check instead of chaining error/status calls.
int http_response_ok(HttpResponse* response);

// Legacy accessor aliases kept for callers that used the older
// `_code` / `_str` names. Prefer the short names above.
int http_response_status_code(HttpResponse* response);
const char* http_response_body_str(HttpResponse* response);
const char* http_response_headers_str(HttpResponse* response);

// ---------------------------------------------------------------------------
// v2 client — request builder, full response access.
//
// Build a request with method + URL + headers + optional body + explicit
// timeout, fire it, get back the full HttpResponse with status / body /
// raw header block, plus a typed case-insensitive header lookup. The
// caller drives status interpretation — non-2xx is no longer collapsed
// to an error; only transport-level failures (DNS, connect, TLS handshake,
// timeout) populate response->error.
//
// Lifecycle:
//   req = http_request_raw("GET", "https://example.com/api/users");
//   http_request_set_header_raw(req, "Authorization", "Bearer ...");
//   http_request_set_timeout_raw(req, 30);   // seconds; 0 = block forever
//   resp = http_send_raw(req);
//   http_request_free_raw(req);
//   /* read resp via the existing http_response_* accessors */
//   http_response_free(resp);
//
// Naming: every v2 client extern carries an `http_request_` /
// `http_send_` / `http_response_header_` prefix that doesn't collide
// with the existing http_response_* accessors above OR with the
// server-side surface in aether_http_server.c (`http_server_*`,
// `http_request_body`, etc. — those stay flat for tinyweb-compat).
// ---------------------------------------------------------------------------

typedef struct HttpRequest HttpRequest;  /* opaque */

HttpRequest* http_request_raw(const char* method, const char* url);

// Returns 0 on success, non-zero on failure (NULL request, OOM,
// invalid header). Header names are stored verbatim and emitted as
// `Name: value\r\n`; built-in headers the wrapper would set itself
// (Host, Content-Length) are overridden by an explicit set_header
// with the same name. Multiple values for one name produce multiple
// `Name: value` lines (RFC 7230 §3.2.2 conformant).
int http_request_set_header_raw(HttpRequest* req, const char* name, const char* value);

// Set the request body. `len` is explicit so binary payloads with
// embedded NULs survive. content_type may be NULL (defaults to
// application/x-www-form-urlencoded for backward compat with v1).
// Replaces any prior body.
int http_request_set_body_raw(HttpRequest* req, const char* body, int len, const char* content_type);

// Per-request timeout in seconds. 0 means "no timeout — block forever"
// (preserves v1's behaviour). Negative values are an error.
int http_request_set_timeout_raw(HttpRequest* req, int seconds);

void http_request_free_raw(HttpRequest* req);

// Fire the configured request. Returns an HttpResponse on success
// (caller frees with http_response_free), NULL only on out-of-memory
// failures BEFORE the request is sent. Transport failures (DNS,
// connect, TLS, timeout) return a non-NULL response with the failure
// recorded in response->error and status_code == 0.
HttpResponse* http_send_raw(HttpRequest* req);

// Case-insensitive response-header lookup. Returns "" when the header
// isn't present. The pointer is owned by the response and valid until
// http_response_free(). Multiple values for one header are joined
// with ", " (RFC 7230 §3.2.2 conformant).
const char* http_response_header_raw(HttpResponse* response, const char* name);

#endif

