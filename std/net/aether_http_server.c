#include "aether_http_server.h"
#include "../../runtime/config/aether_optimization_config.h"

#if !AETHER_HAS_NETWORKING
// Stubs when networking is unavailable
HttpServer* http_server_create(int p) { (void)p; return NULL; }
int http_server_bind(HttpServer* s, const char* h, int p) { (void)s; (void)h; (void)p; return -1; }
int http_server_start(HttpServer* s) { (void)s; return -1; }
void http_server_stop(HttpServer* s) { (void)s; }
void http_server_free(HttpServer* s) { (void)s; }
void http_server_add_route(HttpServer* s, const char* m, const char* p, HttpHandler h, void* u) { (void)s; (void)m; (void)p; (void)h; (void)u; }
void http_server_get(HttpServer* s, const char* p, HttpHandler h, void* u) { (void)s; (void)p; (void)h; (void)u; }
void http_server_post(HttpServer* s, const char* p, HttpHandler h, void* u) { (void)s; (void)p; (void)h; (void)u; }
void http_server_put(HttpServer* s, const char* p, HttpHandler h, void* u) { (void)s; (void)p; (void)h; (void)u; }
void http_server_delete(HttpServer* s, const char* p, HttpHandler h, void* u) { (void)s; (void)p; (void)h; (void)u; }
void http_server_use_middleware(HttpServer* s, HttpMiddleware m, void* u) { (void)s; (void)m; (void)u; }
HttpRequest* http_parse_request(const char* r) { (void)r; return NULL; }
const char* http_get_header(HttpRequest* r, const char* k) { (void)r; (void)k; return NULL; }
const char* http_get_query_param(HttpRequest* r, const char* k) { (void)r; (void)k; return NULL; }
const char* http_get_path_param(HttpRequest* r, const char* k) { (void)r; (void)k; return NULL; }
void http_request_free(HttpRequest* r) { (void)r; }
HttpServerResponse* http_response_create() { return NULL; }
void http_response_set_status(HttpServerResponse* r, int c) { (void)r; (void)c; }
void http_response_set_header(HttpServerResponse* r, const char* k, const char* v) { (void)r; (void)k; (void)v; }
void http_response_set_body(HttpServerResponse* r, const char* b) { (void)r; (void)b; }
void http_response_json(HttpServerResponse* r, const char* j) { (void)r; (void)j; }
char* http_response_serialize(HttpServerResponse* r) { (void)r; return NULL; }
void http_server_response_free(HttpServerResponse* r) { (void)r; }
int http_route_matches(const char* p, const char* u, HttpRequest* r) { (void)p; (void)u; (void)r; return 0; }
const char* http_status_text(int c) { (void)c; return "Unknown"; }
const char* http_mime_type(const char* p) { (void)p; return "application/octet-stream"; }
void http_serve_file(HttpServerResponse* r, const char* f) { (void)r; (void)f; }
void http_serve_static(HttpRequest* r, HttpServerResponse* s, void* d) { (void)r; (void)s; (void)d; }
#else

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../../runtime/utils/aether_thread.h"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #ifdef _MSC_VER
        #pragma comment(lib, "ws2_32.lib")
    #endif
    #define close closesocket
    typedef int socklen_t;
    #ifndef strcasecmp
        #define strcasecmp _stricmp
    #endif
    #ifndef strdup
        #define strdup _strdup
    #endif
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <limits.h>
    #include <poll.h>
    #include <errno.h>
#endif

// Portable case-insensitive substring search (strcasestr is a GNU extension)
static const char* http_strcasestr(const char* haystack, const char* needle) {
    if (!needle || !*needle) return haystack;
    size_t nlen = strlen(needle);
    for (; *haystack; haystack++) {
        if (tolower((unsigned char)*haystack) == tolower((unsigned char)*needle)) {
            size_t i;
            for (i = 1; i < nlen; i++) {
                if (tolower((unsigned char)haystack[i]) != tolower((unsigned char)needle[i]))
                    break;
            }
            if (i == nlen) return haystack;
        }
    }
    return NULL;
}

static int http_server_initialized = 0;

static void http_server_init() {
    if (http_server_initialized) return;
    #ifdef _WIN32
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
    #endif
    http_server_initialized = 1;
}

HttpServer* http_server_create(int port) {
    http_server_init();

    HttpServer* server = (HttpServer*)calloc(1, sizeof(HttpServer));
    server->port = port;
    server->host = strdup("0.0.0.0");
    server->socket_fd = -1;
    server->is_running = 0;
    server->routes = NULL;
    server->middleware_chain = NULL;
    server->max_connections = 1000;
    server->keep_alive_timeout = 30;
    server->scheduler = NULL;
    server->handler_actor = NULL;
    server->send_fn = NULL;
    server->spawn_fn = NULL;
    server->release_fn = NULL;
    server->step_fn = NULL;
    server->accept_epoll_fd = -1;
    server->multi_accept = 0;
    server->accept_thread_count = 0;
    server->accept_threads = NULL;
    server->accept_listen_fds = NULL;
    server->accept_epoll_fds = NULL;

    return server;
}

