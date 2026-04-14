#ifndef AETHER_OS_H
#define AETHER_OS_H

// Run a shell command, return exit code
int os_system(const char* cmd);

// Run a command and capture stdout as a string
// Returns heap-allocated string (caller must free), or NULL on failure
char* os_exec_raw(const char* cmd);

// Get environment variable, returns string or NULL if not set
char* os_getenv(const char* name);

#endif
