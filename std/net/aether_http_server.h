#ifndef AETHER_HTTP_SERVER_H
#define AETHER_HTTP_SERVER_H

#include "../string/aether_string.h"
#include "../../runtime/scheduler/multicore_scheduler.h"

// HTTP Request
typedef struct {
    char* method;           // GET, POST, PUT, DELETE, etc.
    char* path;             // /api/users
    char* query_string;     // ?key=value&foo=bar
    char* http_version;     // HTTP/1.1
    char** header_keys;
    char** header_values;
    int header_count;
    char* body;
    size_t body_length;

    // Parsed data
    char** param_keys;      // From /users/:id
    char** param_values;
    int param_count;
} HttpRequest;

// HTTP Response
typedef struct {
    int status_code;
    char* status_text;
    char** header_keys;
    char** header_values;
    int header_count;
    char* body;
    size_t body_length;
} HttpServerResponse;

// Route handler callback
typedef void (*HttpHandler)(HttpRequest* req, HttpServerResponse* res, void* user_data);

// Middleware callback
typedef int (*HttpMiddleware)(HttpRequest* req, HttpServerResponse* res, void* user_data);

// Route definition
typedef struct HttpRoute {
    char* method;
    char* path_pattern;     // /users/:id
    HttpHandler handler;
    void* user_data;
    struct HttpRoute* next;
} HttpRoute;

// Middleware chain
typedef struct HttpMiddlewareNode {
    HttpMiddleware middleware;
    void* user_data;
    struct HttpMiddlewareNode* next;
} HttpMiddlewareNode;

// HTTP Server
typedef struct {
    int socket_fd;
    int port;
    char* host;
    int is_running;

    // Server-side TLS (#260 Tier 0). When tls_enabled == 1, every
    // accepted connection is wrapped in SSL_accept before the HTTP
    // parse begins. tls_ctx is an SSL_CTX*, declared here as void*
    // so this header doesn't pull in <openssl/ssl.h> for callers
    // that don't need it. Lazy-allocated by http_server_set_tls.
    int tls_enabled;
    void* tls_ctx;

    // Routing
    HttpRoute* routes;

    // Middleware
    HttpMiddlewareNode* middleware_chain;

    // Actor system
    Scheduler* scheduler;

    // Actor dispatch mode (opt-in: set via http_server_set_actor_handler)
    void* handler_actor;            // Legacy single actor (NULL = use C handlers)
    // Fire-and-forget: spawn one actor per request
    void (*send_fn)(void*, void*, size_t);      // aether_send_message (avoids link dep)
    void* (*spawn_fn)(int, void (*)(void*), size_t);  // scheduler_spawn_pooled
    void (*release_fn)(void*);                  // scheduler_release_pooled
    void (*step_fn)(void*);                     // User-provided step function for per-request actors

    // Configuration
    int max_connections;
    int keep_alive_timeout;

    // Accept-side I/O poller: wait for client data before dispatching to worker
    AetherIoPoller accept_poller;   // Platform poller for single-accept mode

    // Multi-accept: one accept thread per core with SO_REUSEPORT (opt-in)
    int multi_accept;               // 0 = single accept (default), 1 = SO_REUSEPORT multi-accept
    int accept_thread_count;
    pthread_t* accept_threads;      // Array of accept thread handles
    int* accept_listen_fds;         // Per-thread listen sockets (SO_REUSEPORT)
    AetherIoPoller* accept_pollers; // Per-thread I/O pollers
} HttpServer;

// ============================================================================
// Actor dispatch mode
// ============================================================================

// Message type IDs for HTTP actor dispatch
#define MSG_HTTP_REQUEST  200   // Pre-parsed request (legacy actor dispatch)
#define MSG_HTTP_CONNECTION 201 // Raw fd — actor does recv+parse+respond+close

// Legacy: pre-parsed request message (accept thread does recv+parse).
typedef struct {
    int type;               // MSG_HTTP_REQUEST (must be first field)
    int client_fd;          // Socket fd (actor writes response + closes)
    HttpRequest* request;   // Parsed request (actor must call http_request_free)
} HttpActorRequest;

