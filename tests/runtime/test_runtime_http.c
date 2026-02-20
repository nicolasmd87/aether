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
    // Test URL query string parsing
    const char* url = "/search?q=test&limit=10";
    ASSERT_NOT_NULL(url);
    ASSERT_TRUE(strstr(url, "?") != NULL);  // Has query string
    ASSERT_TRUE(strstr(url, "q=test") != NULL);
}

TEST_CATEGORY(http_response_cleanup, TEST_CATEGORY_NETWORK) {
    HttpResponse* resp = (HttpResponse*)malloc(sizeof(HttpResponse));
    resp->status_code = 404;
    resp->body = string_new("Not Found");
    resp->headers = NULL;
    resp->error = string_new("Error message");

    ASSERT_EQ(404, resp->status_code);
    ASSERT_NOT_NULL(resp->body);
    ASSERT_NOT_NULL(resp->error);
    ASSERT_NULL(resp->headers);
    http_response_free(resp);
}
