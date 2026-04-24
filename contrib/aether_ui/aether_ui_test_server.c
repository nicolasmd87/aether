// AetherUIDriver — shared HTTP test server.
//
// Platform-neutral HTTP + routing + JSON for the AetherUIDriver. The
// three native backends (GTK4, AppKit, Win32) provide a small hook table
// (see aether_ui_test_server.h) and call aether_ui_test_server_start().
//
// Before this extraction, each backend had its own ~500 LOC copy of the
// socket accept loop, HTTP parser, URL router, JSON emitter, and sealed-
// widget bookkeeping. This file consolidates all of that to a single
// source of truth so feature additions (new endpoints, new filters)
// land on every backend at once.
//
// Socket layer:
//   _WIN32   → winsock2 (WSAStartup, closesocket, ioctlsocket)
//   POSIX    → <sys/socket.h> + <unistd.h>
//
// Threading:
//   _WIN32   → CreateThread
//   POSIX    → pthread_create + pthread_detach

#include "aether_ui_test_server.h"
#include "aether_ui_backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET aether_sock_t;
#define AETHER_SOCK_INVALID INVALID_SOCKET
#define aether_close_socket closesocket
#define aether_socket_recv(s, buf, len) recv((s), (buf), (len), 0)
#define aether_socket_send(s, buf, len) send((s), (buf), (len), 0)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
typedef int aether_sock_t;
#define AETHER_SOCK_INVALID (-1)
#define aether_close_socket close
#define aether_socket_recv(s, buf, len) read((s), (buf), (len))
#define aether_socket_send(s, buf, len) write((s), (buf), (len))
#endif

// Reactive state externs from the backend-neutral state layer (each
// backend defines these; used directly for /state/{id} endpoints).
extern double aether_ui_state_get(int handle);

// ---------------------------------------------------------------------------
// Sealed widget list + banner handle.
// ---------------------------------------------------------------------------
static int* sealed_widgets = NULL;
static int  sealed_count = 0;
static int  sealed_capacity = 0;
static int  banner_handle = 0;

void aether_ui_test_server_set_banner(int handle) {
    banner_handle = handle;
}

int aether_ui_test_server_banner_handle(void) {
    return banner_handle;
}

void aether_ui_test_server_seal_widget(int handle) {
    // Ignore duplicates so repeated seals are idempotent.
    for (int i = 0; i < sealed_count; i++) {
        if (sealed_widgets[i] == handle) return;
    }
    if (sealed_count >= sealed_capacity) {
        sealed_capacity = sealed_capacity == 0 ? 32 : sealed_capacity * 2;
        sealed_widgets = (int*)realloc(sealed_widgets,
                                       sizeof(int) * sealed_capacity);
    }
    sealed_widgets[sealed_count++] = handle;
}

