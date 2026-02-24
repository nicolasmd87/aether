// FFI helpers for Aether benchmark server
// These functions are called from server.ae via extern declarations

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
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

// Read entire file into string (static buffer)
const char* file_read(const char* path) {
    static char content[2 * 1024 * 1024];
    content[0] = '\0';

    FILE* f = fopen(path, "rb");
    if (!f) return content;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size > (long)(sizeof(content) - 1)) {
        size = sizeof(content) - 1;
    }

    size_t nread = fread(content, 1, size, f);
    content[nread] = '\0';
    fclose(f);

    return content;
}

// Check if file exists
int file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 ? 1 : 0;
}

// Get current working directory
char* getcwd_str(void) {
    static char buf[1024];
    if (getcwd(buf, sizeof(buf)) == NULL) {
        return ".";
    }
    return buf;
}

// String helper: allocate empty string of given size
char* allocate_string(int size) {
    char* s = malloc(size + 1);
    memset(s, 0, size + 1);
    return s;
}

void free_string(char* s) {
    if (s) free(s);
}

// String helper: get character at index
char char_at(const char* s, int i) {
    return s[i];
}

// String helper: get substring (static buffer)
const char* substring(const char* s, int start, int len) {
    static char result[1024];
    if (len >= (int)sizeof(result)) len = sizeof(result) - 1;
    strncpy(result, s + start, len);
    result[len] = '\0';
    return result;
}

// String helper: check if string starts with prefix
int starts_with(const char* s, const char* prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0 ? 1 : 0;
}

// String helper: check if string ends with suffix
int ends_with(const char* s, const char* suffix) {
    size_t slen = strlen(s);
    size_t sufflen = strlen(suffix);
    if (sufflen > slen) return 0;
    return strcmp(s + slen - sufflen, suffix) == 0 ? 1 : 0;
}

// String helper: get length
int cstr_length(const char* s) {
    return s ? (int)strlen(s) : 0;
}

// Parse HTTP request path (e.g., "GET /index.html HTTP/1.1" -> "/index.html")
const char* parse_http_path(const char* request) {
    static char result[512];
    result[0] = '/';
    result[1] = '\0';

    if (!request) return result;

    const char* p = strchr(request, ' ');
    if (!p) return result;
    p++;

    const char* end = strchr(p, ' ');
    if (!end) end = p + strlen(p);

    int len = end - p;
    if (len >= (int)sizeof(result)) len = sizeof(result) - 1;
    strncpy(result, p, len);
    result[len] = '\0';

    return result;
}

// String helper: int to string
char* int_to_string(int n) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "%d", n);
    return buf;
}

// String concatenation (alternating static buffers to allow chaining)
const char* str_concat(const char* a, const char* b) {
    static char buf[2][4096];
    static int which = 0;
    char* result = buf[which];
    which = !which;
    result[0] = '\0';
    if (a) strncat(result, a, 4095);
    if (b) strncat(result, b, 4095 - strlen(result));
    return result;
}

// Build complete HTTP response (static buffer, handles large bodies)
const char* build_response(int status, const char* content_type, const char* body) {
    static char response[2 * 1024 * 1024 + 4096];
    const char* status_text = status == 200 ? "OK" : "Not Found";
    size_t body_len = body ? strlen(body) : 0;

    snprintf(response, sizeof(response),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n"
        "%s",
        status, status_text, content_type, body_len, body ? body : "");

    return response;
}
