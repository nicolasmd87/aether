#include "aether_os.h"
#include "../../runtime/config/aether_optimization_config.h"
#include "../../runtime/aether_sandbox.h"

// NOTE on aether_argv0 placement: the implementation lives in
// runtime/aether_runtime.c next to the aether_argc / aether_argv it
// reads. aether_os.c cannot reference those variables directly because
// the compiler binary (aetherc) links std/os/aether_os.c but NOT
// runtime/aether_runtime.c, so a hard reference would break the
// compiler link. Keeping the function next to its state fixes that and
// leaves this file focused on shell/exec helpers.

#if !AETHER_HAS_FILESYSTEM
int os_system(const char* c) { (void)c; return -1; }
char* os_exec_raw(const char* c) { (void)c; return NULL; }
char* os_getenv(const char* n) { (void)n; return NULL; }
int os_execv(const char* p, void* a) { (void)p; (void)a; return -1; }
char* os_which(const char* n) { (void)n; return NULL; }
int os_run(const char* p, void* a, void* e) { (void)p; (void)a; (void)e; return -1; }
char* os_run_capture_raw(const char* p, void* a, void* e) { (void)p; (void)a; (void)e; return NULL; }
#else

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifndef _WIN32
#include <unistd.h>
#endif

// Forward declarations for the Aether collections API. aether_os.c sits
// below std/collections in the link order, so we can't include the header
// here without a dependency cycle; the prototypes match
// std/collections/aether_collections.h.
extern int list_size(void* list);
extern void* list_get(void* list, int index);

#ifndef _WIN32
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#endif

// libaether.a list operations — declared extern so this file doesn't have
// to include the collections header (which would create a build cycle).
extern int   list_size(void* list);
extern void* list_get(void* list, int index);

int os_system(const char* cmd) {
    if (!cmd) return -1;
    if (!aether_sandbox_check("exec", cmd)) return -1;
    return system(cmd);
}

char* os_exec_raw(const char* cmd) {
    if (!cmd) return NULL;
    if (!aether_sandbox_check("exec", cmd)) return NULL;

#ifdef _WIN32
    FILE* pipe = _popen(cmd, "r");
#else
    FILE* pipe = popen(cmd, "r");
#endif
    if (!pipe) return NULL;

    size_t capacity = 1024;
    size_t len = 0;
    char* result = (char*)malloc(capacity);
    if (!result) {
#ifdef _WIN32
        _pclose(pipe);
#else
        pclose(pipe);
#endif
        return NULL;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        size_t chunk = strlen(buffer);
        if (len + chunk + 1 > capacity) {
            capacity *= 2;
            char* new_result = (char*)realloc(result, capacity);
            if (!new_result) {
                free(result);
#ifdef _WIN32
                _pclose(pipe);
#else
                pclose(pipe);
#endif
                return NULL;
            }
            result = new_result;
        }
        memcpy(result + len, buffer, chunk);
        len += chunk;
    }

    result[len] = '\0';

#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return result;
}

char* os_getenv(const char* name) {
    if (!name) return NULL;
    if (!aether_sandbox_check("env", name)) return NULL;
    char* val = getenv(name);
    if (!val) return NULL;
    return strdup(val);
}

