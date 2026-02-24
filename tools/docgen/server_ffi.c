// FFI helpers for Aether documentation server

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>

// Create a listening socket on the given port
int socket_create(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -2;
    }

    if (listen(fd, 128) < 0) {
        close(fd);
        return -3;
    }

    return fd;
}

// Accept a connection
int socket_accept(int fd) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    return accept(fd, (struct sockaddr*)&client_addr, &client_len);
}

// Read from socket
int socket_read(int fd, char* buf, int size) {
    return (int)read(fd, buf, size);
}

// Write to socket
int socket_write(int fd, const char* data, int len) {
    return (int)write(fd, data, len);
}

// Close socket
void socket_close(int fd) {
    close(fd);
}

// Allocate a string buffer
char* allocate_string(int size) {
    char* s = (char*)calloc(size + 1, 1);
    return s;
}

void free_string(char* s) {
    if (s) free(s);
}

// Read entire file into string
const char* file_read(const char* path) {
    static char content[2 * 1024 * 1024]; // 2MB max
    content[0] = '\0';

    FILE* f = fopen(path, "rb");
    if (!f) return content;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size > (long)(sizeof(content) - 1)) {
        size = sizeof(content) - 1;
    }

    size_t read_size = fread(content, 1, size, f);
    content[read_size] = '\0';
    fclose(f);

    return content;
}

// String length (named cstr_length to avoid conflict with stdlib AetherString version)
int cstr_length(const char* s) {
    if (!s) return 0;
    return (int)strlen(s);
}

// Check if string ends with suffix
int ends_with(const char* s, const char* suffix) {
    if (!s || !suffix) return 0;
    size_t s_len = strlen(s);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > s_len) return 0;
    return strcmp(s + s_len - suffix_len, suffix) == 0 ? 1 : 0;
}

// Parse HTTP path from request
const char* parse_http_path(const char* request) {
    static char path[512];
    path[0] = '/';
    path[1] = '\0';

    if (!request) return path;

    const char* start = strstr(request, "GET ");
    if (!start) start = strstr(request, "POST ");
    if (!start) return path;

    start += 4;
    while (*start == ' ') start++;

    const char* end = strchr(start, ' ');
    if (!end) return path;

    size_t len = end - start;
    if (len >= sizeof(path)) len = sizeof(path) - 1;

    strncpy(path, start, len);
    path[len] = '\0';

    return path;
}

// String concatenation (alternating static buffers to allow chaining)
const char* str_concat(const char* a, const char* b) {
    static char buf[2][1024];
    static int which = 0;
    char* result = buf[which];
    which = !which;
    result[0] = '\0';
    if (a) strncat(result, a, 1023);
    if (b) strncat(result, b, 1023 - strlen(result));
    return result;
}

// Build HTTP response
const char* build_response(int status, const char* content_type, const char* body) {
    static char response[2 * 1024 * 1024 + 1024];
    const char* status_text = status == 200 ? "OK" : "Not Found";
    size_t body_len = body ? strlen(body) : 0;

    snprintf(response, sizeof(response),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-cache\r\n"
        "\r\n"
        "%s",
        status, status_text, content_type, body_len, body ? body : "");

    return response;
}
