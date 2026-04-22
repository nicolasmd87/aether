#include "aether_http.h"
#include "../../runtime/config/aether_optimization_config.h"

#if !AETHER_HAS_NETWORKING
HttpResponse* http_get_raw(const char* u) { (void)u; return NULL; }
HttpResponse* http_post_raw(const char* u, const char* b, const char* c) { (void)u; (void)b; (void)c; return NULL; }
HttpResponse* http_put_raw(const char* u, const char* b, const char* c) { (void)u; (void)b; (void)c; return NULL; }
HttpResponse* http_delete_raw(const char* u) { (void)u; return NULL; }
void http_response_free(HttpResponse* r) { (void)r; }
int http_response_status(HttpResponse* r) { (void)r; return 0; }
const char* http_response_body(HttpResponse* r) { (void)r; return ""; }
const char* http_response_headers(HttpResponse* r) { (void)r; return ""; }
const char* http_response_error(HttpResponse* r) { (void)r; return "networking disabled at build time"; }
int http_response_ok(HttpResponse* r) { (void)r; return 0; }
#else

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #ifdef _MSC_VER
        #pragma comment(lib, "ws2_32.lib")
    #endif
    #define close closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <arpa/inet.h>
#endif

#ifdef AETHER_HAS_OPENSSL
    #include <openssl/ssl.h>
    #include <openssl/err.h>
    #include <openssl/x509v3.h>
#endif

static int http_initialized = 0;

static void http_init(void) {
    if (http_initialized) return;
    #ifdef _WIN32
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
    #endif
    http_initialized = 1;
}

// -----------------------------------------------------------------
// URL parsing
// -----------------------------------------------------------------

// Parse `url` into (host, port, path, use_tls).
//   https://foo.example/bar  →  host="foo.example" port=443 path="/bar" use_tls=1
//   http://foo:8080/         →  host="foo"         port=8080 path="/"   use_tls=0
// Returns 1 on success, 0 on malformed input.
static int parse_url(const char* url, char* host, size_t host_size,
                     int* port, char* path, size_t path_size, int* use_tls) {
    if (!url || !host || !port || !path || !use_tls ||
        host_size == 0 || path_size == 0) return 0;

    const char* start;
    *use_tls = 0;

    if (strncmp(url, "http://", 7) == 0) {
        start = url + 7;
        *port = 80;
    } else if (strncmp(url, "https://", 8) == 0) {
        start = url + 8;
        *port = 443;
        *use_tls = 1;
    } else {
        start = url;
        *port = 80;
    }

    const char* slash = strchr(start, '/');
    const char* colon = strchr(start, ':');

    if (colon && (!slash || colon < slash)) {
        size_t host_len = colon - start;
        if (host_len >= host_size) host_len = host_size - 1;
        memcpy(host, start, host_len);
        host[host_len] = '\0';
        *port = atoi(colon + 1);
        if (slash) {
            snprintf(path, path_size, "%s", slash);
        } else {
            snprintf(path, path_size, "/");
        }
    } else if (slash) {
        size_t host_len = slash - start;
        if (host_len >= host_size) host_len = host_size - 1;
        memcpy(host, start, host_len);
        host[host_len] = '\0';
        snprintf(path, path_size, "%s", slash);
    } else {
        snprintf(host, host_size, "%s", start);
        snprintf(path, path_size, "/");
    }

    return 1;
}

// -----------------------------------------------------------------
// OpenSSL context: lazy init, shared across all HTTPS calls
// -----------------------------------------------------------------

#ifdef AETHER_HAS_OPENSSL

static _Atomic(SSL_CTX*) g_ssl_ctx;

// Get (or lazily create) the shared SSL_CTX. Benign race on first call —
// compare-exchange ensures at most one SSL_CTX is installed even if two
// threads reach here simultaneously. Returns NULL on OpenSSL error.
static SSL_CTX* get_ssl_ctx(void) {
    SSL_CTX* ctx = atomic_load(&g_ssl_ctx);
    if (ctx) return ctx;

    ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) return NULL;
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
    // Load the system trust store; fall back to OpenSSL's default search paths.
    SSL_CTX_set_default_verify_paths(ctx);

    SSL_CTX* expected = NULL;
    if (!atomic_compare_exchange_strong(&g_ssl_ctx, &expected, ctx)) {
        SSL_CTX_free(ctx);
        ctx = expected;
    }
    return ctx;
}

