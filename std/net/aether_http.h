#ifndef AETHER_HTTP_H
#define AETHER_HTTP_H

#include "../string/aether_string.h"

typedef struct {
    int status_code;
    AetherString* body;
    AetherString* headers;
    AetherString* error;
} HttpResponse;

HttpResponse* http_get(const char* url);
HttpResponse* http_post(const char* url, const char* body, const char* content_type);
HttpResponse* http_put(const char* url, const char* body, const char* content_type);
HttpResponse* http_delete(const char* url);
void http_response_free(HttpResponse* response);

#endif

