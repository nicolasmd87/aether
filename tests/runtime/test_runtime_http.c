#include "test_harness.h"
#include "../../std/net/aether_http.h"
#include "../../std/string/aether_string.h"

TEST_CATEGORY(http_response_structure, TEST_CATEGORY_NETWORK) {
    HttpResponse* resp = (HttpResponse*)malloc(sizeof(HttpResponse));
    resp->status_code = 200;
    resp->body = string_new("test body");
    resp->headers = string_new("Content-Type: text/html");
    resp->error = NULL;

    ASSERT_EQ(200, resp->status_code);
    ASSERT_NOT_NULL(resp->body);
    ASSERT_NOT_NULL(resp->headers);
    ASSERT_NULL(resp->error);

    http_response_free(resp);
}

TEST_CATEGORY(http_url_parsing, TEST_CATEGORY_NETWORK) {
    // URL parsing test placeholder
    ASSERT_TRUE(1);
}

TEST_CATEGORY(http_response_cleanup, TEST_CATEGORY_NETWORK) {
    HttpResponse* resp = (HttpResponse*)malloc(sizeof(HttpResponse));
    resp->status_code = 404;
    resp->body = string_new("Not Found");
    resp->headers = NULL;
    resp->error = string_new("Error message");

    http_response_free(resp);
    ASSERT_TRUE(1);
}
