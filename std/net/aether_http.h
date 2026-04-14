#ifndef AETHER_HTTP_H
#define AETHER_HTTP_H

#include "../string/aether_string.h"

typedef struct {
    int status_code;
    AetherString* body;
    AetherString* headers;
    AetherString* error;
} HttpResponse;

HttpResponse* http_get_raw(const char* url);
HttpResponse* http_post_raw(const char* url, const char* body, const char* content_type);
HttpResponse* http_put_raw(const char* url, const char* body, const char* content_type);
HttpResponse* http_delete_raw(const char* url);
void http_response_free(HttpResponse* response);

// Response field accessors. All are NULL-safe: passing NULL or a freed
// response returns a sensible default (0 or "") rather than crashing.
// Returned const char* pointers are owned by the response struct and
// valid until http_response_free() is called.
int http_response_status(HttpResponse* response);
const char* http_response_body(HttpResponse* response);
const char* http_response_headers(HttpResponse* response);
const char* http_response_error(HttpResponse* response);

// Convenience: returns 1 if the request succeeded (no transport error
// AND HTTP status is in the 2xx range), 0 otherwise. Use this for the
// common "did it work?" check instead of chaining error/status calls.
int http_response_ok(HttpResponse* response);

#endif