int aether_ui_test_server_is_sealed(int handle) {
    for (int i = 0; i < sealed_count; i++) {
        if (sealed_widgets[i] == handle) return 1;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// HTTP parsing helpers.
// ---------------------------------------------------------------------------
static int parse_http_request(const char* req, char* path, int pathsize) {
    int method = 0;
    if (strncmp(req, "POST", 4) == 0) method = 1;
    const char* p = strchr(req, ' ');
    if (!p) return -1;
    p++;
    const char* end = strchr(p, ' ');
    if (!end) end = strchr(p, '\r');
    if (!end) end = p + strlen(p);
    int len = (int)(end - p);
    if (len >= pathsize) len = pathsize - 1;
    memcpy(path, p, len);
    path[len] = '\0';
    return method;
}

static int extract_id_from_path(const char* path, const char* prefix) {
    size_t plen = strlen(prefix);
    if (strncmp(path, prefix, plen) != 0) return -1;
    return atoi(path + plen);
}

static const char* extract_query_param(const char* path, const char* key) {
    char needle[64];
    snprintf(needle, sizeof(needle), "%s=", key);
    const char* p = strstr(path, needle);
    if (!p) return NULL;
    return p + strlen(needle);
}

// URL-decode s in place (tolerant: passes through malformed escapes).
static void url_decode(char* s) {
    char* out = s;
    for (char* in = s; *in; in++) {
        if (*in == '%' && in[1] && in[2]) {
            int hi = in[1], lo = in[2];
            hi = hi >= 'a' ? hi - 'a' + 10 : hi >= 'A' ? hi - 'A' + 10 : hi - '0';
            lo = lo >= 'a' ? lo - 'a' + 10 : lo >= 'A' ? lo - 'A' + 10 : lo - '0';
            *out++ = (char)(hi * 16 + lo);
            in += 2;
        } else if (*in == '+') {
            *out++ = ' ';
        } else {
            *out++ = *in;
        }
    }
    *out = '\0';
}

// ---------------------------------------------------------------------------
// JSON emission + HTTP response send.
// ---------------------------------------------------------------------------
static void send_http(aether_sock_t fd, int status, const char* status_text,
                      const char* content_type, const char* body) {
    char header[512];
    int bodylen = body ? (int)strlen(body) : 0;
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n"
        "\r\n",
        status, status_text, content_type, bodylen);
    aether_socket_send(fd, header, hlen);
    if (body && bodylen > 0) aether_socket_send(fd, body, bodylen);
}

static int widget_to_json(const AetherDriverHooks* h, int handle,
                          char* buf, int bufsize) {
    const char* type = h->widget_type(handle);
    if (!type || strcmp(type, "null") == 0) {
        return snprintf(buf, bufsize, "{\"id\":%d,\"type\":\"null\"}", handle);
    }

    char text[1024];
    h->widget_text_into(handle, text, sizeof(text));

    // Escape " and \ and \n for JSON.
    char esc[2048];
    int ei = 0;
    for (int i = 0; text[i] && ei < (int)sizeof(esc) - 2; i++) {
        char ch = text[i];
        if (ch == '"' || ch == '\\') { esc[ei++] = '\\'; esc[ei++] = ch; }
        else if (ch == '\n') { esc[ei++] = '\\'; esc[ei++] = 'n'; }
        else if (ch == '\r') { /* skip */ }
        else esc[ei++] = ch;
    }
    esc[ei] = '\0';

    int visible   = h->widget_visible(handle);
    int sealed    = aether_ui_test_server_is_sealed(handle);
    int is_banner = (handle == banner_handle) ? 1 : 0;
    int parent    = h->widget_parent(handle);

    int n = snprintf(buf, bufsize,
        "{\"id\":%d,\"type\":\"%s\",\"text\":\"%s\",\"visible\":%s,"
        "\"sealed\":%s,\"banner\":%s,\"parent\":%d",
        handle, type, esc,
        visible ? "true" : "false",
        sealed  ? "true" : "false",
        is_banner ? "true" : "false",
        parent);

    if (strcmp(type, "toggle") == 0) {
        n += snprintf(buf + n, bufsize - n, ",\"active\":%s",
                      h->toggle_active(handle) ? "true" : "false");
    } else if (strcmp(type, "slider") == 0) {
        n += snprintf(buf + n, bufsize - n, ",\"value\":%.2f",
                      h->slider_value(handle));
    } else if (strcmp(type, "progressbar") == 0) {
        n += snprintf(buf + n, bufsize - n, ",\"value\":%.2f",
                      h->progressbar_fraction(handle));
    }
    n += snprintf(buf + n, bufsize - n, "}");
    return n;
}

// ---------------------------------------------------------------------------
// Request dispatch.
// ---------------------------------------------------------------------------
static void dispatch_and_reply(aether_sock_t client_fd,
                                const AetherDriverHooks* h,
                                AetherDriverActionCtx* ctx,
                                const char* ok_msg) {
    h->dispatch_action(ctx);
    switch (ctx->result) {
        case 0:
            send_http(client_fd, 200, "OK", "text/plain", ok_msg);
            break;
        case 1:
            send_http(client_fd, 403, "Forbidden", "text/plain",
                      "widget is sealed");
            break;
        case 2:
            send_http(client_fd, 403, "Forbidden", "text/plain",
                      "banner is protected from the test API");
            break;
        default:
            send_http(client_fd, 404, "Not Found", "text/plain",
                      "widget not found");
            break;
    }
}

static void handle_request(aether_sock_t client_fd, const AetherDriverHooks* h) {
    char req[4096];
    int n = (int)aether_socket_recv(client_fd, req, sizeof(req) - 1);
    if (n <= 0) { aether_close_socket(client_fd); return; }
    req[n] = '\0';

    char path[1024];
    int method = parse_http_request(req, path, sizeof(path));
    if (method < 0) {
        send_http(client_fd, 400, "Bad Request", "text/plain", "bad request");
        aether_close_socket(client_fd);
        return;
    }

    // GET /widgets[?type=X][&text=Y]
    if (method == 0 && strncmp(path, "/widgets", 8) == 0
        && (path[8] == '\0' || path[8] == '?')) {
        const char* filter_type = extract_query_param(path, "type");
        const char* filter_text = extract_query_param(path, "text");
        char ft[128] = "", fx[128] = "";
        if (filter_type) {
            strncpy(ft, filter_type, sizeof(ft) - 1);
            char* amp = strchr(ft, '&'); if (amp) *amp = '\0';
            url_decode(ft);
        }
        if (filter_text) {
            strncpy(fx, filter_text, sizeof(fx) - 1);
            char* amp = strchr(fx, '&'); if (amp) *amp = '\0';
            url_decode(fx);
        }

        int total = h->widget_count();
        // Per-widget JSON caps at ~512 bytes; allocate a generous buffer.
        char* body = (char*)malloc((size_t)total * 512 + 64);
        int pos = 0, first = 1;
        pos += sprintf(body + pos, "[");
        for (int i = 1; i <= total; i++) {
            const char* type = h->widget_type(i);
            if (!type || strcmp(type, "null") == 0) continue;
            if (ft[0] && strcmp(type, ft) != 0) continue;
            if (fx[0]) {
                char txt[1024];
                h->widget_text_into(i, txt, sizeof(txt));
                if (strcmp(txt, fx) != 0) continue;
            }
            if (!first) pos += sprintf(body + pos, ",");
            first = 0;
            pos += widget_to_json(h, i, body + pos, 512);
        }
        pos += sprintf(body + pos, "]");
        send_http(client_fd, 200, "OK", "application/json", body);
        free(body);
    } else if (method == 0 && strncmp(path, "/widget/", 8) == 0
               && strstr(path, "/children")) {
        int id = extract_id_from_path(path, "/widget/");
        if (!h->widget_children) {
            send_http(client_fd, 501, "Not Implemented", "text/plain",
                      "/children not supported by this backend");
        } else {
            int kids[256];
            int n = h->widget_children(id, kids, 256);
            if (n < 0) {
                send_http(client_fd, 404, "Not Found", "text/plain",
                          "widget not found");
            } else {
                char* body = (char*)malloc((size_t)n * 512 + 64);
                int pos = 0;
                pos += sprintf(body + pos, "[");
                for (int i = 0; i < n; i++) {
                    if (i > 0) pos += sprintf(body + pos, ",");
                    pos += widget_to_json(h, kids[i], body + pos, 512);
                }
                pos += sprintf(body + pos, "]");
                send_http(client_fd, 200, "OK", "application/json", body);
                free(body);
            }
        }
    } else if (method == 0 && strcmp(path, "/screenshot") == 0) {
        if (!h->screenshot_png) {
            send_http(client_fd, 501, "Not Implemented", "text/plain",
                      "/screenshot not supported by this backend");
        } else {
            unsigned char* data = NULL;
            size_t len = 0;
            if (h->screenshot_png(&data, &len) == 0 && data && len > 0) {
                char header[256];
                int hlen = snprintf(header, sizeof(header),
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: image/png\r\n"
                    "Content-Length: %zu\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "Connection: close\r\n\r\n", len);
                aether_socket_send(client_fd, header, hlen);
                aether_socket_send(client_fd, (const char*)data, (int)len);
                free(data);
            } else {
                send_http(client_fd, 500, "Error", "text/plain",
                          "screenshot capture failed");
            }
        }
    } else if (method == 0 && strncmp(path, "/widget/", 8) == 0) {
        int id = extract_id_from_path(path, "/widget/");
        if (id > 0) {
            char body[2048];
            widget_to_json(h, id, body, sizeof(body));
            send_http(client_fd, 200, "OK", "application/json", body);
        } else {
            send_http(client_fd, 404, "Not Found", "text/plain",
                      "widget not found");
        }
    } else if (method == 1 && strstr(path, "/click")) {
        AetherDriverActionCtx ctx = {0};
        ctx.action = AETHER_DRV_CLICK;
        ctx.handle = extract_id_from_path(path, "/widget/");
        dispatch_and_reply(client_fd, h, &ctx, "clicked");
    } else if (method == 1 && strstr(path, "/set_text")) {
        AetherDriverActionCtx ctx = {0};
        ctx.action = AETHER_DRV_SET_TEXT;
        ctx.handle = extract_id_from_path(path, "/widget/");
        const char* v = extract_query_param(path, "v");
        if (v) {
            strncpy(ctx.sval, v, sizeof(ctx.sval) - 1);
            char* amp = strchr(ctx.sval, '&'); if (amp) *amp = '\0';
            url_decode(ctx.sval);
        }
        dispatch_and_reply(client_fd, h, &ctx, "set");
    } else if (method == 1 && strstr(path, "/toggle")) {
        AetherDriverActionCtx ctx = {0};
        ctx.action = AETHER_DRV_TOGGLE;
        ctx.handle = extract_id_from_path(path, "/widget/");
        dispatch_and_reply(client_fd, h, &ctx, "toggled");
    } else if (method == 1 && strstr(path, "/set_value")) {
        AetherDriverActionCtx ctx = {0};
        ctx.action = AETHER_DRV_SET_VALUE;
        ctx.handle = extract_id_from_path(path, "/widget/");
        const char* v = extract_query_param(path, "v");
        if (v) ctx.dval = atof(v);
        dispatch_and_reply(client_fd, h, &ctx, "set");
    } else if (method == 0 && strncmp(path, "/state/", 7) == 0) {
        int id = extract_id_from_path(path, "/state/");
        if (id > 0) {
            char body[64];
            snprintf(body, sizeof(body), "%.2f", aether_ui_state_get(id));
            send_http(client_fd, 200, "OK", "text/plain", body);
        } else {
            send_http(client_fd, 404, "Not Found", "text/plain",
                      "state not found");
        }
    } else if (method == 1 && strstr(path, "/state/") && strstr(path, "/set")) {
        AetherDriverActionCtx ctx = {0};
        ctx.action = AETHER_DRV_SET_STATE;
        ctx.handle = extract_id_from_path(path, "/state/");
        const char* v = extract_query_param(path, "v");
        if (v) ctx.dval = atof(v);
        h->dispatch_action(&ctx);
        send_http(client_fd, 200, "OK", "text/plain", "set");
    } else {
        send_http(client_fd, 404, "Not Found", "text/plain",
                  "unknown endpoint");
    }

    aether_close_socket(client_fd);
}

// ---------------------------------------------------------------------------
// Accept-loop thread.
// ---------------------------------------------------------------------------
typedef struct {
    int port;
    const AetherDriverHooks* hooks;
} ServerArgs;

#ifdef _WIN32
static DWORD WINAPI server_thread(LPVOID arg) {
#else
static void* server_thread(void* arg) {
#endif
    ServerArgs* args = (ServerArgs*)arg;
    int port = args->port;
    const AetherDriverHooks* hooks = args->hooks;

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    aether_sock_t server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == AETHER_SOCK_INVALID) {
        fprintf(stderr, "AetherUIDriver: socket() failed\n");
#ifdef _WIN32
        WSACleanup();
        return 1;
#else
        return NULL;
#endif
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
               (const char*)&opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((unsigned short)port);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "AetherUIDriver: bind to port %d failed\n", port);
        aether_close_socket(server_fd);
#ifdef _WIN32
        WSACleanup();
        return 1;
#else
        return NULL;
#endif
    }
    if (listen(server_fd, 8) < 0) {
        aether_close_socket(server_fd);
#ifdef _WIN32
        WSACleanup();
        return 1;
#else
        return NULL;
#endif
    }

    fprintf(stderr, "AetherUIDriver: listening on http://127.0.0.1:%d\n", port);

    for (;;) {
        struct sockaddr_in caddr;
#ifdef _WIN32
        int caddr_len = sizeof(caddr);
#else
        socklen_t caddr_len = sizeof(caddr);
#endif
        aether_sock_t client_fd = accept(server_fd,
            (struct sockaddr*)&caddr, &caddr_len);
        if (client_fd == AETHER_SOCK_INVALID) break;
        handle_request(client_fd, hooks);
    }

    aether_close_socket(server_fd);
#ifdef _WIN32
    WSACleanup();
    return 0;
#else
    return NULL;
#endif
}

void aether_ui_test_server_start(int port, const AetherDriverHooks* hooks) {
    ServerArgs* args = (ServerArgs*)malloc(sizeof(ServerArgs));
    args->port = port;
    args->hooks = hooks;

#ifdef _WIN32
    CreateThread(NULL, 0, server_thread, args, 0, NULL);
#else
    pthread_t tid;
    pthread_create(&tid, NULL, server_thread, args);
    pthread_detach(tid);
#endif
}
