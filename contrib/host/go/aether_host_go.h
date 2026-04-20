#ifndef AETHER_HOST_GO_H
#define AETHER_HOST_GO_H

#include <stdint.h>

int go_run_sandboxed(void* perms, const char* binary_path);
int go_run_script_sandboxed(void* perms, const char* script_path);
int go_run(const char* binary_path);
int go_run_script(const char* script_path);
int go_run_sandboxed_with_map(void* perms, const char* binary_path, uint64_t map_token);
int go_run_script_sandboxed_with_map(void* perms, const char* script_path, uint64_t map_token);

#endif
