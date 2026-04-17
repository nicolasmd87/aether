#ifndef AETHER_HOST_AETHER_H
#define AETHER_HOST_AETHER_H

#include <stdint.h>

// Compile an Aether script to a native binary.
// Returns 0 on success, -1 on error.
int aether_host_compile(const char* script_path, const char* out_path);

// Run a compiled binary in a sandboxed subprocess (LD_PRELOAD).
// perms: sandbox grant list (Aether list of category/pattern pairs).
// Returns child exit code.
int aether_host_run_sandboxed(void* perms, const char* binary_path);

// Compile + run in one step.
int aether_host_run_script_sandboxed(void* perms, const char* script_path);

// Run with shared map for input/output data exchange.
int aether_host_run_sandboxed_with_map(void* perms, const char* binary_path, uint64_t map_token);
int aether_host_run_script_sandboxed_with_map(
    void* perms, const char* script_path, uint64_t map_token);

// Run without sandbox.
int aether_host_run(const char* binary_path);
int aether_host_run_script(const char* script_path);

// Run sandboxed and capture stdout.
char* aether_host_capture_sandboxed(void* perms, const char* binary_path);
char* aether_host_capture_script_sandboxed(void* perms, const char* script_path);

#endif
