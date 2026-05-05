/* aether_proxy_middleware.c — the proxy middleware glue.
 *
 * Flow per request:
 *   1. Path-prefix match. If no match, return 1 (continue chain).
 *   2. Body cap check; oversize → 502 "request body exceeds proxy cap".
 *   3. Optional cache lookup (GET/HEAD only). On hit → write cached
 *      response into `res`, return 0.
 *   4. LB pick. NULL → 503 "no upstream available".
 *   5. Build the outbound request via std.http.client:
 *        - method + (upstream_base + req->path[strip_prefix..])
 *        - copy inbound headers minus Hop-by-Hop (RFC 7230 §6.1)
 *        - copy inbound body (length-aware)
 *        - set timeout from pool->request_timeout_sec
 *        - add X-Forwarded-{For, Proto, Host}, Via
 *        - rewrite Host: to upstream when preserve_host=0
 *   6. Send. Classify error (transport / timeout / 5xx / 4xx).
 *   7. Copy upstream status + headers (minus Hop-by-Hop) + body
 *      onto `res`. Add X-Cache: MISS when cache is bound.
 *   8. Optionally store in cache.
 *   9. Decrement upstream inflight; record breaker outcome.
 *   10. Return 0 (short-circuit).
 *
 * Refuses requests with `Upgrade:` headers (WebSocket / h2 upstream)
 * with 502 + X-Aether-Proxy-Error: upgrade_unsupported. v2 follow-up.
 */

#include "aether_proxy_internal.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* std.http.client primitives, forward-declared with opaque types
 * to avoid the HttpRequest typedef collision between
 * aether_http.h (client) and aether_http_server.h (server). The
 * underlying ABI is unchanged; the type opaqueness here is purely
 * for the C compiler's name resolution. */
typedef struct HttpClientRequest HttpClientRequest;
typedef struct HttpClientResponse HttpClientResponse;
extern HttpClientRequest* http_request_raw(const char* method, const char* url);
extern int  http_request_set_header_raw(HttpClientRequest* req, const char* name, const char* value);
extern int  http_request_set_body_raw(HttpClientRequest* req, const char* body, int len, const char* content_type);
extern int  http_request_set_timeout_raw(HttpClientRequest* req, int seconds);
extern void http_request_free_raw(HttpClientRequest* req);
extern HttpClientResponse* http_send_raw(HttpClientRequest* req);
extern int  http_response_status(HttpClientResponse* response);
extern const char* http_response_body(HttpClientResponse* response);
extern const char* http_response_headers(HttpClientResponse* response);
extern const char* http_response_error(HttpClientResponse* response);
extern void http_response_free(HttpClientResponse* response);

/* ----- hop-by-hop headers (RFC 7230 §6.1) ----- */

static const char* HOP_HEADERS[] = {
    "Connection",
    "Keep-Alive",
    "Proxy-Authenticate",
    "Proxy-Authorization",
    "TE",
    "Trailer",
    "Transfer-Encoding",
    "Upgrade",
    "Proxy-Connection",   /* legacy */
    NULL
};

static int is_hop_by_hop(const char* name) {
    if (!name) return 0;
    for (const char** p = HOP_HEADERS; *p; p++) {
        if (strcasecmp(name, *p) == 0) return 1;
    }
    return 0;
}

/* ----- helpers ----- */

static const char* client_ip_for_xff(HttpRequest* req) {
    const char* xri = http_get_header(req, "X-Real-IP");
    if (xri && *xri) return xri;
    /* Best-effort fallback. The HTTP server doesn't currently
     * expose the connection's remote-address back to middleware
     * at a stable address, so we use "unknown" rather than
     * lying. Operators who care should register
     * middleware.use_real_ip first; that populates X-Real-IP. */
    return "unknown";
}

/* Append `value` to an existing comma-separated header value.
 * Returns a malloc'd string the caller frees. */