// Flush OpenSSL's error queue into a newly allocated string. Used only
// on error paths to surface the underlying OpenSSL reason.
static char* ssl_err_string(const char* prefix) {
    unsigned long err = ERR_get_error();
    const char* detail = err ? ERR_reason_error_string(err) : NULL;
    size_t plen = strlen(prefix);
    size_t dlen = detail ? strlen(detail) : 0;
    char* msg = (char*)malloc(plen + (dlen ? dlen + 3 : 0) + 1);
    if (!msg) return NULL;
    memcpy(msg, prefix, plen);
    if (dlen) {
        memcpy(msg + plen, ": ", 2);
        memcpy(msg + plen + 2, detail, dlen);
        msg[plen + 2 + dlen] = '\0';
    } else {
        msg[plen] = '\0';
    }
    return msg;
}

#endif // AETHER_HAS_OPENSSL

// -----------------------------------------------------------------
// Transport abstraction
//
// `Transport` wraps either a raw socket or an SSL* connection so the
// request/response loop doesn't need to branch on protocol everywhere.
// send/recv callbacks match the BSD socket signatures so plaintext
// paths can use them as-is.
// -----------------------------------------------------------------

typedef struct {
    int sockfd;
#ifdef AETHER_HAS_OPENSSL
    SSL* ssl;
#endif
} Transport;

static int transport_send(Transport* t, const void* buf, int len) {
#ifdef AETHER_HAS_OPENSSL
    if (t->ssl) return SSL_write(t->ssl, buf, len);
#endif
    return (int)send(t->sockfd, buf, len, 0);
}

static int transport_recv(Transport* t, void* buf, int len) {
#ifdef AETHER_HAS_OPENSSL
    if (t->ssl) return SSL_read(t->ssl, buf, len);
#endif
    return (int)recv(t->sockfd, buf, len, 0);
}

static void transport_close(Transport* t) {
#ifdef AETHER_HAS_OPENSSL
    if (t->ssl) {
        SSL_shutdown(t->ssl);
        SSL_free(t->ssl);
        t->ssl = NULL;
    }
#endif
    if (t->sockfd >= 0) {
        close(t->sockfd);
        t->sockfd = -1;
    }
}

// -----------------------------------------------------------------
// Core request
// -----------------------------------------------------------------

static HttpResponse* http_request(const char* method, const char* url,
                                  const char* body, const char* content_type) {
    http_init();

    HttpResponse* response = (HttpResponse*)malloc(sizeof(HttpResponse));
    if (!response) return NULL;
    response->status_code = 0;
    response->body = NULL;
    response->headers = NULL;
    response->error = NULL;

    char host[256];
    char path[1024];
    int port;
    int use_tls;

    if (!parse_url(url, host, sizeof(host), &port, path, sizeof(path), &use_tls)) {
        response->error = string_new("malformed URL");
        return response;
    }

    if (use_tls) {
#ifndef AETHER_HAS_OPENSSL
        response->error = string_new("HTTPS requested but the build has no OpenSSL support (rebuild with OpenSSL installed)");
        return response;
#endif
    }

    struct hostent* server = gethostbyname(host);
    if (!server) {
        response->error = string_new("could not resolve host");
        return response;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        response->error = string_new("could not create socket");
        return response;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_port = htons(port);

    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sockfd);
        response->error = string_new("connection failed");
        return response;
    }

    Transport t;
    t.sockfd = sockfd;