int http_server_bind(HttpServer* server, const char* host, int port) {
    server->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->socket_fd < 0) {
        fprintf(stderr, "Failed to create socket\n");
        return -1;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(server->socket_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (strcmp(host, "0.0.0.0") == 0) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, host, &addr.sin_addr);
    }
    
    if (bind(server->socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Failed to bind socket to %s:%d\n", host, port);
        close(server->socket_fd);
        server->socket_fd = -1;
        return -1;
    }
    
    if (listen(server->socket_fd, server->max_connections) < 0) {
        fprintf(stderr, "Failed to listen on socket\n");
        close(server->socket_fd);
        server->socket_fd = -1;
        return -1;
    }
    
    // Update host and port (make copy of host string first)
    char* new_host = strdup(host);
    if (server->host) {
        free(server->host);
    }
    server->host = new_host;
    server->port = port;
    
    return 0;
}

// Request parsing
HttpRequest* http_parse_request(const char* raw_request) {
    HttpRequest* req = (HttpRequest*)calloc(1, sizeof(HttpRequest));
    
    // Parse request line: METHOD /path HTTP/1.1
    char* line_end = strstr(raw_request, "\r\n");
    if (!line_end) {
        free(req);
        return NULL;
    }
    
    char request_line[2048];
    int line_len = line_end - raw_request;
    strncpy(request_line, raw_request, line_len);
    request_line[line_len] = '\0';
    
    // Extract method
    char* space = strchr(request_line, ' ');
    if (!space) {
        free(req);
        return NULL;
    }
    
    int method_len = space - request_line;
    req->method = (char*)malloc(method_len + 1);
    strncpy(req->method, request_line, method_len);
    req->method[method_len] = '\0';
    
    // Extract path and query string
    char* path_start = space + 1;
    char* path_end = strchr(path_start, ' ');
    if (!path_end) {
        free(req->method);
        free(req);
        return NULL;
    }
    
    char* query = strchr(path_start, '?');
    if (query && query < path_end) {
        // Has query string
        int path_len = query - path_start;
        req->path = (char*)malloc(path_len + 1);
        strncpy(req->path, path_start, path_len);
        req->path[path_len] = '\0';
        
        int query_len = path_end - query - 1;
        req->query_string = (char*)malloc(query_len + 1);
        strncpy(req->query_string, query + 1, query_len);
        req->query_string[query_len] = '\0';
    } else {
        // No query string
        int path_len = path_end - path_start;
        req->path = (char*)malloc(path_len + 1);
        strncpy(req->path, path_start, path_len);
        req->path[path_len] = '\0';
        req->query_string = NULL;
    }
    
    // Extract HTTP version
    char* version_start = path_end + 1;
    req->http_version = strdup(version_start);
    
    // Parse headers
    req->header_keys = (char**)malloc(sizeof(char*) * 50);
    req->header_values = (char**)malloc(sizeof(char*) * 50);
    req->header_count = 0;
    
    const char* header_start = line_end + 2;
    while (1) {
        line_end = strstr(header_start, "\r\n");
        if (!line_end || line_end == header_start) {
            // End of headers
            if (line_end) {
                header_start = line_end + 2;
            }
            break;
        }
        
        char header_line[1024];
        line_len = line_end - header_start;
        strncpy(header_line, header_start, line_len);
        header_line[line_len] = '\0';
        
        char* colon = strchr(header_line, ':');
        if (colon && req->header_count < 50) {
            *colon = '\0';
            char* key = header_line;
            char* value = colon + 1;

            // Trim whitespace from value
            while (*value == ' ') value++;

            req->header_keys[req->header_count] = strdup(key);
            req->header_values[req->header_count] = strdup(value);
            req->header_count++;
        }
        
        header_start = line_end + 2;
    }
    
    // Parse body
    if (header_start && *header_start) {
        req->body = strdup(header_start);
        req->body_length = strlen(req->body);
    } else {
        req->body = NULL;
        req->body_length = 0;
    }
    
    req->param_keys = NULL;
    req->param_values = NULL;
    req->param_count = 0;
    
    return req;
}

const char* http_get_header(HttpRequest* req, const char* key) {
    for (int i = 0; i < req->header_count; i++) {
        if (strcasecmp(req->header_keys[i], key) == 0) {
            return req->header_values[i];
        }
    }
    return NULL;
}

const char* http_get_query_param(HttpRequest* req, const char* key) {
    if (!req->query_string) return NULL;
    
    // Parse query params on demand
    char* found = strstr(req->query_string, key);
    if (!found) return NULL;
    
    // Check if it's actually the key (not part of another key)
    if (found != req->query_string && *(found - 1) != '&') {
        return NULL;
    }
    
    char* equals = strchr(found, '=');
    if (!equals) return NULL;
    
    char* value_start = equals + 1;
    char* value_end = strchr(value_start, '&');
    
    static char value_buf[256];
    size_t value_len = value_end ? (size_t)(value_end - value_start) : strlen(value_start);
    strncpy(value_buf, value_start, value_len);
    value_buf[value_len] = '\0';
    
    return value_buf;
}