static char* append_csv(const char* existing, const char* value) {
    if (!existing || !*existing) return strdup(value ? value : "");
    size_t a = strlen(existing);
    size_t b = strlen(value);
    char* out = (char*)malloc(a + 2 + b + 1);
    if (!out) return NULL;
    memcpy(out, existing, a);
    out[a] = ',';
    out[a + 1] = ' ';
    memcpy(out + a + 2, value, b);
    out[a + 2 + b] = '\0';
    return out;
}

/* Build "<base_url><path>" with single slash boundary. Caller frees. */
static char* build_upstream_url(const char* base_url,
                                const char* path,
                                const char* query) {
    if (!base_url) return NULL;
    if (!path) path = "/";
    size_t bl = strlen(base_url);
    int chop = (bl > 0 && base_url[bl - 1] == '/' && path[0] == '/');
    size_t pl = strlen(path);
    size_t ql = (query && *query) ? strlen(query) + 1 : 0;  /* +1 for '?' */
    size_t out_len = (chop ? bl - 1 : bl) + pl + ql + 1;
    char* out = (char*)malloc(out_len);
    if (!out) return NULL;
    size_t pos = 0;
    if (chop) {
        memcpy(out + pos, base_url, bl - 1); pos += bl - 1;
    } else {
        memcpy(out + pos, base_url, bl); pos += bl;
    }
    memcpy(out + pos, path, pl); pos += pl;
    if (query && *query) {
        out[pos++] = '?';
        memcpy(out + pos, query, strlen(query)); pos += strlen(query);
    }
    out[pos] = '\0';
    return out;
}

/* Extract the host:port piece of a URL ("http://host:port/...") for
 * Host: rewriting. Returns malloc'd string, caller frees. NULL on
 * malformed. */
static char* extract_authority(const char* url) {
    if (!url) return NULL;
    const char* p = strstr(url, "://");
    if (!p) return NULL;
    p += 3;
    const char* end = p;
    while (*end && *end != '/' && *end != '?' && *end != '#') end++;
    size_t n = (size_t)(end - p);
    char* out = (char*)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, p, n);
    out[n] = '\0';
    return out;
}

/* ----- response writer ----- */

/* Apply a cache hit's snapshot to the inbound HttpServerResponse. */
static void serve_from_cache(HttpServerResponse* res,
                             AetherProxyCacheEntry* e) {
    http_response_set_status(res, e->status_code);
    /* Replay headers (minus hop-by-hop, just in case). */
    for (int i = 0; i < e->header_count; i++) {
        if (!is_hop_by_hop(e->header_keys[i])) {
            http_response_set_header(res, e->header_keys[i], e->header_values[i]);
        }
    }
    http_response_set_header(res, "X-Cache", "HIT");
    if (e->body && e->body_length > 0) {
        http_response_set_body_n(res, e->body, e->body_length);
    } else {
        http_response_set_body(res, "");
    }
}

/* ----- the middleware function itself ----- */

