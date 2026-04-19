#ifndef AETHER_HOST_TCL_H
#define AETHER_HOST_TCL_H

#include <stdint.h>

int tcl_run_sandboxed(void* perms, const char* code);
int tcl_run_sandboxed_with_map(void* perms, const char* code, uint64_t map_token);
int tcl_run(const char* code);
int tcl_init(void);
void tcl_finalize(void);

#endif
