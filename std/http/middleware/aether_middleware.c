// std.http.middleware — pre-handler middleware primitives.
// See aether_middleware.h for the API contract.
//
// Each middleware ships with:
//   - An options struct (config) the user constructs once at startup.
//   - A C function with the existing HttpMiddleware signature
//     `int(req, res, user_data) -> int`, registered via
//     http_server_use_middleware. Returns 1 to continue the chain,
//     0 to short-circuit.
//
// The hot path is straight C function pointers — no closure
// indirection, no Aether-side dispatch. Aether-side factory
// wrappers in std/http/middleware/module.ae allocate the user_data
// and register the middleware with the server.
#include "aether_middleware.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <pthread.h>

// Borrow string accessors from the existing http surface.
extern const char* http_get_header(HttpRequest*, const char*);
extern void        http_response_set_status(HttpServerResponse*, int);
extern void        http_response_set_header(HttpServerResponse*, const char*, const char*);
extern void        http_response_set_body  (HttpServerResponse*, const char*);
extern const char* http_request_method(HttpRequest*);
extern const char* http_request_path  (HttpRequest*);

// -----------------------------------------------------------------
// CORS
// -----------------------------------------------------------------
struct AetherCorsOpts {
    char* allow_origin;       /* NULL/"" -> don't emit */
    char* allow_methods;
    char* allow_headers;
    int   allow_credentials;
    int   max_age_seconds;    /* <= 0 -> don't emit Max-Age */
};

static char* dup_or_null(const char* s) {
    if (!s || !*s) return NULL;
    return strdup(s);
}

AetherCorsOpts* aether_cors_opts_new(const char* allow_origin,
                                     const char* allow_methods,
                                     const char* allow_headers,
                                     int allow_credentials,
                                     int max_age_seconds) {
    AetherCorsOpts* o = (AetherCorsOpts*)calloc(1, sizeof(AetherCorsOpts));
    if (!o) return NULL;
    o->allow_origin      = dup_or_null(allow_origin);
    o->allow_methods     = dup_or_null(allow_methods);
    o->allow_headers     = dup_or_null(allow_headers);
    o->allow_credentials = allow_credentials ? 1 : 0;
    o->max_age_seconds   = max_age_seconds;
    return o;
}

void aether_cors_opts_free(AetherCorsOpts* o) {
    if (!o) return;
    free(o->allow_origin);
    free(o->allow_methods);
    free(o->allow_headers);
    free(o);
}

int aether_middleware_cors(HttpRequest* req, HttpServerResponse* res, void* user_data) {
    AetherCorsOpts* o = (AetherCorsOpts*)user_data;
    if (!o) return 1;

    /* CORS headers go on every response, not just preflight. */
    if (o->allow_origin)  http_response_set_header(res, "Access-Control-Allow-Origin",  o->allow_origin);
    if (o->allow_methods) http_response_set_header(res, "Access-Control-Allow-Methods", o->allow_methods);
    if (o->allow_headers) http_response_set_header(res, "Access-Control-Allow-Headers", o->allow_headers);
    if (o->allow_credentials)
        http_response_set_header(res, "Access-Control-Allow-Credentials", "true");

    /* Preflight OPTIONS — answer with 204 + Max-Age and short-circuit
     * the chain. The route handler never runs for preflight requests. */
    const char* method = http_request_method(req);
    if (method && strcasecmp(method, "OPTIONS") == 0) {
        if (o->max_age_seconds > 0) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", o->max_age_seconds);
            http_response_set_header(res, "Access-Control-Max-Age", buf);
        }
        http_response_set_status(res, 204);
        http_response_set_body(res, "");
        return 0;  /* short-circuit */
    }
    return 1;  /* continue chain */
}

// -----------------------------------------------------------------
// Basic Authentication
// -----------------------------------------------------------------
struct AetherBasicAuthOpts {
    char* realm;
    AetherBasicAuthVerifier verify;
    void* verifier_user_data;
};

AetherBasicAuthOpts* aether_basic_auth_opts_new(const char* realm,
                                                AetherBasicAuthVerifier verify,
                                                void* verifier_user_data) {
    if (!verify) return NULL;
    AetherBasicAuthOpts* o = (AetherBasicAuthOpts*)calloc(1, sizeof(AetherBasicAuthOpts));
    if (!o) return NULL;
    o->realm = realm && *realm ? strdup(realm) : strdup("Restricted");
    o->verify = verify;
    o->verifier_user_data = verifier_user_data;
    return o;
}

void aether_basic_auth_opts_free(AetherBasicAuthOpts* o) {
    if (!o) return;
    free(o->realm);
    free(o);
}

