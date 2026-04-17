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

// Run a child process directly via fork+execvp+waitpid (POSIX) or
// CreateProcessW (Windows — TODO). NO SHELL is interpreted: argv items
// are passed verbatim, so paths-with-spaces, $vars, |, ;, *, etc. in
// arguments are not metachars. Returns the child's exit code, or -1
// on failure to spawn (program not found, etc.).
//
//   prog    — program to execute. Resolved via PATH if it has no '/'.
//   argv    — Aether list (ArrayList*) of strings; argv[0] should be the
//             program name itself. May be NULL (treated as empty list).
//   env     — Aether list (ArrayList*) of "KEY=VALUE" strings, or NULL
//             to inherit the parent's environment.
int os_run(const char* prog, void* argv, void* env);

// Same as os_run but captures the child's stdout into a heap-allocated
// string the caller must free. Returns NULL on spawn failure. The
// child's exit code is discarded — if you need it, use os_run instead
// (or a future os_run_capture_with_status() if demand arises).
char* os_run_capture_raw(const char* prog, void* argv, void* env);

#endif
