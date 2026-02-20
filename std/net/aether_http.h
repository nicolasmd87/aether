#ifndef AETHER_HTTP_H
#define AETHER_HTTP_H

#include "../string/aether_string.h"

typedef struct {
    int status_code;
    AetherString* body;
    AetherString* headers;
    AetherString* error;
} HttpResponse;

HttpResponse* http_get(AetherString* url);
HttpResponse* http_post(AetherString* url, AetherString* body, AetherString* content_type);
HttpResponse* http_put(AetherString* url, AetherString* body, AetherString* content_type);
HttpResponse* http_delete(AetherString* url);
void http_response_free(HttpResponse* response);

#endif

