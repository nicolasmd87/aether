#include "test_harness.h"
#include "../../std/net/aether_http_server.h"
#include <string.h>
#include <stdlib.h>

// HTTP Server Implementation Tests

int test_http_request_parsing_get() {
    printf("Test: HTTP Request Parsing (GET)... ");
    const char* raw_request = 
        "GET /api/users/123 HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "User-Agent: AetherTest/1.0\r\n"
        "Accept: application/json\r\n"
        "\r\n";
    
    HttpRequest* req = aether_http_parse_request(raw_request);
    ASSERT_NOT_NULL(req);
    ASSERT_STREQ("GET", req->method);
    ASSERT_STREQ("/api/users/123", req->path);
    
    const char* host = aether_http_get_header(req, "Host");
    ASSERT_NOT_NULL(host);
    ASSERT_STREQ("localhost:8080", host);
    
    aether_http_request_free(req);
    printf("PASS\\n");
    return 0;
}

int test_http_request_parsing_post_with_body() {
    printf("Test: HTTP Request Parsing (POST with body)... ");
    const char* raw_request = 
        "POST /api/todos HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 27\r\n"
        "\r\n"
        "{\"title\":\"Test todo item\"}";
    
    HttpRequest* req = aether_http_parse_request(raw_request);
    ASSERT_NOT_NULL(req);
    ASSERT_STREQ("POST", req->method);
    ASSERT_STREQ("/api/todos", req->path);
    ASSERT_NOT_NULL(req->body);
    ASSERT_STREQ("{\"title\":\"Test todo item\"}", req->body);
    
    const char* content_type = aether_http_get_header(req, "Content-Type");
    ASSERT_STREQ("application/json", content_type);
    
    aether_http_request_free(req);
}

TEST(http_query_params_parsing) {
    const char* raw_request = 
        "GET /api/search?q=aether&limit=10&offset=20 HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";
    
    HttpRequest* req = aether_http_parse_request(raw_request);
    ASSERT_NOT_NULL(req);
    
    const char* q = aether_http_get_query_param(req, "q");
    const char* limit = aether_http_get_query_param(req, "limit");
    const char* offset = aether_http_get_query_param(req, "offset");
    
    ASSERT_NOT_NULL(q);
    ASSERT_STREQ("aether", q);
    ASSERT_NOT_NULL(limit);
    ASSERT_STREQ("10", limit);
    ASSERT_NOT_NULL(offset);
    ASSERT_STREQ("20", offset);
    
    aether_http_request_free(req);
}

TEST(http_response_creation) {
    HttpServerResponse* res = aether_http_response_create();
    ASSERT_NOT_NULL(res);
    ASSERT_EQ(200, res->status_code);
    
    aether_http_response_set_status(res, 404);
    ASSERT_EQ(404, res->status_code);
    
    aether_http_response_set_header(res, "Content-Type", "text/plain");
    aether_http_response_set_body(res, "Not Found");
    
    char* serialized = aether_http_response_serialize(res);
    ASSERT_NOT_NULL(serialized);
    ASSERT_TRUE(strstr(serialized, "HTTP/1.1 404 Not Found") != NULL);
    ASSERT_TRUE(strstr(serialized, "Content-Type: text/plain") != NULL);
    ASSERT_TRUE(strstr(serialized, "Not Found") != NULL);

    free(serialized);
    aether_http_server_response_free(res);
}

TEST(http_response_json) {
    HttpServerResponse* res = aether_http_response_create();
    aether_http_response_json(res, "{\"status\":\"ok\",\"count\":42}");
    
    char* serialized = aether_http_response_serialize(res);
    ASSERT_NOT_NULL(serialized);
    ASSERT_TRUE(strstr(serialized, "Content-Type: application/json") != NULL);
    ASSERT_TRUE(strstr(serialized, "{\"status\":\"ok\",\"count\":42}") != NULL);

    free(serialized);
    aether_http_server_response_free(res);
}

TEST(http_route_exact_match) {
    HttpRequest req = {0};
    req.method = "GET";
    req.path = "/api/users";
    
    int result = aether_http_route_matches("/api/users", "/api/users", &req);
    ASSERT_EQ(1, result);
    
    result = aether_http_route_matches("/api/users", "/api/todos", &req);
    ASSERT_EQ(0, result);
}