#ifdef AETHER_HAS_OPENSSL
    t.ssl = NULL;

    if (use_tls) {
        SSL_CTX* ctx = get_ssl_ctx();
        if (!ctx) {
            close(sockfd);
            char* msg = ssl_err_string("TLS context init failed");
            response->error = string_new(msg ? msg : "TLS context init failed");
            free(msg);
            return response;
        }

        SSL* ssl = SSL_new(ctx);
        if (!ssl) {
            close(sockfd);
            char* msg = ssl_err_string("SSL_new failed");
            response->error = string_new(msg ? msg : "SSL_new failed");
            free(msg);
            return response;
        }

        // SNI: server-name indication so virtual-hosted TLS services
        // return the right cert.
        SSL_set_tlsext_host_name(ssl, host);

        // Verify the cert's CN/SAN matches the hostname we connected to.
        X509_VERIFY_PARAM* vpm = SSL_get0_param(ssl);
        X509_VERIFY_PARAM_set_hostflags(vpm, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
        X509_VERIFY_PARAM_set1_host(vpm, host, 0);

        SSL_set_fd(ssl, sockfd);
        int connect_result = SSL_connect(ssl);
        if (connect_result != 1) {
            int ssl_err = SSL_get_error(ssl, connect_result);
            (void)ssl_err;
            SSL_free(ssl);
            close(sockfd);
            char* msg = ssl_err_string("TLS handshake failed");
            response->error = string_new(msg ? msg : "TLS handshake failed");
            free(msg);
            return response;
        }

        t.ssl = ssl;
    }
#endif

    char request[4096];
    int request_len = 0;

    if (body && strlen(body) > 0) {
        request_len = snprintf(request, sizeof(request),
            "%s %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            method, path, host,
            content_type ? content_type : "application/x-www-form-urlencoded",
            strlen(body),
            body);
    } else {
        request_len = snprintf(request, sizeof(request),
            "%s %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Connection: close\r\n"
            "\r\n",
            method, path, host);
    }

    if (transport_send(&t, request, request_len) < 0) {
        transport_close(&t);
        response->error = string_new("send failed");
        return response;
    }

    // Accumulator grows with capacity doubling. The previous
    // realloc-per-recv pattern was quadratic on large responses;
    // doubling amortises growth to O(n).
    char   buffer[8192];
    char*  full_response = NULL;
    size_t total_len = 0;
    size_t cap = 0;
    int    n;

    while ((n = transport_recv(&t, buffer, sizeof(buffer) - 1)) > 0) {
        if (total_len + (size_t)n + 1 > cap) {
            size_t new_cap = cap ? cap * 2 : 16384;
            while (new_cap < total_len + (size_t)n + 1) new_cap *= 2;
            char* new_resp = (char*)realloc(full_response, new_cap);
            if (!new_resp) {
                free(full_response);
                transport_close(&t);
                response->error = string_new("out of memory reading response");
                return response;
            }
            full_response = new_resp;
            cap = new_cap;
        }
        memcpy(full_response + total_len, buffer, (size_t)n);
        total_len += (size_t)n;
        full_response[total_len] = '\0';
    }

    // Zero-byte response: still need a valid empty string so strstr
    // below is safe. Tiny allocation, done only on the empty path.
    if (!full_response) {
        full_response = (char*)malloc(1);
        if (!full_response) {
            transport_close(&t);
            response->error = string_new("out of memory");
            return response;
        }
        full_response[0] = '\0';
    }

    transport_close(&t);

    char* header_end = strstr(full_response, "\r\n\r\n");
    if (header_end) {
        *header_end = '\0';
        char* status_line = full_response;
        char* space1 = strchr(status_line, ' ');
        if (space1) {
            response->status_code = atoi(space1 + 1);
        }

        response->headers = string_new(full_response);
        response->body = string_new(header_end + 4);
    } else {
        response->body = string_new(full_response);
    }

    free(full_response);
    return response;
}

HttpResponse* http_get_raw(const char* url) {
    return http_request("GET", url, NULL, NULL);
}

HttpResponse* http_post_raw(const char* url, const char* body, const char* content_type) {
    return http_request("POST", url, body, content_type);
}

HttpResponse* http_put_raw(const char* url, const char* body, const char* content_type) {
    return http_request("PUT", url, body, content_type);
}

HttpResponse* http_delete_raw(const char* url) {
    return http_request("DELETE", url, NULL, NULL);
}

void http_response_free(HttpResponse* response) {
    if (!response) return;
    if (response->body) string_release(response->body);
    if (response->headers) string_release(response->headers);
    if (response->error) string_release(response->error);
    free(response);
}

// Response accessors. All NULL-safe: callers can pass a NULL response
// (e.g. from an out-of-memory path) without crashing.

int http_response_status(HttpResponse* response) {
    if (!response) return 0;
    return response->status_code;
}

const char* http_response_body(HttpResponse* response) {
    if (!response || !response->body) return "";
    const char* s = string_to_cstr(response->body);
    return s ? s : "";
}

const char* http_response_headers(HttpResponse* response) {
    if (!response || !response->headers) return "";
    const char* s = string_to_cstr(response->headers);
    return s ? s : "";
}

const char* http_response_error(HttpResponse* response) {
    if (!response || !response->error) return "";
    const char* s = string_to_cstr(response->error);
    return s ? s : "";
}

int http_response_ok(HttpResponse* response) {
    if (!response) return 0;
    if (response->error) return 0;
    return response->status_code >= 200 && response->status_code < 300;
}

// Legacy accessor aliases — thin wrappers over the short names above.
int http_response_status_code(HttpResponse* response) {
    return http_response_status(response);
}

const char* http_response_body_str(HttpResponse* response) {
    return http_response_body(response);
}

const char* http_response_headers_str(HttpResponse* response) {
    return http_response_headers(response);
}

#endif // AETHER_HAS_NETWORKING
