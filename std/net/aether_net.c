#include "aether_net.h"
#include "../../runtime/config/aether_optimization_config.h"
#include "../../runtime/aether_sandbox.h"

#if !AETHER_HAS_NETWORKING
TcpSocket* tcp_connect(const char* h, int p) { (void)h; (void)p; return NULL; }
int tcp_send(TcpSocket* s, const char* d) { (void)s; (void)d; return 0; }
char* tcp_receive(TcpSocket* s, int m) { (void)s; (void)m; return NULL; }
int tcp_close(TcpSocket* s) { (void)s; return 0; }
TcpServer* tcp_listen(int p) { (void)p; return NULL; }
TcpSocket* tcp_accept(TcpServer* s) { (void)s; return NULL; }
int tcp_server_close(TcpServer* s) { (void)s; return 0; }
#else

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #ifdef _MSC_VER
        #pragma comment(lib, "ws2_32.lib")
    #endif
    #define close closesocket
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <arpa/inet.h>
#endif

struct TcpSocket {
    int fd;
    int connected;
};

struct TcpServer {
    int fd;
    int port;
};

static int net_initialized = 0;

static void net_init() {
    if (net_initialized) return;
    #ifdef _WIN32
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
    #endif
    net_initialized = 1;
}

TcpSocket* tcp_connect(const char* host, int port) {
    // Sandbox check: is TCP connect to this host allowed?
    if (!aether_sandbox_check("tcp", host)) return NULL;

    net_init();

    struct hostent* server = gethostbyname(host);
    if (!server) {
        return NULL;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        return NULL;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_port = htons(port);

    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sockfd);
        return NULL;
    }

    TcpSocket* sock = (TcpSocket*)malloc(sizeof(TcpSocket));
    if (!sock) { close(sockfd); return NULL; }
    sock->fd = sockfd;
    sock->connected = 1;
    return sock;
}

int tcp_send(TcpSocket* sock, const char* data) {
    if (!sock || !sock->connected || !data) return -1;

    int sent = send(sock->fd, data, strlen(data), 0);
    return sent;
}

char* tcp_receive(TcpSocket* sock, int max_bytes) {
    if (!sock || !sock->connected || max_bytes <= 0) return NULL;

    char* buffer = (char*)malloc(max_bytes + 1);
    if (!buffer) return NULL;
    int received = recv(sock->fd, buffer, max_bytes, 0);

    if (received <= 0) {
        free(buffer);
        sock->connected = 0;
        return NULL;
    }

    buffer[received] = '\0';
    return buffer;
}

int tcp_close(TcpSocket* sock) {
    if (!sock) return -1;

    if (sock->connected) {
        close(sock->fd);
        sock->connected = 0;
    }

    free(sock);
    return 0;
}

TcpServer* tcp_listen(int port) {
    // Sandbox check: is listening on this port allowed?
    if (!aether_sandbox_check("tcp_listen", "*")) return NULL;

    net_init();

    // Validate port range (1-65535)
    if (port < 1 || port > 65535) {
        return NULL;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        return NULL;
    }

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sockfd);
        return NULL;
    }

    if (listen(sockfd, 5) < 0) {
        close(sockfd);
        return NULL;
    }

    TcpServer* server = (TcpServer*)malloc(sizeof(TcpServer));
    if (!server) { close(sockfd); return NULL; }
    server->fd = sockfd;
    server->port = port;
    return server;
}

TcpSocket* tcp_accept(TcpServer* server) {
    if (!server) return NULL;

    struct sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);

    int newsockfd = accept(server->fd, (struct sockaddr*)&cli_addr, &clilen);
    if (newsockfd < 0) {
        return NULL;
    }

    TcpSocket* sock = (TcpSocket*)malloc(sizeof(TcpSocket));
    if (!sock) { close(newsockfd); return NULL; }
    sock->fd = newsockfd;
    sock->connected = 1;
    return sock;
}

int tcp_server_close(TcpServer* server) {
    if (!server) return -1;

    close(server->fd);
    free(server);
    return 0;
}

#endif // AETHER_HAS_NETWORKING