const char* http_get_path_param(HttpRequest* req, const char* key) {
    for (int i = 0; i < req->param_count; i++) {
        if (strcmp(req->param_keys[i], key) == 0) {
            return req->param_values[i];
        }
    }
    return NULL;
}

void http_request_free(HttpRequest* req) {
    if (!req) return;
    
    free(req->method);
    free(req->path);
    free(req->query_string);
    free(req->http_version);
    free(req->body);
    
    for (int i = 0; i < req->header_count; i++) {
        free(req->header_keys[i]);
        free(req->header_values[i]);
    }
    free(req->header_keys);
    free(req->header_values);
    
    for (int i = 0; i < req->param_count; i++) {
        free(req->param_keys[i]);
        free(req->param_values[i]);
    }
    free(req->param_keys);
    free(req->param_values);
    
    free(req);
}

// Response building
HttpServerResponse* http_response_create() {
    HttpServerResponse* res = (HttpServerResponse*)calloc(1, sizeof(HttpServerResponse));
    res->status_code = 200;
    res->status_text = strdup("OK");
    res->header_keys = (char**)malloc(sizeof(char*) * 50);
    res->header_values = (char**)malloc(sizeof(char*) * 50);
    res->header_count = 0;
    res->body = NULL;
    res->body_length = 0;

    // Add default headers
    http_response_set_header(res, "Content-Type", "text/html; charset=utf-8");
    http_response_set_header(res, "Server", "Aether/1.0");

    return res;
}

void http_response_set_status(HttpServerResponse* res, int code) {
    res->status_code = code;
    free(res->status_text);
    res->status_text = strdup(http_status_text(code));
}

void http_response_set_header(HttpServerResponse* res, const char* key, const char* value) {
    // Check if header exists, update it
    for (int i = 0; i < res->header_count; i++) {
        if (strcasecmp(res->header_keys[i], key) == 0) {
            free(res->header_values[i]);
            res->header_values[i] = strdup(value);
            return;
        }
    }
    
    // Add new header (max 50)
    if (res->header_count >= 50) return;
    res->header_keys[res->header_count] = strdup(key);
    res->header_values[res->header_count] = strdup(value);
    res->header_count++;
}

void http_response_set_body(HttpServerResponse* res, const char* body) {
    free(res->body);
    res->body = strdup(body);
    res->body_length = strlen(body);

    // Update Content-Length
    char len_str[32];
    snprintf(len_str, sizeof(len_str), "%zu", res->body_length);
    http_response_set_header(res, "Content-Length", len_str);
}

void http_response_json(HttpServerResponse* res, const char* json) {
    http_response_set_header(res, "Content-Type", "application/json");
    http_response_set_body(res, json);
}

// Returns a heap-allocated string; caller must free() it.
char* http_response_serialize(HttpServerResponse* res) {
    // Compute required size: status line + headers + blank line + body
    size_t needed = 64;  // status line headroom
    for (int i = 0; i < res->header_count; i++)
        needed += strlen(res->header_keys[i]) + strlen(res->header_values[i]) + 4;
    needed += 2;  // blank line
    if (res->body) needed += res->body_length + 1;

    char* buf = malloc(needed);
    if (!buf) return NULL;

    int off = snprintf(buf, needed, "HTTP/1.1 %d %s\r\n",
                       res->status_code, res->status_text);
    for (int i = 0; i < res->header_count; i++)
        off += snprintf(buf + off, needed - off, "%s: %s\r\n",
                        res->header_keys[i], res->header_values[i]);
    off += snprintf(buf + off, needed - off, "\r\n");
    if (res->body)
        memcpy(buf + off, res->body, res->body_length + 1);

    return buf;
}

void http_server_response_free(HttpServerResponse* res) {
    if (!res) return;

    free(res->status_text);
    free(res->body);

    for (int i = 0; i < res->header_count; i++) {
        free(res->header_keys[i]);
        free(res->header_values[i]);
    }
    free(res->header_keys);
    free(res->header_values);

    free(res);
}

const char* http_status_text(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 500: return "Internal Server Error";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        default: return "Unknown";
    }
}

// Routing
void http_server_add_route(HttpServer* server, const char* method, const char* path, HttpHandler handler, void* user_data) {
    HttpRoute* route = (HttpRoute*)malloc(sizeof(HttpRoute));
    route->method = strdup(method);
    route->path_pattern = strdup(path);
    route->handler = handler;
    route->user_data = user_data;
    route->next = server->routes;
    server->routes = route;
}

void http_server_get(HttpServer* server, const char* path, HttpHandler handler, void* user_data) {
    http_server_add_route(server, "GET", path, handler, user_data);
}

void http_server_post(HttpServer* server, const char* path, HttpHandler handler, void* user_data) {
    http_server_add_route(server, "POST", path, handler, user_data);
}

void http_server_put(HttpServer* server, const char* path, HttpHandler handler, void* user_data) {
    http_server_add_route(server, "PUT", path, handler, user_data);
}

void http_server_delete(HttpServer* server, const char* path, HttpHandler handler, void* user_data) {
    http_server_add_route(server, "DELETE", path, handler, user_data);
}

