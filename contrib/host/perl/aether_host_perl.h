#ifndef AETHER_HOST_PERL_H
#define AETHER_HOST_PERL_H

#include <stdint.h>

int aether_perl_run_sandboxed(void* perms, const char* code);
int aether_perl_run_sandboxed_with_map(void* perms, const char* code, uint64_t map_token);
int aether_perl_run(const char* code);
int aether_perl_init(void);
void aether_perl_finalize(void);

#endif
