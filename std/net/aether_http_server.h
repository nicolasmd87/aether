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

    // Routing
    HttpRoute* routes;

    // Middleware
    HttpMiddlewareNode* middleware_chain;

    // Actor system
    Scheduler* scheduler;

    // Configuration
    int max_connections;
    int keep_alive_timeout;
} HttpServer;

// Server lifecycle
HttpServer* http_server_create(int port);
int http_server_bind(HttpServer* server, const char* host, int port);
int http_server_start(HttpServer* server);
void http_server_stop(HttpServer* server);
void http_server_free(HttpServer* server);

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

#endif