void http_server_use_middleware(HttpServer* server, HttpMiddleware middleware, void* user_data) {
    HttpMiddlewareNode* node = (HttpMiddlewareNode*)malloc(sizeof(HttpMiddlewareNode));
    node->middleware = middleware;
    node->user_data = user_data;
    node->next = server->middleware_chain;
    server->middleware_chain = node;
}

// Route matching with parameter extraction
int http_route_matches(const char* pattern, const char* path, HttpRequest* req) {
    // Exact match
    if (strcmp(pattern, path) == 0) {
        return 1;
    }
    
    // Pattern matching with parameters
    const char* p = pattern;
    const char* u = path;
    
    // Allocate space for params
    req->param_keys = (char**)malloc(sizeof(char*) * 10);
    req->param_values = (char**)malloc(sizeof(char*) * 10);
    req->param_count = 0;
    
    while (*p && *u) {
        if (*p == ':') {
            // Parameter segment
            p++; // Skip ':'
            
            // Extract parameter name
            const char* param_start = p;
            while (*p && *p != '/') p++;
            
            int param_name_len = p - param_start;
            char* param_name = (char*)malloc(param_name_len + 1);
            strncpy(param_name, param_start, param_name_len);
            param_name[param_name_len] = '\0';
            
            // Extract parameter value from URL
            const char* value_start = u;
            while (*u && *u != '/') u++;
            
            int value_len = u - value_start;
            char* value = (char*)malloc(value_len + 1);
            strncpy(value, value_start, value_len);
            value[value_len] = '\0';
            
            req->param_keys[req->param_count] = param_name;
            req->param_values[req->param_count] = value;
            req->param_count++;
            
        } else if (*p == '*') {
            // Wildcard - matches anything remaining
            return 1;
        } else if (*p == *u) {
            p++;
            u++;
        } else {
            // No match
            return 0;
        }
    }
    
    // Both should be at end for exact match
    return (*p == '\0' && *u == '\0');
}

// Handle a single client connection
static void handle_client_connection(HttpServer* server, int client_fd) {
    // Set read timeout so a slow or dead client doesn't hold this thread forever
#ifdef _WIN32
    DWORD rcv_timeout = 30000;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&rcv_timeout, sizeof(rcv_timeout));
#else
    struct timeval rcv_tv = { .tv_sec = 30, .tv_usec = 0 };
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &rcv_tv, sizeof(rcv_tv));
#endif

    // Read headers first (up to 8KB), then read body up to Content-Length
    int capacity = 8192;
    char* buffer = malloc(capacity);
    if (!buffer) { close(client_fd); return; }

    int total = 0;
    // Read until we see the header/body separator \r\n\r\n
    while (total < capacity - 1) {
        int n = recv(client_fd, buffer + total, capacity - 1 - total, 0);
        if (n <= 0) break;
        total += n;
        buffer[total] = '\0';
        if (strstr(buffer, "\r\n\r\n")) break;
    }
    if (total <= 0) { free(buffer); close(client_fd); return; }
    buffer[total] = '\0';

    // Find Content-Length header if present and read remaining body
    const char* cl_hdr = http_strcasestr(buffer, "Content-Length:");
    if (cl_hdr) {
        long content_length = strtol(cl_hdr + 15, NULL, 10);
        const char* header_end = strstr(buffer, "\r\n\r\n");
        if (header_end && content_length > 0) {
            int header_size = (int)(header_end - buffer) + 4;
            int body_received = total - header_size;
            long body_needed = content_length - body_received;
            if (body_needed > 0) {
                // Grow buffer to fit full body
                int new_cap = header_size + (int)content_length + 1;
                char* nb = realloc(buffer, new_cap);
                if (nb) {
                    buffer = nb;
                    while (body_needed > 0) {
                        int n = recv(client_fd, buffer + total, (int)body_needed, 0);
                        if (n <= 0) break;
                        total += n;
                        body_needed -= n;
                    }
                    buffer[total] = '\0';
                }
            }
        }
    }

    // Parse request
    HttpRequest* req = http_parse_request(buffer);
    free(buffer);
    if (!req) {
        close(client_fd);
        return;
    }

    // Create response
    HttpServerResponse* res = http_response_create();

    // Execute middleware chain
    HttpMiddlewareNode* middleware = server->middleware_chain;
    int should_continue = 1;

    while (middleware && should_continue) {
        should_continue = middleware->middleware(req, res, middleware->user_data);
        middleware = middleware->next;
    }

    // If middleware blocked, send response and return
    if (!should_continue) {
        char* response_str = http_response_serialize(res);
        if (response_str) { send(client_fd, response_str, strlen(response_str), 0); free(response_str); }
        close(client_fd);
        http_request_free(req);
        http_server_response_free(res);
        return;
    }

    // Find matching route
    HttpRoute* route = server->routes;
    HttpRoute* matched_route = NULL;

    while (route) {
        if (strcmp(route->method, req->method) == 0) {
            if (http_route_matches(route->path_pattern, req->path, req)) {
                matched_route = route;
                break;
            }
        }
        route = route->next;
    }

    // Execute route handler or return 404
    if (matched_route) {
        matched_route->handler(req, res, matched_route->user_data);
    } else {
        http_response_set_status(res, 404);
        http_response_set_body(res, "404 Not Found");
    }

    // Send response
    char* response_str = http_response_serialize(res);
    if (response_str) { send(client_fd, response_str, strlen(response_str), 0); free(response_str); }

    // Cleanup
    close(client_fd);
    http_request_free(req);
    http_server_response_free(res);
}