int aether_middleware_reverse_proxy(HttpRequest* req,
                                    HttpServerResponse* res,
                                    void* user_data) {
    AetherProxyOpts* opts = (AetherProxyOpts*)user_data;
    if (!opts || !opts->pool) return 1;  /* misconfigured — pass through */
    if (!req || !res) return 1;

    /* Path-prefix gate. Empty prefix or "/" matches everything. */
    const char* prefix = opts->path_prefix ? opts->path_prefix : "/";
    if (*prefix && strcmp(prefix, "/") != 0) {
        size_t pl = strlen(prefix);
        if (!req->path || strncmp(req->path, prefix, pl) != 0) {
            return 1;
        }
    }

    /* Refuse Upgrade-bearing requests — WebSocket / h2 upstream
     * support is v2 follow-up. The client expects an upgrade and
     * we'd half-perform it, so be explicit. */
    const char* upgrade = http_get_header(req, "Upgrade");
    if (upgrade && *upgrade) {
        http_response_set_status(res, 502);
        http_response_set_header(res, "X-Aether-Proxy-Error", "upgrade_unsupported");
        http_response_set_header(res, "Content-Type", "text/plain");
        http_response_set_body(res,
            "Upgrade headers are not forwarded by std.http.proxy v1.\n");
        return 0;
    }

    /* Body cap check. */
    if (req->body_length > (size_t)opts->max_body_bytes) {
        http_response_set_status(res, 502);
        http_response_set_header(res, "X-Aether-Proxy-Error", "request_body_too_large");
        http_response_set_header(res, "Content-Type", "text/plain");
        http_response_set_body(res, "request body exceeds proxy cap\n");
        return 0;
    }

    /* Strip the configured prefix from the path before forwarding.
     * "/api/users" with strip "/api" becomes "/users". When
     * path_prefix matches but strip is unset, the full path is
     * forwarded as-is. */
    const char* forward_path = req->path ? req->path : "/";
    if (opts->strip_path_prefix && *opts->strip_path_prefix) {
        size_t sl = strlen(opts->strip_path_prefix);
        if (strncmp(forward_path, opts->strip_path_prefix, sl) == 0) {
            forward_path = forward_path + sl;
            if (!*forward_path) forward_path = "/";
        }
    }

    /* ---- Cache lookup ---- */
    if (opts->cache &&
        req->method &&
        (strcmp(req->method, "GET") == 0 || strcmp(req->method, "HEAD") == 0)) {
        /* Build the lookup URL (base-less — just method+path so the
         * key is independent of which upstream serves it; caching is
         * pool-scoped, not upstream-scoped). */
        AetherProxyCacheEntry* hit = aether_proxy_cache_lookup(
            opts->cache, req->method, forward_path, req);
        if (hit) {
            serve_from_cache(res, hit);
            return 0;
        }
    }

    /* ---- Pick upstream ---- */
    AetherUpstream* u = aether_proxy_lb_pick(opts->pool, req);
    if (!u) {
        http_response_set_status(res, 503);
        http_response_set_header(res, "X-Aether-Proxy-Error", "no_upstream");
        http_response_set_header(res, "Retry-After", "1");
        http_response_set_header(res, "Content-Type", "text/plain");
        http_response_set_body(res,
            "no upstream available (every upstream is unhealthy or "
            "the breaker is open)\n");
        return 0;
    }

    /* ---- Build the outbound request ---- */
    char* upstream_url = build_upstream_url(u->base_url, forward_path,
                                            req->query_string);
    HttpClientRequest* outbound = upstream_url ?
        http_request_raw(req->method ? req->method : "GET", upstream_url) : NULL;

    if (!outbound) {
        free(upstream_url);
        aether_proxy_inflight_dec(u);
        http_response_set_status(res, 502);
        http_response_set_header(res, "X-Aether-Proxy-Error", "request_alloc_failed");
        http_response_set_header(res, "Content-Type", "text/plain");
        http_response_set_body(res, "proxy request allocation failed\n");
        return 0;
    }

    /* Per-request timeout from the pool config. */
    if (opts->pool->request_timeout_sec > 0) {
        http_request_set_timeout_raw(outbound, opts->pool->request_timeout_sec);
    }

    /* Forward inbound headers minus hop-by-hop. */
    for (int i = 0; i < req->header_count; i++) {
        const char* k = req->header_keys[i];
        const char* v = req->header_values[i];
        if (!k || is_hop_by_hop(k)) continue;
        /* Skip Host: — we'll set our own afterwards. */
        if (strcasecmp(k, "Host") == 0) continue;
        http_request_set_header_raw(outbound, k, v ? v : "");
    }

    /* Host: rewrite. */
    if (opts->preserve_host) {
        const char* h = http_get_header(req, "Host");
        if (h && *h) http_request_set_header_raw(outbound, "Host", h);
    } else {
        char* authority = extract_authority(u->base_url);
        if (authority) {
            http_request_set_header_raw(outbound, "Host", authority);
            free(authority);
        }
    }

    /* X-Forwarded-* injection. */
    if (opts->add_xff) {
        const char* prior = http_get_header(req, "X-Forwarded-For");
        const char* client = client_ip_for_xff(req);
        char* xff = append_csv(prior, client);
        if (xff) {
            http_request_set_header_raw(outbound, "X-Forwarded-For", xff);
            free(xff);
        }
    }
    if (opts->add_xfp) {
        /* The server doesn't expose tls_enabled to middleware
         * directly here; the X-Forwarded-Proto we set is best-
         * effort based on the Host: header convention used
         * upstream. Plain http server → "http". */
        http_request_set_header_raw(outbound, "X-Forwarded-Proto", "http");
    }
    if (opts->add_xfh) {
        const char* h = http_get_header(req, "Host");
        if (h && *h) http_request_set_header_raw(outbound, "X-Forwarded-Host", h);
    }
    /* Via: 1.1 aether-proxy (RFC 7230 §5.7.1). */
    {
        const char* prior_via = http_get_header(req, "Via");
        char* via = append_csv(prior_via, "1.1 aether-proxy");
        if (via) {
            http_request_set_header_raw(outbound, "Via", via);
            free(via);
        }
    }

    /* Forward the request body verbatim. */
    if (req->body && req->body_length > 0) {
        const char* ct = http_get_header(req, "Content-Type");
        http_request_set_body_raw(outbound,
                                  req->body,
                                  (int)req->body_length,
                                  ct ? ct : "application/octet-stream");
    }

    /* ---- Send ---- */
    HttpClientResponse* upstream = http_send_raw(outbound);
    http_request_free_raw(outbound);
    free(upstream_url);

    if (!upstream) {
        aether_proxy_breaker_record(opts->pool, u, 0);
        aether_proxy_inflight_dec(u);
        http_response_set_status(res, 502);
        http_response_set_header(res, "X-Aether-Proxy-Error", "send_alloc_failed");
        http_response_set_header(res, "Content-Type", "text/plain");
        http_response_set_body(res, "proxy upstream call alloc failed\n");
        return 0;
    }

    const char* err = http_response_error(upstream);
    int upstream_status = http_response_status(upstream);

    if (err && *err) {
        /* Transport-class failure. Distinguish timeout from generic
         * connect/DNS by inspecting the error string the client
         * produces (it includes "timeout" / "timed out" tokens). */
        int is_timeout = strstr(err, "timeout") != NULL ||
                         strstr(err, "timed out") != NULL;
        int is_oversize_resp = strstr(err, "exceeds") != NULL;
        aether_proxy_breaker_record(opts->pool, u, 0);
        aether_proxy_inflight_dec(u);
        http_response_set_status(res, is_timeout ? 504 : 502);
        http_response_set_header(res, "X-Aether-Proxy-Error",
            is_timeout ? "upstream_timeout" :
            is_oversize_resp ? "response_too_large" : "upstream_transport");
        http_response_set_header(res, "Content-Type", "text/plain");
        http_response_set_body(res, err);
        http_response_free(upstream);
        return 0;
    }

    /* Status reached us. Record breaker outcome (ok = 2xx/3xx/4xx,
     * !ok = 5xx). 4xx is client error, not upstream fault. */
    int classified_ok = (upstream_status >= 200 && upstream_status < 500);
    aether_proxy_breaker_record(opts->pool, u, classified_ok);
    aether_proxy_inflight_dec(u);

    /* ---- Copy upstream response onto `res` ---- */
    http_response_set_status(res, upstream_status);

    /* Headers — parse the raw block, drop hop-by-hop. */
    const char* headers_block = http_response_headers(upstream);
    int seen_content_type = 0;
    if (headers_block) {
        const char* p = headers_block;
        while (*p) {
            const char* eol = strstr(p, "\r\n");
            if (!eol) break;
            const char* colon = strchr(p, ':');
            if (colon && colon < eol) {
                size_t kl = (size_t)(colon - p);
                char keybuf[128];
                if (kl < sizeof(keybuf)) {
                    memcpy(keybuf, p, kl);
                    keybuf[kl] = '\0';
                    if (!is_hop_by_hop(keybuf)) {
                        const char* v = colon + 1;
                        while (v < eol && (*v == ' ' || *v == '\t')) v++;
                        size_t vl = (size_t)(eol - v);
                        char* val = (char*)malloc(vl + 1);
                        if (val) {
                            memcpy(val, v, vl);
                            val[vl] = '\0';
                            http_response_set_header(res, keybuf, val);
                            if (strcasecmp(keybuf, "Content-Type") == 0) seen_content_type = 1;
                            free(val);
                        }
                    }
                }
            }
            p = eol + 2;
        }
    }
    if (!seen_content_type) {
        http_response_set_header(res, "Content-Type", "application/octet-stream");
    }
    if (opts->cache) {
        http_response_set_header(res, "X-Cache", "MISS");
    }

    /* Body — length-aware (binary-safe). The client wrapper exposes
     * the body as a NUL-terminated cstr, so we use strlen as a
     * reasonable upper bound when an explicit length isn't visible
     * here. The HttpResponse internally holds AetherString length;
     * the public accessor doesn't expose it, so this is a v1
     * limitation: bodies with embedded NULs may be truncated.
     * Caching skips bodies past max_body_bytes anyway. */
    const char* body = http_response_body(upstream);
    int body_length = body ? (int)strlen(body) : 0;
    if (body_length > opts->max_body_bytes) {
        http_response_set_status(res, 502);
        http_response_set_header(res, "X-Aether-Proxy-Error", "response_too_large");
        http_response_set_body(res, "upstream response exceeds proxy cap\n");
        http_response_free(upstream);
        return 0;
    }
    if (body && body_length > 0) {
        http_response_set_body_n(res, body, body_length);
    } else {
        http_response_set_body(res, "");
    }

    /* ---- Cache store ---- */
    if (opts->cache && headers_block && req->method &&
        (strcmp(req->method, "GET") == 0 || strcmp(req->method, "HEAD") == 0)) {
        aether_proxy_cache_store(opts->cache, req->method, forward_path, req,
                                 upstream_status, headers_block,
                                 body, body_length);
    }

    http_response_free(upstream);
    return 0;
}

