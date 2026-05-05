/* Verifier shims for the http_auth integration test.
 *
 * The Bearer + session-cookie middleware accept a `int (*)(const char*,
 * void*)` verifier pointer. Real deployments wire that to a JWT
 * signature check, a DB lookup, or an OAuth introspection call. For
 * the test we just hard-code one accepted credential per shim so the
 * shell runner can assert deterministic accept/reject behaviour.
 *
 * Lives in a separate shim so the verifier signature remains the
 * exact `const char* token` the C middleware expects — comparing a
 * `const char*` to a literal in Aether would need C-string accessors
 * that aren't part of the public stdlib surface. */

#include <string.h>

int aether_test_bearer_verify(const char* token, void* ud) {
    (void)ud;
    if (!token) return 0;
    return strcmp(token, "good-token") == 0 ? 1 : 0;
}

int aether_test_session_verify(const char* cookie_value, void* ud) {
    (void)ud;
    if (!cookie_value) return 0;
    return strcmp(cookie_value, "valid-sid") == 0 ? 1 : 0;
}
