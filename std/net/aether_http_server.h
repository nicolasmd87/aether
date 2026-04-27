#ifndef AETHER_HTTP_SERVER_H
#define AETHER_HTTP_SERVER_H

#include "../string/aether_string.h"
#include "../../runtime/scheduler/multicore_scheduler.h"
#include <stdatomic.h>

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

// Forward declare these so the HttpServer struct can hold them as
// fields. Their full type definitions appear with the related
// public functions below.
struct HttpServer; /* forward, for hook signatures */
typedef void (*HttpLifecycleHook)(struct HttpServer* server, void* user_data);
typedef int  (*HttpReadyCheck)(void* user_data);

// Per-request observation hook (#260 Tier 3 — F1 logging, F2
// metrics). Fires once per completed request with the parsed
// request, the populated response, and the duration in
// microseconds. The hook may not mutate either argument; it's an
// observation point. Used by the built-in access logger and the
// per-route metrics collector. Multiple hooks may register; they
// run in registration order.
typedef void (*HttpRequestHook)(HttpRequest* req,
                                HttpServerResponse* res,
                                long duration_us,
                                void* user_data);

// HTTP Server
typedef struct HttpServer {
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

    // Response transformers (#260 Tier 1). Run after the route
    // handler, before serialization. See
    // http_server_use_response_transformer above.
    struct HttpResponseTransformerNode* response_transformer_chain;

    // Lifecycle hooks (#260 Tier 3). NULL == not installed.
    HttpLifecycleHook on_start;
    void* on_start_user_data;
    HttpLifecycleHook on_stop;
    void* on_stop_user_data;

    // Health probes (#260 Tier 3). When non-zero, the route table
    // will already have routes registered for live/ready_path — the
    // health-probe handlers are stateless functions that close
    // over `ready_check` via the route's user_data slot.
    HttpReadyCheck ready_check;
    void* ready_check_user_data;

    // In-flight connection tracker for graceful shutdown (#260 Tier 3).
    // Incremented at the top of http_server_drain_connection and
    // decremented at the bottom; the shutdown helper waits on this.
    _Atomic int inflight_connections;

    // Per-request observation hook chain (#260 Tier 3 F1 / F2).
    // Hooks fire once per completed request with timing.
    struct HttpRequestHookNode* request_hook_chain;

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
    int keep_alive_timeout;          // legacy field (was always 30, never read)
    // HTTP/1.1 keep-alive (#260 Tier 0). When keep_alive_enabled is
    // non-zero, handle_client_connection loops over the same socket,
    // parsing successive requests until either:
    //   - the client sends Connection: close,
    //   - keep_alive_max requests have been served, or
    //   - keep_alive_idle_ms elapses with no new bytes on the socket.
    // The default is 0 (single-request, matches pre-#260 behaviour).
    int keep_alive_enabled;
    int keep_alive_max;              // 0 means "unlimited"
    int keep_alive_idle_ms;          // 0 means "use SO_RCVTIMEO default (30s)"

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

// Enable HTTP/1.1 keep-alive on this server (#260 Tier 0). When
// `enabled` is non-zero, the connection handler reads multiple
// requests off one socket instead of closing after the first. The
// loop terminates when:
//   - the client sends `Connection: close` (or HTTP/1.0 without
//     `Connection: keep-alive`),
//   - the response status is one that mandates close (e.g. 408,
//     426),
//   - max_requests requests have been served on this socket
//     (0 = unlimited),
//   - idle_ms elapses with no new bytes (0 = use the default
//     30-second SO_RCVTIMEO already applied by the connection
//     handler).
// Returns "" on success; "server is null" on bad input.
const char* http_server_set_keepalive_raw(HttpServer* server,
                                          int enabled,
                                          int max_requests,
                                          int idle_ms);

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

// Response-transformer chain (#260 Tier 1). Registered transformers
// run AFTER the route handler emits a response and BEFORE the
// response is serialized to the wire. Transformers may mutate the
// response (compress body, swap error pages, attach headers).
// Transformers run in registration order; each receives (req, res,
// user_data) and has no return value (mutation is the only effect).
typedef void (*HttpResponseTransformer)(HttpRequest* req,
                                        HttpServerResponse* res,
                                        void* user_data);
void http_server_use_response_transformer(HttpServer* server,
                                          HttpResponseTransformer xform,
                                          void* user_data);

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
// Length-aware variant — use this when the response body might contain
// binary (e.g. gzip-compressed). The returned buffer is NOT NUL-
// terminated as a string; pass *out_len bytes to send. Caller free()s.
char* http_response_serialize_len(HttpServerResponse* res, size_t* out_len);
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

// Graceful shutdown (#260 Tier 3). Stops accepting new connections,
// then waits up to `timeout_ms` for in-flight connections to finish
// naturally. Returns "" on clean drain, "timeout" if the deadline
// passed with connections still active. Callers typically install
// this on SIGTERM in their entry point.
const char* http_server_shutdown_graceful_raw(HttpServer* server, int timeout_ms);

// Lifecycle hooks (#260 Tier 3). on_start fires once after the
// listen socket is bound but before the accept loop runs (good
// place to log "ready" or set readiness probes). on_stop fires
// after the accept loop exits but before sockets close (good place
// to flush logs, snapshot metrics). HttpLifecycleHook typedef
// declared earlier so the HttpServer struct could carry the field.
void http_server_set_on_start(HttpServer* server, HttpLifecycleHook hook, void* user_data);
void http_server_set_on_stop (HttpServer* server, HttpLifecycleHook hook, void* user_data);

// Health-probe endpoints (#260 Tier 3). Registers two routes:
//   `live_path`  always returns 200 "ok"  (process is up)
//   `ready_path` calls `ready_check` (returns 1 = 200, 0 = 503)
// Either path may be NULL/"" to skip registration. ready_check may
// be NULL — in that case `ready_path` always returns 200 too.
const char* http_server_set_health_probes_raw(HttpServer* server,
                                              const char* live_path,
                                              const char* ready_path,
                                              HttpReadyCheck ready_check,
                                              void* user_data);

// Register a per-request observation hook (#260 Tier 3). Multiple
// hooks may register; they run in registration order after the
// route handler emits the response and after any response
// transformers have run. Used by the built-in access logger and
// metrics collector below; user code can register custom hooks too.
void http_server_use_request_hook(HttpServer* server,
                                  HttpRequestHook hook,
                                  void* user_data);

// Built-in access logger (#260 Tier 3 / F1). Writes one log line
// per completed request. `format` selects the wire format:
//   "combined" — NCSA Combined Log Format (Apache default)
//   "json"     — one-object-per-line JSON
//   ""         — disabled
// `output_path` is a file path (opened with "ab" — appends so
// process restarts don't truncate), or "-" for stderr, or "" to
// disable. Returns "" on success, error string on failure.
const char* http_server_set_access_log_raw(HttpServer* server,
                                           const char* format,
                                           const char* output_path);

// Built-in per-route metrics collector (#260 Tier 3 / F2). Tracks
// request counts and latency histograms keyed by method+path-
// pattern. Exposes the standard Prometheus text format on a
// configurable endpoint (default "/metrics"). Returns "" on
// success, error string on failure.
const char* http_server_set_metrics_raw(HttpServer* server,
                                        const char* metrics_endpoint);

// Drain an accepted connection through the full HTTP lifecycle: TLS
// handshake (when enabled), parse request, run middleware, dispatch
// to the matching route, emit response, then loop for the next
// request when keep-alive is enabled. Closes the socket when the
// connection terminates. Idempotent on a closed/invalid fd
// (returns immediately).
//
// This is the building block that power callers — actor step
// functions, custom thread-pool workers, embedding hosts —
// use to plug a single connection into the standard server
// machinery. The thread-pool worker path inside this file calls it
// internally; user actor step functions registered via
// http_server_set_actor_handler should call it on the
// MSG_HTTP_CONNECTION message's client_fd to get the same
// keep-alive / TLS / route-dispatch behaviour. (#260 Tier 0)
void http_server_drain_connection(HttpServer* server, int client_fd);

// Request accessors (for use from Aether .ae code via opaque ptr)
const char* http_request_method(HttpRequest* req);
const char* http_request_path(HttpRequest* req);
const char* http_request_body(HttpRequest* req);
const char* http_request_query(HttpRequest* req);

#endif
