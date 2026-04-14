#ifndef AETHER_OS_H
#define AETHER_OS_H

// Run a shell command, return exit code
int os_system(const char* cmd);

// Run a command and capture stdout as a string
// Returns heap-allocated string (caller must free), or NULL on failure
char* os_exec_raw(const char* cmd);

// Get environment variable, returns string or NULL if not set
char* os_getenv(const char* name);

// Replace the current process image with `prog`, passing each element
// of `argv_list` as an argv entry. `argv_list` is an Aether `list<ptr>`
// whose entries must be C strings; element 0 is argv[0] for the new
// program and need not match `prog`. Does NOT return on success — on
// failure returns -1 and leaves the current process running. `prog` is
// looked up on PATH if it does not contain a slash (POSIX `execvp`).
// Not available on Windows (returns -1).
int os_execv(const char* prog, void* argv_list);

// Search PATH for an executable named `name`. Returns the absolute
// path to the first executable hit, or NULL if not found. If `name`
// already contains '/', it's returned as-is when it's executable
// (matches POSIX `command -v` semantics for absolute/relative paths).
// Caller owns the returned string.
char* os_which(const char* name);

#endif
