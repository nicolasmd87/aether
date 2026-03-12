#ifndef AETHER_NET_H
#define AETHER_NET_H

#include <stddef.h>

typedef struct TcpSocket TcpSocket;
typedef struct TcpServer TcpServer;

// TCP Client
TcpSocket* tcp_connect(const char* host, int port);
int tcp_send(TcpSocket* sock, const char* data);
char* tcp_receive(TcpSocket* sock, int max_bytes);
int tcp_close(TcpSocket* sock);

// TCP Server
TcpServer* tcp_listen(int port);
TcpSocket* tcp_accept(TcpServer* server);
int tcp_server_close(TcpServer* server);

#endif