// ============================================================================
// Bounded thread pool for connection handling
// ============================================================================
// Replaces thread-per-connection with a fixed pool of worker threads.
// Connections beyond pool capacity wait in the kernel accept backlog.
// This prevents unbounded thread creation under load.

#if AETHER_HAS_THREADS && !defined(_WIN32)

#define HTTP_POOL_WORKERS   8
#define HTTP_POOL_QUEUE_CAP 256

typedef struct {
    HttpServer* server;
    int queue[HTTP_POOL_QUEUE_CAP];   // Ring buffer of pending client fds
    int head, tail, count;
    pthread_mutex_t lock;
    pthread_cond_t  not_empty;
    pthread_cond_t  not_full;
    int shutdown;
    pthread_t workers[HTTP_POOL_WORKERS];
} HttpConnectionPool;

static void* http_pool_worker(void* arg) {
    HttpConnectionPool* pool = (HttpConnectionPool*)arg;
    while (1) {
        pthread_mutex_lock(&pool->lock);
        while (pool->count == 0 && !pool->shutdown) {
            pthread_cond_wait(&pool->not_empty, &pool->lock);
        }
        if (pool->shutdown && pool->count == 0) {
            pthread_mutex_unlock(&pool->lock);
            break;
        }
        int client_fd = pool->queue[pool->head];
        pool->head = (pool->head + 1) % HTTP_POOL_QUEUE_CAP;
        pool->count--;
        pthread_cond_signal(&pool->not_full);
        pthread_mutex_unlock(&pool->lock);

        handle_client_connection(pool->server, client_fd);
    }
    return NULL;
}

static HttpConnectionPool* http_pool_create(HttpServer* server) {
    HttpConnectionPool* pool = calloc(1, sizeof(HttpConnectionPool));
    if (!pool) return NULL;
    pool->server = server;
    pthread_mutex_init(&pool->lock, NULL);
    pthread_cond_init(&pool->not_empty, NULL);
    pthread_cond_init(&pool->not_full, NULL);
    for (int i = 0; i < HTTP_POOL_WORKERS; i++) {
        pthread_create(&pool->workers[i], NULL, http_pool_worker, pool);
    }
    return pool;
}

static void http_pool_submit(HttpConnectionPool* pool, int client_fd) {
    pthread_mutex_lock(&pool->lock);
    while (pool->count >= HTTP_POOL_QUEUE_CAP && !pool->shutdown) {
        pthread_cond_wait(&pool->not_full, &pool->lock);
    }
    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->lock);
        close(client_fd);
        return;
    }
    pool->queue[pool->tail] = client_fd;
    pool->tail = (pool->tail + 1) % HTTP_POOL_QUEUE_CAP;
    pool->count++;
    pthread_cond_signal(&pool->not_empty);
    pthread_mutex_unlock(&pool->lock);
}

static void http_pool_destroy(HttpConnectionPool* pool) {
    if (!pool) return;
    pthread_mutex_lock(&pool->lock);
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->not_empty);
    pthread_cond_broadcast(&pool->not_full);
    pthread_mutex_unlock(&pool->lock);
    for (int i = 0; i < HTTP_POOL_WORKERS; i++) {
        pthread_join(pool->workers[i], NULL);
    }
    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->not_empty);
    pthread_cond_destroy(&pool->not_full);
    free(pool);
}

#endif // AETHER_HAS_THREADS && !_WIN32

// ---------------------------------------------------------------------------
// Accept thread context (one per core in multi-accept mode)
// ---------------------------------------------------------------------------
#if defined(__linux__) && !defined(_WIN32)
typedef struct {
    HttpServer* server;
    int listen_fd;      // This thread's SO_REUSEPORT listen socket
    int epoll_fd;       // This thread's epoll instance
    int thread_index;   // Which core's workers to prefer
} AcceptThreadCtx;

// Create a SO_REUSEPORT listen socket bound to the same port
static int create_reuseport_socket(const char* host, int port, int backlog) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (strcmp(host, "0.0.0.0") == 0) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, host, &addr.sin_addr);
    }

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    if (listen(fd, backlog) < 0) {
        close(fd);
        return -1;
    }

    // Non-blocking for epoll
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    return fd;
}

