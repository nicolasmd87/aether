#include "test_harness.h"
#include "../../std/net/aether_net.h"

TEST_CATEGORY(socket_null_handling, TEST_CATEGORY_NETWORK) {
    int result = tcp_send(NULL, NULL);
    ASSERT_EQ(-1, result);

    char* received = tcp_receive(NULL, 1024);
    ASSERT_NULL(received);

    result = tcp_close(NULL);
    ASSERT_EQ(-1, result);
}

TEST_CATEGORY(server_null_handling, TEST_CATEGORY_NETWORK) {
    TcpSocket* sock = tcp_accept(NULL);
    ASSERT_NULL(sock);

    int result = tcp_server_close(NULL);
    ASSERT_EQ(-1, result);
}

TEST_CATEGORY(socket_connect_invalid_host, TEST_CATEGORY_NETWORK) {
#ifndef _WIN32
    TcpSocket* sock = tcp_connect("invalid.host.that.does.not.exist.12345", 80);
    ASSERT_NULL(sock);
#else
    ASSERT_TRUE(1);  // DNS resolution can hang on Windows
#endif
}

TEST_CATEGORY(server_create_invalid_port, TEST_CATEGORY_NETWORK) {
    TcpServer* server = tcp_listen(-1);
    ASSERT_NULL(server);
}
