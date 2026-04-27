// std.http.middleware — composable middleware for the HTTP server.
//
// Each middleware is a C function with the existing
// HttpMiddleware signature `int(req, res, user_data) -> int`
// (return 1 to continue the chain, 0 to short-circuit and send the
// already-populated response). Aether-side factory functions
// allocate the per-middleware config struct (the "user_data") and
// register the C function pointer via the existing
// http_server_use_middleware chain. No new dispatch path; the hot
// loop stays a function-pointer chain.
//
// This file ships the C primitives + factories. The Aether-side
// surface lives in std/http/middleware/module.ae.
//
// Issue #260 Tier 1.
#ifndef AETHER_MIDDLEWARE_H
#define AETHER_MIDDLEWARE_H

#include "../../net/aether_http_server.h"

// -----------------------------------------------------------------
// CORS (Cross-Origin Resource Sharing)
// -----------------------------------------------------------------
typedef struct AetherCorsOpts AetherCorsOpts;

// Allocate a CORS config. NULL strings disable that header. The
// Aether-side wrapper passes either a literal or "" to disable.
//
//   allow_origin       — value of Access-Control-Allow-Origin
//                        ("*" / "https://example.com" / "" = unset)
//   allow_methods      — value of Access-Control-Allow-Methods
//                        ("GET, POST, OPTIONS" / "")
//   allow_headers      — value of Access-Control-Allow-Headers
//   allow_credentials  — non-zero -> "Access-Control-Allow-Credentials: true"
//   max_age_seconds    — non-zero -> Access-Control-Max-Age header
//                        (browsers cache the preflight for this long)
AetherCorsOpts* aether_cors_opts_new(const char* allow_origin,
                                     const char* allow_methods,
                                     const char* allow_headers,
                                     int allow_credentials,
                                     int max_age_seconds);
void aether_cors_opts_free(AetherCorsOpts* opts);

// Middleware function — register via http_server_use_middleware
// with the AetherCorsOpts* as user_data.
int aether_middleware_cors(HttpRequest* req, HttpServerResponse* res, void* user_data);

// -----------------------------------------------------------------
// Basic Authentication (RFC 7617)
// -----------------------------------------------------------------
typedef struct AetherBasicAuthOpts AetherBasicAuthOpts;

// Verifier signature: receives the decoded username + password and
// returns 1 if the credentials are valid, 0 otherwise. The realm
// string is shown in the browser's auth prompt.
typedef int (*AetherBasicAuthVerifier)(const char* username,
                                       const char* password,
                                       void* verifier_user_data);

AetherBasicAuthOpts* aether_basic_auth_opts_new(const char* realm,
                                                AetherBasicAuthVerifier verify,
                                                void* verifier_user_data);
void aether_basic_auth_opts_free(AetherBasicAuthOpts* opts);

int aether_middleware_basic_auth(HttpRequest* req, HttpServerResponse* res, void* user_data);

// -----------------------------------------------------------------
// Token-bucket rate limiter (per-client-IP)
// -----------------------------------------------------------------
typedef struct AetherRateLimitOpts AetherRateLimitOpts;

// max_requests per window_ms milliseconds, per remote-IP bucket. The
// bucket is refilled continuously; clients that exceed the rate get a
// 429 Too Many Requests response with a Retry-After header.
//
// Implementation: hash table keyed by the X-Forwarded-For header (or
// the request path's prefix as a fallback when no IP is available
// in the request struct). Single mutex protects the table.
AetherRateLimitOpts* aether_rate_limit_opts_new(int max_requests, int window_ms);
void aether_rate_limit_opts_free(AetherRateLimitOpts* opts);

int aether_middleware_rate_limit(HttpRequest* req, HttpServerResponse* res, void* user_data);

// -----------------------------------------------------------------
// Virtual host routing (host -> handler_id dispatch)
// -----------------------------------------------------------------
typedef struct AetherVhostMap AetherVhostMap;

// vhost is a request-side mux: looks at the Host: header on each
// incoming request, and either lets it through (the host is one of
// the registered ones) or short-circuits with a 404. Useful for
// running multiple "sites" off one server when they're already
// distinguished by hostname.
//
// Each call to aether_vhost_register_host adds one allowed host. To
// reject all unknown hosts with a configurable status, set the
// fallback_status (0 means use 404 with body "Unknown host").
AetherVhostMap* aether_vhost_map_new(int fallback_status);
void aether_vhost_map_free(AetherVhostMap* map);
int  aether_vhost_register_host(AetherVhostMap* map, const char* host);

int aether_middleware_vhost(HttpRequest* req, HttpServerResponse* res, void* user_data);

#endif
