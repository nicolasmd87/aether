// aether_host_lua.c — Embedded Lua Language Host Module
//
// Embeds Lua in the Aether process. When run_sandboxed is called,
// installs the Aether sandbox checker so Lua's libc calls are
// intercepted and checked against the grant list.

#include "aether_host_lua.h"
#include "../../../runtime/aether_sandbox.h"
#include "../../../runtime/aether_shared_map.h"

#ifdef AETHER_HAS_LUA
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <string.h>

static lua_State* L = NULL;

// Bridge-owned permission stack. Self-contained — see comment in
// contrib/host/tcl/aether_host_tcl.c for rationale.
static void* lua_perms_stack[64];
static int   lua_perms_depth = 0;

// Permission checker — same as Python host module
extern int list_size(void*);
extern void* list_get_raw(void*, int);

static int pattern_match(const char* pat, const char* resource) {
    // Normalize IPv4-mapped IPv6 addresses so a grant for "10.0.0.1"
    // matches a TCP resource reported as "::ffff:10.0.0.1" (and
    // vice versa). Safe for non-TCP categories because "::ffff:"
    // doesn't appear in filesystem paths, env var names, or exec
    // command strings.
    if (pat && strncmp(pat, "::ffff:", 7) == 0) pat += 7;
    if (resource && strncmp(resource, "::ffff:", 7) == 0) resource += 7;
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

static int perms_allow(void* ctx, const char* category, const char* resource) {
    if (!ctx) return 1;
    int n = list_size(ctx);
    if (n == 0) return 0;
    for (int i = 0; i < n; i += 2) {
        const char* cat = (const char*)list_get_raw(ctx, i);
        const char* pat = (const char*)list_get_raw(ctx, i + 1);
        if (!cat || !pat) continue;
        if (cat[0] == '*' && pat[0] == '*') return 1;
        if (strcmp(cat, category) == 0 && pattern_match(pat, resource)) return 1;
    }
    return 0;
}

static int host_lua_checker(const char* category, const char* resource) {
    if (lua_perms_depth <= 0) return 1;
    for (int level = 0; level < lua_perms_depth; level++) {
        if (!perms_allow(lua_perms_stack[level], category, resource)) return 0;
    }
    return 1;
}

int lua_init(void) {
    if (L) return 0;
    L = luaL_newstate();
    if (!L) return -1;
    luaL_openlibs(L);
    return 0;
}

void lua_finalize(void) {
    if (L) {
        lua_close(L);
        L = NULL;
    }
}

int lua_run(const char* code) {
    if (!code) return -1;
    lua_init();
    if (luaL_dostring(L, code) != 0) {
        fprintf(stderr, "[lua] %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return -1;
    }
    return 0;
}

int lua_run_sandboxed(void* perms, const char* code) {
    if (!code) return -1;
    lua_init();
    if (lua_perms_depth >= 64) return -1;

    // Push perms and install checker
    lua_perms_stack[lua_perms_depth++] = perms;
    aether_sandbox_check_fn prev = _aether_sandbox_checker;
    _aether_sandbox_checker = host_lua_checker;

    int result = 0;
    if (luaL_dostring(L, code) != 0) {
        fprintf(stderr, "[lua] %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        result = -1;
    }

    // Restore
    _aether_sandbox_checker = prev;
    lua_perms_depth--;

    return result;
}

// --- Shared map native bindings for Lua ---

// Current active token (set before running hosted code)
static uint64_t current_map_token = 0;

// aether_map_get(key) → string or nil
static int lua_aether_map_get(lua_State* state) {
    const char* key = luaL_checkstring(state, 1);
    const char* val = aether_shared_map_get_by_token(current_map_token, key);
    if (val) {
        lua_pushstring(state, val);
    } else {
        lua_pushnil(state);
    }
    return 1;
}

// aether_map_put(key, value)
static int lua_aether_map_put(lua_State* state) {
    const char* key = luaL_checkstring(state, 1);
    const char* val = luaL_checkstring(state, 2);
    aether_shared_map_put_by_token(current_map_token, key, val);
    return 0;
}

static void register_map_bindings(lua_State* state) {
    lua_pushcfunction(state, lua_aether_map_get);
    lua_setglobal(state, "aether_map_get");
    lua_pushcfunction(state, lua_aether_map_put);
    lua_setglobal(state, "aether_map_put");
}

int lua_run_sandboxed_with_map(void* perms, const char* code, uint64_t map_token) {
    if (!code) return -1;
    lua_init();
    register_map_bindings(L);

    // Freeze inputs — hosted code can read them but not overwrite
    // (find the map by token and freeze it)
    extern void aether_shared_map_freeze_inputs_by_token(uint64_t);
    aether_shared_map_freeze_inputs_by_token(map_token);

    // Set the active token for this invocation
    current_map_token = map_token;

    if (lua_perms_depth >= 64) return -1;

    // Push perms and install checker
    lua_perms_stack[lua_perms_depth++] = perms;
    aether_sandbox_check_fn prev = _aether_sandbox_checker;
    _aether_sandbox_checker = host_lua_checker;

    int result = 0;
    if (luaL_dostring(L, code) != 0) {
        fprintf(stderr, "[lua] %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        result = -1;
    }

    // Restore and revoke token
    _aether_sandbox_checker = prev;
    lua_perms_depth--;
    current_map_token = 0;

    return result;
}

#else
#include <stdio.h>
int lua_init(void) {
    fprintf(stderr, "error: contrib.host.lua not available (compile with AETHER_HAS_LUA)\n");
    return -1;
}
void lua_finalize(void) {}
int lua_run(const char* code) { (void)code; return lua_init(); }
int lua_run_sandboxed(void* perms, const char* code) { (void)perms; (void)code; return lua_init(); }
int lua_run_sandboxed_with_map(void* perms, const char* code,
    uint64_t token) {
  (void)perms; (void)code; (void)token;
  return lua_init();
}
#endif
