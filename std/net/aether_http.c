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

static int http_initialized = 0;

static void http_init() {
    if (http_initialized) return;
    #ifdef _WIN32
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
    #endif
    http_initialized = 1;
}

static int parse_url(const char* url, char* host, size_t host_size, int* port, char* path, size_t path_size) {
    if (!url || !host || !port || !path || host_size == 0 || path_size == 0) return 0;

    const char* start = url;

    if (strncmp(url, "http://", 7) == 0) {
        start = url + 7;
        *port = 80;
    } else if (strncmp(url, "https://", 8) == 0) {
        return 0;
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

static HttpResponse* http_request(const char* method, const char* url, const char* body, const char* content_type) {
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

    if (!parse_url(url, host, sizeof(host), &port, path, sizeof(path))) {
        response->error = string_new("HTTPS not supported in basic implementation");
        return response;
    }

    struct hostent* server = gethostbyname(host);
    if (!server) {
        response->error = string_new("Could not resolve host");
        return response;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        response->error = string_new("Could not create socket");
        return response;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_port = htons(port);

    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sockfd);
        response->error = string_new("Connection failed");
        return response;
    }
    
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
    
    if (send(sockfd, request, request_len, 0) < 0) {
        close(sockfd);
        response->error = string_new("Send failed");
        return response;
    }
    
    char buffer[8192];
    char* full_response = (char*)malloc(1);
    if (!full_response) {
        close(sockfd);
        response->error = string_new("Out of memory");
        return response;
    }
    full_response[0] = '\0';
    size_t total_len = 0;
    int n;
    
    while ((n = recv(sockfd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[n] = '\0';
        char* new_resp = (char*)realloc(full_response, total_len + n + 1);
        if (!new_resp) {
            free(full_response);
            close(sockfd);
            response->error = string_new("Out of memory reading response");
            return response;
        }
        full_response = new_resp;
        memcpy(full_response + total_len, buffer, n);
        total_len += n;
        full_response[total_len] = '\0';
    }
    
    close(sockfd);
    
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

#endif // AETHER_HAS_NETWORKING