// Per-core accept + epoll loop
static void accept_epoll_loop(HttpServer* server, int listen_fd, int epoll_fd) {
    struct epoll_event events[256];

    while (server->is_running) {
        int n = epoll_wait(epoll_fd, events, 256, 100);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;

            if (fd == listen_fd) {
                // Accept all pending connections
                while (1) {
                    struct sockaddr_in client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    int client_fd = accept(listen_fd,
                        (struct sockaddr*)&client_addr, &client_len);
                    if (client_fd < 0) break;

                    int cflags = fcntl(client_fd, F_GETFL, 0);
                    if (cflags >= 0) fcntl(client_fd, F_SETFL, cflags | O_NONBLOCK);

                    struct epoll_event cev;
                    cev.events = EPOLLIN | EPOLLONESHOT;
                    cev.data.fd = client_fd;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &cev) != 0) {
                        close(client_fd);
                    }
                }
            } else {
                // Data ready — dispatch to worker
                void* worker = server->spawn_fn(-1, server->step_fn, 0);
                if (worker) {
                    HttpConnectionMessage conn_msg;
                    conn_msg.type = MSG_HTTP_CONNECTION;
                    conn_msg.client_fd = fd;
                    server->send_fn(worker, &conn_msg, sizeof(conn_msg));
                } else {
                    const char* err = "HTTP/1.1 503 Service Unavailable\r\n"
                                      "Content-Length: 19\r\n\r\nService Unavailable";
                    send(fd, err, strlen(err), MSG_NOSIGNAL);
                    close(fd);
                }
            }
        }
    }
}

static void* accept_thread_fn(void* arg) {
    AcceptThreadCtx* ctx = (AcceptThreadCtx*)arg;
    accept_epoll_loop(ctx->server, ctx->listen_fd, ctx->epoll_fd);
    free(ctx);
    return NULL;
}
#endif

int http_server_start(HttpServer* server) {
    server->is_running = 1;

    int use_actor_mode = (server->spawn_fn && server->send_fn && server->step_fn);

    // Actor mode with epoll: accept → epoll_wait for data → dispatch to worker
    // This ensures workers never block on recv() inside their step() function.
#if defined(__linux__) && !defined(_WIN32)
    if (use_actor_mode && server->multi_accept) {
        // Multi-accept mode (opt-in): one accept thread per core with SO_REUSEPORT.
        // Best for very high connection rates where accept() is the bottleneck.
        int n_threads = (int)sysconf(_SC_NPROCESSORS_ONLN);
        if (n_threads <= 0) n_threads = 4;
        if (n_threads > 16) n_threads = 16;

        server->accept_listen_fds = calloc(n_threads, sizeof(int));
        server->accept_epoll_fds = calloc(n_threads, sizeof(int));
        server->accept_threads = calloc(n_threads, sizeof(pthread_t));
        if (!server->accept_listen_fds || !server->accept_epoll_fds || !server->accept_threads) {
            fprintf(stderr, "Failed to allocate accept thread state\n");
            return -1;
        }

        for (int i = 0; i < n_threads; i++) {
            server->accept_listen_fds[i] = -1;
            server->accept_epoll_fds[i] = -1;
        }

        for (int i = 0; i < n_threads; i++) {
            server->accept_listen_fds[i] = create_reuseport_socket(
                server->host, server->port, server->max_connections);
            if (server->accept_listen_fds[i] < 0) {
                fprintf(stderr, "Failed to create SO_REUSEPORT socket for thread %d\n", i);
                return -1;
            }

            server->accept_epoll_fds[i] = epoll_create1(EPOLL_CLOEXEC);
            if (server->accept_epoll_fds[i] < 0) {
                fprintf(stderr, "epoll_create1 failed for thread %d\n", i);
                return -1;
            }

            struct epoll_event ev;
            ev.events = EPOLLIN;
            ev.data.fd = server->accept_listen_fds[i];
            epoll_ctl(server->accept_epoll_fds[i], EPOLL_CTL_ADD,
                      server->accept_listen_fds[i], &ev);
        }

        server->accept_thread_count = n_threads;

        printf("Server running at http://%s:%d (%d accept threads, SO_REUSEPORT)\n",
               server->host, server->port, n_threads);
        printf("Press Ctrl+C to stop\n\n");
        fflush(stdout);

        for (int i = 1; i < n_threads; i++) {
            AcceptThreadCtx* ctx = malloc(sizeof(AcceptThreadCtx));
            ctx->server = server;
            ctx->listen_fd = server->accept_listen_fds[i];
            ctx->epoll_fd = server->accept_epoll_fds[i];
            ctx->thread_index = i;
            pthread_create(&server->accept_threads[i], NULL, accept_thread_fn, ctx);
        }

        accept_epoll_loop(server, server->accept_listen_fds[0],
                          server->accept_epoll_fds[0]);

        for (int i = 1; i < n_threads; i++) {
            pthread_join(server->accept_threads[i], NULL);
        }

        for (int i = 0; i < n_threads; i++) {
            if (server->accept_epoll_fds[i] >= 0) close(server->accept_epoll_fds[i]);
            if (server->accept_listen_fds[i] >= 0) close(server->accept_listen_fds[i]);
        }
        free(server->accept_listen_fds);
        free(server->accept_epoll_fds);
        free(server->accept_threads);
        server->accept_listen_fds = NULL;
        server->accept_epoll_fds = NULL;
        server->accept_threads = NULL;
        server->accept_thread_count = 0;

    } else if (use_actor_mode) {
        // Single-accept with epoll (default): one accept thread waits for data
        // before dispatching to worker actors. Best for most workloads.
        if (http_server_bind(server, server->host, server->port) < 0) {
            return -1;
        }

        server->accept_epoll_fd = epoll_create1(EPOLL_CLOEXEC);
        if (server->accept_epoll_fd < 0) {
            fprintf(stderr, "epoll_create1 failed: %s\n", strerror(errno));
            return -1;
        }

        struct epoll_event listen_ev;
        listen_ev.events = EPOLLIN;
        listen_ev.data.fd = server->socket_fd;
        epoll_ctl(server->accept_epoll_fd, EPOLL_CTL_ADD, server->socket_fd, &listen_ev);

        int flags = fcntl(server->socket_fd, F_GETFL, 0);
        if (flags >= 0) fcntl(server->socket_fd, F_SETFL, flags | O_NONBLOCK);

        printf("Server running at http://%s:%d\n", server->host, server->port);
        printf("Press Ctrl+C to stop\n\n");
        fflush(stdout);

        accept_epoll_loop(server, server->socket_fd, server->accept_epoll_fd);

        close(server->accept_epoll_fd);
        server->accept_epoll_fd = -1;

    } else
#endif
    {
        if (http_server_bind(server, server->host, server->port) < 0) {
            return -1;
        }

        printf("Server running at http://%s:%d\n", server->host, server->port);
        printf("Press Ctrl+C to stop\n\n");
        fflush(stdout);

#if AETHER_HAS_THREADS && !defined(_WIN32)
        HttpConnectionPool* pool = http_pool_create(server);
#endif

        // Fallback: poll + thread pool (non-Linux or no actor handler)
        while (server->is_running) {
#if !defined(_WIN32)
            struct pollfd pfd = { .fd = server->socket_fd, .events = POLLIN };
            int ready = poll(&pfd, 1, 1000);
            if (ready <= 0) continue;
#endif

            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(server->socket_fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd < 0) {
                if (!server->is_running) break;
                continue;
            }

#if !AETHER_HAS_THREADS
            handle_client_connection(server, client_fd);
#elif defined(_WIN32)
            handle_client_connection(server, client_fd);
#else
            http_pool_submit(pool, client_fd);
#endif
        }

#if AETHER_HAS_THREADS && !defined(_WIN32)
        http_pool_destroy(pool);
#endif
    }

    return 0;
}

