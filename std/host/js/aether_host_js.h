#ifndef AETHER_HOST_JS_H
#define AETHER_HOST_JS_H

#include <stdint.h>

int js_run_sandboxed(void* perms, const char* code);
int js_run_sandboxed_with_map(void* perms, const char* code, uint64_t map_token);
int js_run(const char* code);
int js_init(void);
void js_finalize(void);

#endif