int os_execv(const char* prog, void* argv_list) {
    if (!prog) return -1;
    if (!aether_sandbox_check("exec", prog)) return -1;

#ifdef _WIN32
    // No fork/exec equivalent on Windows that preserves PID and stdio
    // the way POSIX execvp does. Callers needing a Windows process
    // replacement should use os_run (PR #148) and then exit(rc).
    (void)argv_list;
    return -1;
#else
    // Build a NULL-terminated char* array from the Aether list. We copy
    // pointers only — the list owns the string storage and keeps it
    // alive for the duration of the call (which, on success, is the
    // rest of forever — the process image is replaced in place).
    int n = argv_list ? list_size(argv_list) : 0;

    // Guard against pathological sizes before multiplying for malloc.
    if (n < 0) return -1;
    if ((size_t)n > (SIZE_MAX / sizeof(char*)) - 2) return -1;

    char** argv = (char**)malloc(sizeof(char*) * (size_t)(n + 2));
    if (!argv) return -1;

    // Canonical POSIX behaviour: argv[0] is the program name. If the
    // caller passed an empty list we synthesise argv[0] from `prog` so
    // the callee sees a sensible value; otherwise the caller owns the
    // whole argv and we trust their layout.
    int ai = 0;
    if (n == 0) {
        argv[ai++] = (char*)prog;
    } else {
        for (int i = 0; i < n; i++) {
            void* item = list_get(argv_list, i);
            if (!item) {
                // Bail early rather than pass NULL into execvp's
                // variadic-argv contract, which has undefined behaviour
                // on many implementations.
                free(argv);
                return -1;
            }
            argv[ai++] = (char*)item;
        }
    }
    argv[ai] = NULL;

    // Flush stdio before replacing the process image. execvp destroys
    // the caller's stdio buffers, so anything println'd but not yet
    // flushed would be silently lost. Line-buffered output already on
    // a terminal is safe, but redirected / pipe output is typically
    // fully-buffered — without this flush, pre-exec diagnostics vanish.
    fflush(stdout);
    fflush(stderr);

    // execvp honours PATH if `prog` does not contain a slash. On
    // success this call never returns; on failure we free scratch and
    // report -1. We intentionally do not touch errno — callers that
    // want diagnostic detail should read it themselves after the call
    // via a dedicated wrapper (not exposed yet).
    execvp(prog, argv);
    free(argv);
    return -1;
#endif
}

// Search PATH for an executable. POSIX semantics:
//   1. If `name` contains a '/', it's treated as a path (absolute or
//      relative to cwd). Return it as-is if executable, else NULL.
//   2. Otherwise iterate through colon-separated entries in $PATH (or a
//      sensible default if PATH isn't set), looking for `<dir>/<name>`
//      that's executable. Return the first hit.
//
// Caller owns the returned string.
char* os_which(const char* name) {
    if (!name || !*name) return NULL;
    if (!aether_sandbox_check("env", "PATH")) return NULL;

#ifdef _WIN32
    // Windows uses ';' as PATH separator and PATHEXT to choose extensions.
    // Stub for now; a follow-up can implement the full Windows lookup.
    (void)name;
    return NULL;
#else
    if (strchr(name, '/')) {
        if (access(name, X_OK) == 0) return strdup(name);
        return NULL;
    }

    const char* path = getenv("PATH");
    if (!path || !*path) path = "/usr/local/bin:/usr/bin:/bin";

    size_t name_len = strlen(name);
    char buf[4096];
    const char* p = path;
    while (*p) {
        const char* end = strchr(p, ':');
        size_t dirlen = end ? (size_t)(end - p) : strlen(p);
        // Empty entry means current directory (POSIX).
        if (dirlen == 0) {
            // Guard: we write "./" (2 bytes) plus name_len+1 bytes
            // (including null terminator) starting at buf+2. The last
            // written byte is at index (2 + name_len), which must be
            // strictly less than sizeof(buf) for validity.
            if (2 + name_len < sizeof(buf)) {
                buf[0] = '.';
                buf[1] = '/';
                memcpy(buf + 2, name, name_len + 1);
                if (access(buf, X_OK) == 0) return strdup(buf);
            }
        } else if (dirlen + 1 + name_len < sizeof(buf)) {
            memcpy(buf, p, dirlen);
            buf[dirlen] = '/';
            memcpy(buf + dirlen + 1, name, name_len + 1);
            if (access(buf, X_OK) == 0) return strdup(buf);
        }
        if (!end) break;
        p = end + 1;
    }
    return NULL;
#endif
}


// --- os_run / os_run_capture: argv-based child process launch ---
//
// Both functions take an Aether list as the argv (and optional env)
// rather than a shell-string command line. There is no /bin/sh in the
// loop, so paths-with-spaces, $variables, |, ;, *, and other shell
// metacharacters in argv items are passed verbatim. This eliminates a
// large class of quoting bugs and makes the same Aether code portable
// to platforms without a POSIX shell.
//
// Implementation: POSIX uses fork + execvp + waitpid, with execve when
// an explicit env is provided. The Windows backend is a TODO — for now
// it just returns -1 / NULL on _WIN32 builds.

#ifndef _WIN32