void http_server_stop(HttpServer* server) {
    if (!server) return;

    server->is_running = 0;

#if defined(__linux__) && !defined(_WIN32)
    // Close all accept epoll fds to unblock epoll_wait in accept threads
    for (int i = 0; i < server->accept_thread_count; i++) {
        if (server->accept_epoll_fds && server->accept_epoll_fds[i] >= 0) {
            close(server->accept_epoll_fds[i]);
            server->accept_epoll_fds[i] = -1;
        }
        if (server->accept_listen_fds && server->accept_listen_fds[i] >= 0) {
            close(server->accept_listen_fds[i]);
            server->accept_listen_fds[i] = -1;
        }
    }

    if (server->accept_epoll_fd >= 0) {
        close(server->accept_epoll_fd);
        server->accept_epoll_fd = -1;
    }
#endif

    if (server->socket_fd >= 0) {
#ifdef _WIN32
        closesocket(server->socket_fd);
        WSACleanup();
#else
        close(server->socket_fd);
#endif
        server->socket_fd = -1;
    }
}

void http_server_free(HttpServer* server) {
    if (!server) return;

    http_server_stop(server);
    
    free(server->host);
    
    // Free routes
    HttpRoute* route = server->routes;
    while (route) {
        HttpRoute* next = route->next;
        free(route->method);
        free(route->path_pattern);
        free(route);
        route = next;
    }
    
    // Free middleware
    HttpMiddlewareNode* middleware = server->middleware_chain;
    while (middleware) {
        HttpMiddlewareNode* next = middleware->next;
        free(middleware);
        middleware = next;
    }
    
    free(server);
}

// MIME type detection based on file extension
const char* http_mime_type(const char* path) {
    if (!path) return "application/octet-stream";

    const char* ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";

    ext++; // Skip the dot

    // Common web MIME types
    if (strcasecmp(ext, "html") == 0 || strcasecmp(ext, "htm") == 0) return "text/html; charset=utf-8";
    if (strcasecmp(ext, "css") == 0) return "text/css; charset=utf-8";
    if (strcasecmp(ext, "js") == 0) return "application/javascript; charset=utf-8";
    if (strcasecmp(ext, "json") == 0) return "application/json; charset=utf-8";
    if (strcasecmp(ext, "xml") == 0) return "application/xml; charset=utf-8";
    if (strcasecmp(ext, "txt") == 0) return "text/plain; charset=utf-8";

    // Images
    if (strcasecmp(ext, "png") == 0) return "image/png";
    if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0) return "image/jpeg";
    if (strcasecmp(ext, "gif") == 0) return "image/gif";
    if (strcasecmp(ext, "svg") == 0) return "image/svg+xml";
    if (strcasecmp(ext, "ico") == 0) return "image/x-icon";
    if (strcasecmp(ext, "webp") == 0) return "image/webp";

    // Fonts
    if (strcasecmp(ext, "woff") == 0) return "font/woff";
    if (strcasecmp(ext, "woff2") == 0) return "font/woff2";
    if (strcasecmp(ext, "ttf") == 0) return "font/ttf";
    if (strcasecmp(ext, "otf") == 0) return "font/otf";

    // Other
    if (strcasecmp(ext, "pdf") == 0) return "application/pdf";
    if (strcasecmp(ext, "zip") == 0) return "application/zip";
    if (strcasecmp(ext, "wasm") == 0) return "application/wasm";

    return "application/octet-stream";
}

