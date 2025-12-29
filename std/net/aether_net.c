#include "aether_net.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define close closesocket
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <arpa/inet.h>
#endif

struct Socket {
    int fd;
    int connected;
};

struct ServerSocket {
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

Socket* aether_socket_connect(AetherString* host, int port) {
    net_init();
    
    struct hostent* server = gethostbyname(host->data);
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
    
    Socket* sock = (Socket*)malloc(sizeof(Socket));
    sock->fd = sockfd;
    sock->connected = 1;
    return sock;
}

int aether_socket_send(Socket* sock, AetherString* data) {
    if (!sock || !sock->connected || !data) return -1;
    
    int sent = send(sock->fd, data->data, data->length, 0);
    return sent;
}

AetherString* aether_socket_receive(Socket* sock, int max_bytes) {
    if (!sock || !sock->connected) return NULL;
    
    char* buffer = (char*)malloc(max_bytes + 1);
    int received = recv(sock->fd, buffer, max_bytes, 0);
    
    if (received <= 0) {
        free(buffer);
        sock->connected = 0;
        return NULL;
    }
    
    buffer[received] = '\0';
    AetherString* result = aether_string_new_with_length(buffer, received);
    free(buffer);
    return result;
}

int aether_socket_close(Socket* sock) {
    if (!sock) return -1;
    
    if (sock->connected) {
        close(sock->fd);
        sock->connected = 0;
    }
    
    free(sock);
    return 0;
}

ServerSocket* aether_server_create(int port) {
    net_init();
    
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
    
    ServerSocket* server = (ServerSocket*)malloc(sizeof(ServerSocket));
    server->fd = sockfd;
    server->port = port;
    return server;
}

Socket* aether_server_accept(ServerSocket* server) {
    if (!server) return NULL;
    
    struct sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);
    
    int newsockfd = accept(server->fd, (struct sockaddr*)&cli_addr, &clilen);
    if (newsockfd < 0) {
        return NULL;
    }
    
    Socket* sock = (Socket*)malloc(sizeof(Socket));
    sock->fd = newsockfd;
    sock->connected = 1;
    return sock;
}

int aether_server_close(ServerSocket* server) {
    if (!server) return -1;
    
    close(server->fd);
    free(server);
    return 0;
}