// Build a NULL-terminated argv array from an Aether list. The first
// entry in argv[] is `prog`. Caller must free the returned array (the
// strings inside are pointers into the Aether list and must NOT be
// freed individually). Returns NULL on allocation failure.
static char** build_argv_array(const char* prog, void* argv_list) {
    int n = argv_list ? list_size(argv_list) : 0;
    char** av = (char**)malloc(sizeof(char*) * (size_t)(n + 2));
    if (!av) return NULL;
    av[0] = (char*)prog;
    for (int i = 0; i < n; i++) {
        av[i + 1] = (char*)list_get(argv_list, i);
    }
    av[n + 1] = NULL;
    return av;
}

// Build a NULL-terminated environ array from an Aether list of
// "KEY=VALUE" strings. Returns NULL if env_list is NULL (caller should
// inherit parent env in that case). Caller must free the returned
// array.
static char** build_envp_array(void* env_list) {
    if (!env_list) return NULL;
    int n = list_size(env_list);
    char** envp = (char**)malloc(sizeof(char*) * (size_t)(n + 1));
    if (!envp) return NULL;
    for (int i = 0; i < n; i++) {
        envp[i] = (char*)list_get(env_list, i);
    }
    envp[n] = NULL;
    return envp;
}

int os_run(const char* prog, void* argv_list, void* env_list) {
    if (!prog) return -1;
    if (!aether_sandbox_check("exec", prog)) return -1;

    char** av = build_argv_array(prog, argv_list);
    if (!av) return -1;
    char** envp = build_envp_array(env_list);

    pid_t pid = fork();
    if (pid < 0) {
        free(av);
        free(envp);
        return -1;
    }
    if (pid == 0) {
        // Child
        if (envp) {
            execve(prog, av, envp);
        } else {
            execvp(prog, av);
        }
        // exec only returns on failure
        _exit(127);
    }
    // Parent
    free(av);
    free(envp);
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) return -1;
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return -1;
}

char* os_run_capture_raw(const char* prog, void* argv_list, void* env_list) {
    if (!prog) return NULL;
    if (!aether_sandbox_check("exec", prog)) return NULL;

    int pipefd[2];
    if (pipe(pipefd) != 0) return NULL;

    char** av = build_argv_array(prog, argv_list);
    if (!av) {
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }
    char** envp = build_envp_array(env_list);

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        free(av);
        free(envp);
        return NULL;
    }
    if (pid == 0) {
        // Child: redirect stdout to pipe write end, close read end
        close(pipefd[0]);
        if (dup2(pipefd[1], 1) < 0) _exit(127);
        close(pipefd[1]);
        if (envp) {
            execve(prog, av, envp);
        } else {
            execvp(prog, av);
        }
        _exit(127);
    }
    // Parent: close write end, read until EOF
    close(pipefd[1]);
    free(av);
    free(envp);

    size_t cap = 1024;
    size_t len = 0;
    char* result = (char*)malloc(cap);
    if (!result) {
        close(pipefd[0]);
        // Reap the child so we don't leave a zombie
        int st = 0;
        waitpid(pid, &st, 0);
        return NULL;
    }
    char buf[1024];
    for (;;) {
        ssize_t n = read(pipefd[0], buf, sizeof(buf));
        if (n < 0) {
            if (errno == EINTR) continue;
            free(result);
            close(pipefd[0]);
            int st = 0;
            waitpid(pid, &st, 0);
            return NULL;
        }
        if (n == 0) break;
        if (len + (size_t)n + 1 > cap) {
            while (len + (size_t)n + 1 > cap) cap *= 2;
            char* bigger = (char*)realloc(result, cap);
            if (!bigger) {
                free(result);
                close(pipefd[0]);
                int st = 0;
                waitpid(pid, &st, 0);
                return NULL;
            }
            result = bigger;
        }
        memcpy(result + len, buf, (size_t)n);
        len += (size_t)n;
    }
    result[len] = '\0';
    close(pipefd[0]);
    int st = 0;
    waitpid(pid, &st, 0);
    return result;
}

#else // _WIN32

int os_run(const char* prog, void* argv_list, void* env_list) {
    // TODO: CreateProcessW with explicit command-line and environment
    // block. Until then, Windows builds get -1 and a clear failure mode.
    (void)prog; (void)argv_list; (void)env_list;
    return -1;
}

char* os_run_capture_raw(const char* prog, void* argv_list, void* env_list) {
    (void)prog; (void)argv_list; (void)env_list;
    return NULL;
}

#endif // !_WIN32


#endif // AETHER_HAS_FILESYSTEM
