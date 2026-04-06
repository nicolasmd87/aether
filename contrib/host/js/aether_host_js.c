// aether_host_js.c — Embedded JavaScript Language Host Module (Duktape)
//
// Unlike Python/Lua, Duktape has NO built-in filesystem or env access.
// We provide native bindings (env, readFile, print) that go through
// the Aether sandbox checker. This is the purest containment model —
// the guest can ONLY do what we explicitly expose.

#include "aether_host_js.h"
#include "../../../runtime/aether_sandbox.h"
#include "../../../runtime/aether_shared_map.h"

extern void* _aether_ctx_stack[];
extern int _aether_ctx_depth;

#ifdef AETHER_HAS_JS
#include <duktape.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static duk_context* ctx = NULL;

// Permission checker — shared with Python/Lua host modules
extern int list_size(void*);
extern void* list_get(void*, int);

static int pattern_match(const char* pat, const char* resource) {
    int plen = strlen(pat);
    int rlen = strlen(resource);
    if (plen == 1 && pat[0] == '*') return 1;
    if (plen > 1 && pat[plen-1] == '*') {
        if (strncmp(pat, resource, plen-1) == 0) return 1;
    }
    if (plen > 1 && pat[0] == '*') {
        int slen = plen - 1;
        if (rlen >= slen && strcmp(resource + rlen - slen, pat + 1) == 0) return 1;
    }
    return strcmp(pat, resource) == 0;
}

static int perms_allow(void* perms, const char* category, const char* resource) {
    if (!perms) return 1;
    int n = list_size(perms);
    if (n == 0) return 0;
    for (int i = 0; i < n; i += 2) {
        const char* cat = (const char*)list_get(perms, i);
        const char* pat = (const char*)list_get(perms, i + 1);
        if (!cat || !pat) continue;
        if (cat[0] == '*' && pat[0] == '*') return 1;
        if (strcmp(cat, category) == 0 && pattern_match(pat, resource)) return 1;
    }
    return 0;
}

static int check_sandbox(const char* category, const char* resource) {
    if (_aether_ctx_depth <= 0) return 1;
    for (int level = 0; level < _aether_ctx_depth; level++) {
        if (!perms_allow(_aether_ctx_stack[level], category, resource)) return 0;
    }
    return 1;
}

// --- Duktape native bindings (exposed to JS) ---

// print(...) — always available
static duk_ret_t js_print(duk_context* c) {
    int n = duk_get_top(c);
    for (int i = 0; i < n; i++) {
        if (i > 0) printf(" ");
        printf("%s", duk_to_string(c, i));
    }
    printf("\n");
    return 0;
}

// env(name) — returns string or undefined, checked by sandbox
static duk_ret_t js_env(duk_context* c) {
    const char* name = duk_require_string(c, 0);
    if (!check_sandbox("env", name)) {
        duk_push_undefined(c);
        return 1;
    }
    const char* val = getenv(name);
    if (val) {
        duk_push_string(c, val);
    } else {
        duk_push_undefined(c);
    }
    return 1;
}

