#ifndef AETHER_HOST_RUBY_H
#define AETHER_HOST_RUBY_H

#include <stdint.h>

int ruby_run_sandboxed(void* perms, const char* code);
int ruby_run_sandboxed_with_map(void* perms, const char* code, uint64_t map_token);
int ruby_run(const char* code);
int ruby_init_host(void);
void ruby_finalize_host(void);

#endif
