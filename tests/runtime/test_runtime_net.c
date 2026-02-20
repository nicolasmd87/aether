#include "test_harness.h"
#include "../../std/net/aether_net.h"
#include "../../std/string/aether_string.h"

TEST_CATEGORY(socket_null_handling, TEST_CATEGORY_NETWORK) {
    int result = tcp_send(NULL, NULL);
    ASSERT_EQ(-1, result);

    AetherString* received = tcp_receive(NULL, 1024);
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
    AetherString* host = string_new("invalid.host.that.does.not.exist.12345");
    TcpSocket* sock = tcp_connect(host, 80);
    ASSERT_NULL(sock);
    string_release(host);
#else
    ASSERT_TRUE(1);  // DNS resolution can hang on Windows
#endif
}

TEST_CATEGORY(server_create_invalid_port, TEST_CATEGORY_NETWORK) {
    TcpServer* server = tcp_listen(-1);
    ASSERT_NULL(server);
}