TEST(http_route_param_extraction) {
    HttpRequest req = {0};
    req.method = "GET";
    req.path = "/api/users/123";
    req.param_count = 0;
    
    int result = aether_http_route_matches("/api/users/:id", "/api/users/123", &req);
    ASSERT_EQ(1, result);
    ASSERT_EQ(1, req.param_count);
    
    const char* id = aether_http_get_path_param(&req, "id");
    ASSERT_NOT_NULL(id);
    ASSERT_STREQ("123", id);
}

TEST(http_route_multiple_params) {
    HttpRequest req = {0};
    req.method = "GET";
    req.path = "/api/users/42/posts/99";
    req.param_count = 0;
    
    int result = aether_http_route_matches("/api/users/:userId/posts/:postId", 
                                           "/api/users/42/posts/99", &req);
    ASSERT_EQ(1, result);
    ASSERT_EQ(2, req.param_count);
    
    const char* user_id = aether_http_get_path_param(&req, "userId");
    const char* post_id = aether_http_get_path_param(&req, "postId");
    
    ASSERT_NOT_NULL(user_id);
    ASSERT_STREQ("42", user_id);
    ASSERT_NOT_NULL(post_id);
    ASSERT_STREQ("99", post_id);
}

TEST(http_route_wildcard_match) {
    HttpRequest req = {0};
    req.method = "GET";
    req.path = "/static/css/main.css";
    req.param_count = 0;
    
    int result = aether_http_route_matches("/static/*", "/static/css/main.css", &req);
    ASSERT_EQ(1, result);
}

TEST(http_server_status_text) {
    ASSERT_STREQ("OK", aether_http_status_text(200));
    ASSERT_STREQ("Created", aether_http_status_text(201));
    ASSERT_STREQ("No Content", aether_http_status_text(204));
    ASSERT_STREQ("Bad Request", aether_http_status_text(400));
    ASSERT_STREQ("Not Found", aether_http_status_text(404));
    ASSERT_STREQ("Internal Server Error", aether_http_status_text(500));
}

TEST(http_server_create_and_free) {
    HttpServer* server = aether_http_server_create(8080);
    ASSERT_NOT_NULL(server);
    ASSERT_EQ(8080, server->port);
    ASSERT_EQ(-1, server->socket_fd);
    ASSERT_EQ(0, server->is_running);
    
    aether_http_server_free(server);
}

TEST(http_server_add_routes) {
    HttpServer* server = aether_http_server_create(8080);
    
    // Simple handler that does nothing
    void test_handler(HttpRequest* req, HttpServerResponse* res, void* data) {
        aether_http_response_set_body(res, "test");
    }
    
    aether_http_server_get(server, "/test", test_handler, NULL);
    
    ASSERT_NOT_NULL(server->routes);
    ASSERT_STREQ("GET", server->routes->method);
    ASSERT_STREQ("/test", server->routes->path_pattern);
    
    aether_http_server_free(server);
}

TEST(http_server_middleware_chain) {
    HttpServer* server = aether_http_server_create(8080);
    
    // Simple middleware that does nothing
    int test_middleware(HttpRequest* req, HttpServerResponse* res, void* data) {
        return 1;
    }
    
    aether_http_server_use_middleware(server, test_middleware, NULL);
    
    ASSERT_NOT_NULL(server->middleware_chain);
    ASSERT_NOT_NULL(server->middleware_chain->middleware);
    
    aether_http_server_free(server);
}

int main() {
    printf("HTTP Server Implementation Tests\n");
    printf("================================\n\n");
    
    int failures = 0;
    
    failures += test_http_request_parsing_get();
    failures += test_http_request_parsing_post_with_body();
    failures += test_http_query_params_parsing();
    failures += test_http_response_creation();
    failures += test_http_response_json();
    failures += test_http_route_exact_match();
    failures += test_http_route_param_extraction();
    failures += test_http_route_multiple_params();
    failures += test_http_route_wildcard_match();
    failures += test_http_server_status_text();
    failures += test_http_server_create_and_free();
    failures += test_http_server_add_routes();
    failures += test_http_server_middleware_chain();
    
    printf("\n================================\n");
    if (failures == 0) {
        printf("ALL TESTS PASSED ✓\n");
    } else {
        printf("%d TESTS FAILED ✗\n", failures);
    }
    
    return failures;
}
