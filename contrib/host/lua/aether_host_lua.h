#ifndef AETHER_HOST_LUA_H
#define AETHER_HOST_LUA_H

#include <stdint.h>

int lua_run_sandboxed(void* perms, const char* code);
int lua_run_sandboxed_with_map(void* perms, const char* code, uint64_t map_token);
int lua_run(const char* code);
int lua_init(void);
void lua_finalize(void);

#endif