// readFile(path) — returns string or undefined, checked by sandbox
static duk_ret_t js_read_file(duk_context* c) {
    const char* path = duk_require_string(c, 0);
    if (!check_sandbox("fs_read", path)) {
        duk_push_undefined(c);
        return 1;
    }
    FILE* f = fopen(path, "r");
    if (!f) {
        duk_push_undefined(c);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = malloc(len + 1);
    if (!buf) { fclose(f); duk_push_undefined(c); return 1; }
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    duk_push_string(c, buf);
    free(buf);
    return 1;
}

// fileExists(path) — returns boolean, checked by sandbox
static duk_ret_t js_file_exists(duk_context* c) {
    const char* path = duk_require_string(c, 0);
    if (!check_sandbox("fs_read", path)) {
        duk_push_boolean(c, 0);
        return 1;
    }
    FILE* f = fopen(path, "r");
    if (f) { fclose(f); duk_push_boolean(c, 1); }
    else { duk_push_boolean(c, 0); }
    return 1;
}

static void register_bindings(duk_context* c) {
    duk_push_c_function(c, js_print, DUK_VARARGS);
    duk_put_global_string(c, "print");

    duk_push_c_function(c, js_env, 1);
    duk_put_global_string(c, "env");

    duk_push_c_function(c, js_read_file, 1);
    duk_put_global_string(c, "readFile");

    duk_push_c_function(c, js_file_exists, 1);
    duk_put_global_string(c, "fileExists");
}

int js_init(void) {
    if (ctx) return 0;
    ctx = duk_create_heap_default();
    if (!ctx) return -1;
    register_bindings(ctx);
    return 0;
}

void js_finalize(void) {
    if (ctx) {
        duk_destroy_heap(ctx);
        ctx = NULL;
    }
}

int js_run(const char* code) {
    if (!code) return -1;
    js_init();
    if (duk_peval_string(ctx, code) != 0) {
        fprintf(stderr, "[js] %s\n", duk_safe_to_string(ctx, -1));
        duk_pop(ctx);
        return -1;
    }
    duk_pop(ctx);
    return 0;
}

int js_run_sandboxed(void* perms, const char* code) {
    if (!code) return -1;
    js_init();

    _aether_ctx_stack[_aether_ctx_depth++] = perms;
    aether_sandbox_check_fn prev = _aether_sandbox_checker;
    _aether_sandbox_checker = NULL;  // JS uses direct check_sandbox, not libc interception

    int result = 0;
    if (duk_peval_string(ctx, code) != 0) {
        fprintf(stderr, "[js] %s\n", duk_safe_to_string(ctx, -1));
        duk_pop(ctx);
        result = -1;
    } else {
        duk_pop(ctx);
    }

    _aether_sandbox_checker = prev;
    _aether_ctx_depth--;

    return result;
}

// --- Shared map bindings for JS ---

static uint64_t js_current_map_token = 0;

static duk_ret_t js_aether_map_get(duk_context* c) {
    const char* key = duk_require_string(c, 0);
    const char* val = aether_shared_map_get_by_token(js_current_map_token, key);
    if (val) { duk_push_string(c, val); } else { duk_push_undefined(c); }
    return 1;
}

static duk_ret_t js_aether_map_put(duk_context* c) {
    const char* key = duk_require_string(c, 0);
    const char* val = duk_require_string(c, 1);
    aether_shared_map_put_by_token(js_current_map_token, key, val);
    return 0;
}

static void register_map_bindings(duk_context* c) {
    duk_push_c_function(c, js_aether_map_get, 1);
    duk_put_global_string(c, "aether_map_get");
    duk_push_c_function(c, js_aether_map_put, 2);
    duk_put_global_string(c, "aether_map_put");
}

int js_run_sandboxed_with_map(void* perms, const char* code, uint64_t map_token) {
    if (!code) return -1;
    js_init();
    register_map_bindings(ctx);

    extern void aether_shared_map_freeze_inputs_by_token(uint64_t);
    aether_shared_map_freeze_inputs_by_token(map_token);
    js_current_map_token = map_token;

    _aether_ctx_stack[_aether_ctx_depth++] = perms;
    aether_sandbox_check_fn prev = _aether_sandbox_checker;
    _aether_sandbox_checker = NULL;

    int result = 0;
    if (duk_peval_string(ctx, code) != 0) {
        fprintf(stderr, "[js] %s\n", duk_safe_to_string(ctx, -1));
        duk_pop(ctx);
        result = -1;
    } else {
        duk_pop(ctx);
    }

    _aether_sandbox_checker = prev;
    _aether_ctx_depth--;
    js_current_map_token = 0;

    return result;
}

#else
#include <stdio.h>
int js_init(void) {
    fprintf(stderr, "error: std.host.js not available (compile with AETHER_HAS_JS)\n");
    return -1;
}
void js_finalize(void) {}
int js_run(const char* code) { (void)code; return js_init(); }
int js_run_sandboxed(void* perms, const char* code) { (void)perms; (void)code; return js_init(); }
int js_run_sandboxed_with_map(void* perms, const char* code,
    uint64_t token) {
  (void)perms; (void)code; (void)token;
  return js_init();
}
#endif
