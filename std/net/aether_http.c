#include "aether_http.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
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

static int parse_url(const char* url, char* host, int* port, char* path) {
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
        int host_len = colon - start;
        strncpy(host, start, host_len);
        host[host_len] = '\0';
        *port = atoi(colon + 1);
        if (slash) {
            strcpy(path, slash);
        } else {
            strcpy(path, "/");
        }
    } else if (slash) {
        int host_len = slash - start;
        strncpy(host, start, host_len);
        host[host_len] = '\0';
        strcpy(path, slash);
    } else {
        strcpy(host, start);
        strcpy(path, "/");
    }
    
    return 1;
}

static HttpResponse* http_request(const char* method, AetherString* url, AetherString* body, AetherString* content_type) {
    http_init();
    
    HttpResponse* response = (HttpResponse*)malloc(sizeof(HttpResponse));
    response->status_code = 0;
    response->body = NULL;
    response->headers = NULL;
    response->error = NULL;
    
    char host[256];
    char path[1024];
    int port;
    
    if (!parse_url(url->data, host, &port, path)) {
        response->error = aether_string_new("HTTPS not supported in basic implementation");
        return response;
    }
    
    struct hostent* server = gethostbyname(host);
    if (!server) {
        response->error = aether_string_new("Could not resolve host");
        return response;
    }
    
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        response->error = aether_string_new("Could not create socket");
        return response;
    }
    
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_port = htons(port);
    
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sockfd);
        response->error = aether_string_new("Connection failed");
        return response;
    }
    
    char request[4096];
    int request_len = 0;
    
    if (body && body->length > 0) {
        request_len = snprintf(request, sizeof(request),
            "%s %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            method, path, host,
            content_type ? content_type->data : "application/x-www-form-urlencoded",
            body->length,
            body->data);
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
        response->error = aether_string_new("Send failed");
        return response;
    }
    
    char buffer[8192];
    char* full_response = (char*)malloc(1);
    full_response[0] = '\0';
    size_t total_len = 0;
    int n;
    
    while ((n = recv(sockfd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[n] = '\0';
        full_response = (char*)realloc(full_response, total_len + n + 1);
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
        
        response->headers = aether_string_new(full_response);
        response->body = aether_string_new(header_end + 4);
    } else {
        response->body = aether_string_new(full_response);
    }
    
    free(full_response);
    return response;
}

HttpResponse* aether_http_get(AetherString* url) {
    return http_request("GET", url, NULL, NULL);
}

HttpResponse* aether_http_post(AetherString* url, AetherString* body, AetherString* content_type) {
    return http_request("POST", url, body, content_type);
}

HttpResponse* aether_http_put(AetherString* url, AetherString* body, AetherString* content_type) {
    return http_request("PUT", url, body, content_type);
}

HttpResponse* aether_http_delete(AetherString* url) {
    return http_request("DELETE", url, NULL, NULL);
}

void aether_http_response_free(HttpResponse* response) {
    if (!response) return;
    if (response->body) aether_string_release(response->body);
    if (response->headers) aether_string_release(response->headers);
    if (response->error) aether_string_release(response->error);
    free(response);
}

