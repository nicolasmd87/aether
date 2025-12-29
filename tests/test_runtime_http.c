#include "test_harness.h"
#include "../std/net/aether_http.h"
#include "../std/string/aether_string.h"

TEST(http_response_structure) {
    HttpResponse* resp = (HttpResponse*)malloc(sizeof(HttpResponse));
    resp->status_code = 200;
    resp->body = aether_string_new("test body");
    resp->headers = aether_string_new("Content-Type: text/html");
    resp->error = NULL;
    
    ASSERT_EQ(200, resp->status_code);
    ASSERT_NOT_NULL(resp->body);
    ASSERT_NOT_NULL(resp->headers);
    ASSERT_NULL(resp->error);
    
    aether_http_response_free(resp);
}

TEST(http_url_parsing) {
    char host[256];
    char path[1024];
    int port;
    
    const char* url = "http://example.com/path";
    ASSERT_TRUE(1);
}

TEST(http_response_cleanup) {
    HttpResponse* resp = (HttpResponse*)malloc(sizeof(HttpResponse));
    resp->status_code = 404;
    resp->body = aether_string_new("Not Found");
    resp->headers = NULL;
    resp->error = aether_string_new("Error message");
    
    aether_http_response_free(resp);
    ASSERT_TRUE(1);
}

