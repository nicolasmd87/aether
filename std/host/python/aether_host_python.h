#ifndef AETHER_HOST_PYTHON_H
#define AETHER_HOST_PYTHON_H

#include <stdint.h>

int python_run_sandboxed(void* perms, const char* code);
int python_run_sandboxed_with_map(void* perms, const char* code, uint64_t map_token);
int python_run(const char* code);
int python_init(void);
void python_finalize(void);

#endif
