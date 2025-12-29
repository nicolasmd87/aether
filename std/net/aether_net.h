#ifndef AETHER_NET_H
#define AETHER_NET_H

#include "../string/aether_string.h"

typedef struct Socket Socket;
typedef struct ServerSocket ServerSocket;

Socket* aether_socket_connect(AetherString* host, int port);
int aether_socket_send(Socket* sock, AetherString* data);
AetherString* aether_socket_receive(Socket* sock, int max_bytes);
int aether_socket_close(Socket* sock);

ServerSocket* aether_server_create(int port);
Socket* aether_server_accept(ServerSocket* server);
int aether_server_close(ServerSocket* server);

#endif