/* RFC 4648 base64 alphabet. Decoding tolerant of whitespace + padding. */
static int b64_decode_char(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

/* Decode a base64 string. Caller frees. Returns NULL on malformed
 * input. *out_len receives the decoded byte count. */
static char* b64_decode(const char* src, size_t* out_len) {
    size_t src_len = strlen(src);
    char* out = (char*)malloc(src_len + 1);
    if (!out) return NULL;
    int bits = 0, val = 0;
    size_t out_pos = 0;
    for (size_t i = 0; i < src_len; i++) {
        int c = (unsigned char)src[i];
        if (c == '=' || c == '\r' || c == '\n' || c == ' ' || c == '\t') continue;
        int d = b64_decode_char(c);
        if (d < 0) { free(out); return NULL; }
        val = (val << 6) | d;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out[out_pos++] = (char)((val >> bits) & 0xff);
        }
    }
    out[out_pos] = '\0';
    *out_len = out_pos;
    return out;
}

int aether_middleware_basic_auth(HttpRequest* req, HttpServerResponse* res, void* user_data) {
    AetherBasicAuthOpts* o = (AetherBasicAuthOpts*)user_data;
    if (!o || !o->verify) return 1;

    const char* auth = http_get_header(req, "Authorization");
    if (!auth || strncasecmp(auth, "Basic ", 6) != 0) {
        char hdr[256];
        snprintf(hdr, sizeof(hdr), "Basic realm=\"%s\"", o->realm);
        http_response_set_header(res, "WWW-Authenticate", hdr);
        http_response_set_status(res, 401);
        http_response_set_body(res, "Unauthorized");
        return 0;
    }

    const char* b64 = auth + 6;
    while (*b64 == ' ' || *b64 == '\t') b64++;

    size_t dec_len = 0;
    char* decoded = b64_decode(b64, &dec_len);
    if (!decoded) {
        http_response_set_status(res, 400);
        http_response_set_body(res, "Bad Authorization header");
        return 0;
    }
    char* sep = strchr(decoded, ':');
    if (!sep) {
        free(decoded);
        http_response_set_status(res, 400);
        http_response_set_body(res, "Bad Authorization payload");
        return 0;
    }
    *sep = '\0';
    const char* user = decoded;
    const char* pass = sep + 1;

    int ok = o->verify(user, pass, o->verifier_user_data);
    free(decoded);

    if (!ok) {
        char hdr[256];
        snprintf(hdr, sizeof(hdr), "Basic realm=\"%s\"", o->realm);
        http_response_set_header(res, "WWW-Authenticate", hdr);
        http_response_set_status(res, 401);
        http_response_set_body(res, "Unauthorized");
        return 0;
    }
    return 1;  /* authorised — continue */
}

// -----------------------------------------------------------------
// Token-bucket rate limiter (per-client-IP)
// -----------------------------------------------------------------
typedef struct RateBucket {
    char* key;          /* X-Forwarded-For value or fallback */
    double tokens;      /* current bucket level */
    long   last_refill; /* monotonic ms */
    struct RateBucket* next;
} RateBucket;

#define RATE_TABLE_SIZE 256

struct AetherRateLimitOpts {
    int max_requests;
    int window_ms;
    pthread_mutex_t lock;
    RateBucket* table[RATE_TABLE_SIZE];
};

static long monotonic_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static unsigned int rate_hash(const char* s) {
    unsigned int h = 5381;
    while (*s) { h = ((h << 5) + h) + (unsigned char)*s; s++; }
    return h;
}

AetherRateLimitOpts* aether_rate_limit_opts_new(int max_requests, int window_ms) {
    if (max_requests <= 0 || window_ms <= 0) return NULL;
    AetherRateLimitOpts* o = (AetherRateLimitOpts*)calloc(1, sizeof(AetherRateLimitOpts));
    if (!o) return NULL;
    o->max_requests = max_requests;
    o->window_ms = window_ms;
    pthread_mutex_init(&o->lock, NULL);
    return o;
}

void aether_rate_limit_opts_free(AetherRateLimitOpts* o) {
    if (!o) return;
    for (int i = 0; i < RATE_TABLE_SIZE; i++) {
        RateBucket* b = o->table[i];
        while (b) {
            RateBucket* next = b->next;
            free(b->key);
            free(b);
            b = next;
        }
    }
    pthread_mutex_destroy(&o->lock);
    free(o);
}