// Full-actor mode: only the fd crosses thread boundary.
// The worker actor owns the entire lifecycle: recv, parse, respond, close.
typedef struct {
    int type;               // MSG_HTTP_CONNECTION (must be first field)
    int client_fd;          // Socket fd (actor owns everything)
} HttpConnectionMessage;

// Server lifecycle
HttpServer* http_server_create(int port);
int http_server_bind_raw(HttpServer* server, const char* host, int port);
int http_server_start_raw(HttpServer* server);
void http_server_stop(HttpServer* server);
void http_server_free(HttpServer* server);

// Enable TLS termination on this server (#260 Tier 0). Loads the cert
// and private key from the given file paths (PEM-encoded), verifies
// they match, and configures the server's SSL_CTX. Returns "" on
// success; an error string on failure (file unreadable, parse error,
// cert/key mismatch, OpenSSL not built in). After this call, every
// accepted connection completes a TLS handshake before the HTTP parse
// begins; clients connecting plain-HTTP get rejected with a TLS
// handshake error. Idempotent — calling twice with the same files is
// a no-op success; calling with different files re-loads.
//
// When the build does not include OpenSSL (AETHER_HAS_OPENSSL undef),
// returns "TLS unavailable: built without OpenSSL".
const char* http_server_set_tls_raw(HttpServer* server,
                                    const char* cert_path,
                                    const char* key_path);

// Routing
void http_server_add_route(HttpServer* server, const char* method, const char* path, HttpHandler handler, void* user_data);
void http_server_get(HttpServer* server, const char* path, HttpHandler handler, void* user_data);
void http_server_post(HttpServer* server, const char* path, HttpHandler handler, void* user_data);
void http_server_put(HttpServer* server, const char* path, HttpHandler handler, void* user_data);
void http_server_delete(HttpServer* server, const char* path, HttpHandler handler, void* user_data);

// Middleware
void http_server_use_middleware(HttpServer* server, HttpMiddleware middleware, void* user_data);

// Request parsing
HttpRequest* http_parse_request(const char* raw_request);
const char* http_get_header(HttpRequest* req, const char* key);
const char* http_get_query_param(HttpRequest* req, const char* key);
const char* http_get_path_param(HttpRequest* req, const char* key);
void http_request_free(HttpRequest* req);

// Response building
HttpServerResponse* http_response_create();
void http_response_set_status(HttpServerResponse* res, int code);
void http_response_set_header(HttpServerResponse* res, const char* key, const char* value);
void http_response_set_body(HttpServerResponse* res, const char* body);
void http_response_json(HttpServerResponse* res, const char* json);
char* http_response_serialize(HttpServerResponse* res);  // caller must free()
void http_server_response_free(HttpServerResponse* res);

// Helpers
int http_route_matches(const char* pattern, const char* path, HttpRequest* req);
const char* http_status_text(int code);

// MIME type detection
const char* http_mime_type(const char* path);

// Static file serving
void http_serve_file(HttpServerResponse* res, const char* filepath);
void http_serve_static(HttpRequest* req, HttpServerResponse* res, void* base_dir);

// Actor dispatch mode (fire-and-forget, one actor per request)
// step_fn: actor step function that handles MSG_HTTP_REQUEST messages
// send_fn: pass aether_send_message
// spawn_fn: pass scheduler_spawn_pooled
// release_fn: pass scheduler_release_pooled
void http_server_set_actor_handler(HttpServer* server, void (*step_fn)(void*),
                                    void (*send_fn)(void*, void*, size_t),
                                    void* (*spawn_fn)(int, void (*)(void*), size_t),
                                    void (*release_fn)(void*));

// Request accessors (for use from Aether .ae code via opaque ptr)
const char* http_request_method(HttpRequest* req);
const char* http_request_path(HttpRequest* req);
const char* http_request_body(HttpRequest* req);
const char* http_request_query(HttpRequest* req);

#endif