// Serve a single file
void http_serve_file(HttpServerResponse* res, const char* filepath) {
    FILE* f = fopen(filepath, "rb");
    if (!f) {
        http_response_set_status(res, 404);
        http_response_set_body(res, "404 - File Not Found");
        return;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    // Read file content
    char* content = (char*)malloc(size + 1);
    if (!content) {
        fclose(f);
        http_response_set_status(res, 500);
        http_response_set_body(res, "500 - Server Error");
        return;
    }

    size_t bytes_read = fread(content, 1, size, f);
    fclose(f);
    if (bytes_read == 0 && size > 0) {
        free(content);
        http_response_set_status(res, 500);
        http_response_set_body(res, "500 - Server Error");
        return;
    }
    content[bytes_read] = '\0';

    // Set response
    http_response_set_status(res, 200);
    http_response_set_header(res, "Content-Type", http_mime_type(filepath));
    http_response_set_header(res, "Access-Control-Allow-Origin", "*");
    http_response_set_body(res, content);
    free(content);
}

// Static file serving handler (for use with wildcard routes)
void http_serve_static(HttpRequest* req, HttpServerResponse* res, void* base_dir) {
    const char* dir = (const char*)base_dir;
    if (!dir) dir = ".";

    // Build filepath from request path
    const char* req_path = req->path;
    if (!req_path || req_path[0] == '\0') req_path = "/";

    // Handle root path
    if (strcmp(req_path, "/") == 0) {
        req_path = "/index.html";
    }

    // Skip leading slash
    if (req_path[0] == '/') req_path++;

    // Security: reject encoded traversal sequences (%2e, %2f, %5c)
    if (strstr(req_path, "..") != NULL ||
        strstr(req_path, "%2e") != NULL || strstr(req_path, "%2E") != NULL ||
        strstr(req_path, "%2f") != NULL || strstr(req_path, "%2F") != NULL ||
        strstr(req_path, "%5c") != NULL || strstr(req_path, "%5C") != NULL ||
        strstr(req_path, "\\") != NULL) {
        http_response_set_status(res, 403);
        http_response_set_body(res, "403 - Forbidden");
        return;
    }

    // Build full path
    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/%s", dir, req_path);

    // Security: resolve to canonical path and verify it's within the root dir
#ifndef _WIN32
    char resolved[PATH_MAX];
    char resolved_dir[PATH_MAX];
    if (!realpath(filepath, resolved) || !realpath(dir, resolved_dir)) {
        http_response_set_status(res, 404);
        http_response_set_body(res, "404 - Not Found");
        return;
    }
    if (strncmp(resolved, resolved_dir, strlen(resolved_dir)) != 0) {
        http_response_set_status(res, 403);
        http_response_set_body(res, "403 - Forbidden");
        return;
    }
    // Serve the resolved, validated path
    http_serve_file(res, resolved);
#else
    // On Windows, use _fullpath for canonicalization
    char resolved[1024];
    char resolved_dir[1024];
    if (!_fullpath(resolved, filepath, sizeof(resolved)) ||
        !_fullpath(resolved_dir, dir, sizeof(resolved_dir))) {
        http_response_set_status(res, 404);
        http_response_set_body(res, "404 - Not Found");
        return;
    }
    if (_strnicmp(resolved, resolved_dir, strlen(resolved_dir)) != 0) {
        http_response_set_status(res, 403);
        http_response_set_body(res, "403 - Forbidden");
        return;
    }
    http_serve_file(res, resolved);
#endif
}

// ============================================================================
// Actor dispatch mode
// ============================================================================

void http_server_set_actor_handler(HttpServer* server, void (*step_fn)(void*),
                                    void (*send_fn)(void*, void*, size_t),
                                    void* (*spawn_fn)(int, void (*)(void*), size_t),
                                    void (*release_fn)(void*)) {
    if (!server || !step_fn || !send_fn || !spawn_fn) return;
    server->step_fn = step_fn;
    server->send_fn = send_fn;
    server->spawn_fn = spawn_fn;
    server->release_fn = release_fn;
}

// Request accessors (for Aether .ae code via opaque ptr)
const char* http_request_method(HttpRequest* req) {
    return req ? req->method : "";
}

const char* http_request_path(HttpRequest* req) {
    return req ? req->path : "";
}

const char* http_request_body(HttpRequest* req) {
    return req ? req->body : "";
}

const char* http_request_query(HttpRequest* req) {
    return req ? req->query_string : "";
}

#endif // AETHER_HAS_NETWORKING