int aether_middleware_rate_limit(HttpRequest* req, HttpServerResponse* res, void* user_data) {
    AetherRateLimitOpts* o = (AetherRateLimitOpts*)user_data;
    if (!o) return 1;

    /* Best-effort client identifier: prefer X-Forwarded-For (proxy
     * deployments), then X-Real-IP, then "anonymous". The HTTP
     * server doesn't currently expose the peer's sockaddr through
     * HttpRequest; this is an acceptable approximation for a
     * front-by-default token bucket. */
    const char* key = http_get_header(req, "X-Forwarded-For");
    if (!key) key = http_get_header(req, "X-Real-IP");
    if (!key) key = "anonymous";

    /* Trim to the first comma (X-Forwarded-For can be a chain). */
    char keybuf[128];
    size_t klen = 0;
    while (*key && *key != ',' && klen + 1 < sizeof(keybuf)) {
        if (*key != ' ' && *key != '\t') keybuf[klen++] = *key;
        key++;
    }
    keybuf[klen] = '\0';

    unsigned int slot = rate_hash(keybuf) % RATE_TABLE_SIZE;
    long now = monotonic_ms();

    pthread_mutex_lock(&o->lock);
    RateBucket* bucket = o->table[slot];
    RateBucket* prev = NULL;
    while (bucket && strcmp(bucket->key, keybuf) != 0) {
        prev = bucket;
        bucket = bucket->next;
    }
    if (!bucket) {
        bucket = (RateBucket*)calloc(1, sizeof(RateBucket));
        if (!bucket) {
            pthread_mutex_unlock(&o->lock);
            return 1;  /* fail open on OOM */
        }
        bucket->key = strdup(keybuf);
        bucket->tokens = (double)o->max_requests;
        bucket->last_refill = now;
        if (prev) prev->next = bucket;
        else o->table[slot] = bucket;
    }

    /* Continuous refill: rate = max_requests / window_ms tokens per ms. */
    long elapsed = now - bucket->last_refill;
    if (elapsed > 0) {
        double refill = (double)elapsed * (double)o->max_requests / (double)o->window_ms;
        bucket->tokens += refill;
        if (bucket->tokens > (double)o->max_requests) {
            bucket->tokens = (double)o->max_requests;
        }
        bucket->last_refill = now;
    }

    if (bucket->tokens < 1.0) {
        /* Out of tokens — compute Retry-After in seconds. */
        double need = 1.0 - bucket->tokens;
        double per_ms = (double)o->max_requests / (double)o->window_ms;
        int retry_sec = (int)(need / per_ms / 1000.0) + 1;
        pthread_mutex_unlock(&o->lock);

        char buf[32];
        snprintf(buf, sizeof(buf), "%d", retry_sec);
        http_response_set_header(res, "Retry-After", buf);
        http_response_set_status(res, 429);
        http_response_set_body(res, "Too Many Requests");
        return 0;
    }
    bucket->tokens -= 1.0;
    pthread_mutex_unlock(&o->lock);
    return 1;
}

// -----------------------------------------------------------------
// Virtual host
// -----------------------------------------------------------------
struct AetherVhostMap {
    char** hosts;
    int    count;
    int    cap;
    int    fallback_status;  /* 0 -> use 404 */
};

AetherVhostMap* aether_vhost_map_new(int fallback_status) {
    AetherVhostMap* m = (AetherVhostMap*)calloc(1, sizeof(AetherVhostMap));
    if (!m) return NULL;
    m->fallback_status = fallback_status > 0 ? fallback_status : 404;
    return m;
}

void aether_vhost_map_free(AetherVhostMap* m) {
    if (!m) return;
    for (int i = 0; i < m->count; i++) free(m->hosts[i]);
    free(m->hosts);
    free(m);
}

int aether_vhost_register_host(AetherVhostMap* m, const char* host) {
    if (!m || !host || !*host) return -1;
    if (m->count >= m->cap) {
        int new_cap = m->cap > 0 ? m->cap * 2 : 4;
        char** nh = (char**)realloc(m->hosts, new_cap * sizeof(char*));
        if (!nh) return -1;
        m->hosts = nh;
        m->cap = new_cap;
    }
    m->hosts[m->count++] = strdup(host);
    return 0;
}

int aether_middleware_vhost(HttpRequest* req, HttpServerResponse* res, void* user_data) {
    AetherVhostMap* m = (AetherVhostMap*)user_data;
    if (!m) return 1;
    const char* host_hdr = http_get_header(req, "Host");
    if (!host_hdr) host_hdr = "";

    /* Strip the optional :port suffix for the comparison. */
    char host[256];
    size_t hlen = 0;
    while (*host_hdr && *host_hdr != ':' && hlen + 1 < sizeof(host)) {
        host[hlen++] = (char)tolower((unsigned char)*host_hdr++);
    }
    host[hlen] = '\0';

    for (int i = 0; i < m->count; i++) {
        if (strcasecmp(m->hosts[i], host) == 0) return 1;  /* allowed */
    }

    http_response_set_status(res, m->fallback_status);
    http_response_set_body(res, "Unknown host");
    return 0;
}