/* ----- install ----- */

const char* aether_proxy_use_reverse_proxy(HttpServer* server,
                                           const char* path_prefix,
                                           AetherProxyPool* pool,
                                           AetherProxyOpts* opts) {
    if (!server) return "server is null";
    if (!pool)   return "pool is null";
    if (!opts)   return "opts is null";
    if (!path_prefix || *path_prefix != '/') return "path_prefix must start with /";

    /* Bind the pool to opts (refcount-incremented). The opts are
     * the user_data that the middleware receives. */
    aether_proxy_pool_retain(pool);
    if (opts->pool && opts->pool != pool) {
        /* Caller is rebinding. Release the prior pool first. */
        aether_proxy_pool_free(opts->pool);
    }
    opts->pool = pool;

    free(opts->path_prefix);
    opts->path_prefix = strdup(path_prefix);
    if (!opts->path_prefix) {
        aether_proxy_pool_free(pool);  /* roll back retain */
        opts->pool = NULL;
        return "out of memory";
    }

    http_server_use_middleware(server, aether_middleware_reverse_proxy, opts);
    return "";
}

const char* aether_proxy_use_simple_proxy(HttpServer* server,
                                          const char* path_prefix,
                                          const char* upstream_url,
                                          int request_timeout_sec) {
    if (!server) return "server is null";
    if (!path_prefix || *path_prefix != '/') return "path_prefix must start with /";
    if (!upstream_url || !*upstream_url) return "upstream_url is empty";

    AetherProxyPool* pool = aether_proxy_pool_new(
        AETHER_PROXY_LB_ROUND_ROBIN,
        request_timeout_sec, 0, 0);
    if (!pool) return "pool allocation failed";

    const char* err = aether_proxy_upstream_add(pool, upstream_url, 1);
    if (err && *err) {
        aether_proxy_pool_free(pool);
        return err;
    }

    AetherProxyOpts* opts = aether_proxy_opts_new();
    if (!opts) {
        aether_proxy_pool_free(pool);
        return "opts allocation failed";
    }

    err = aether_proxy_use_reverse_proxy(server, path_prefix, pool, opts);
    /* The opts owns the pool refcount now via use_reverse_proxy.
     * We drop our local one. */
    aether_proxy_pool_free(pool);
    return err;
}
