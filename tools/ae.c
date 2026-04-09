// ae - Unified Aether CLI tool
// The single entry point for the Aether programming language.
//
// Usage:
//   ae init <name>          Create a new Aether project
//   ae run [file.ae]        Compile and run a program
//   ae build [file.ae]      Compile to executable
//   ae test [file|dir]      Run tests
//   ae add <package>        Add a dependency
//   ae repl                 Start interactive REPL
//   ae fmt [file]           Format source code
//   ae version              Show version
//   ae help                 Show help

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <sys/stat.h>


#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <process.h>   // getpid() / _getpid() on MinGW and MSVC
#define PATH_SEP "\\"
#define EXE_EXT ".exe"
#define mkdir_p(path) _mkdir(path)
// MSVC uses _popen/_pclose; MinGW maps popen/pclose but be explicit
#ifndef popen
#  define popen  _popen
#  define pclose _pclose
#endif
// MinGW exposes getpid() in <process.h>; MSVC only has _getpid()
#ifndef getpid
#  define getpid _getpid
#endif
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <spawn.h>
#include <libgen.h>
#include <dirent.h>
#define PATH_SEP "/"
#define EXE_EXT ""
#define mkdir_p(path) mkdir(path, 0755)
extern char** environ;
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#include "apkg/toml_parser.h"

// Version is set by Makefile from VERSION file
#ifndef AETHER_VERSION
#define AETHER_VERSION "0.0.0-dev"
#endif
#define AE_VERSION AETHER_VERSION

// --------------------------------------------------------------------------
// Cross-platform temp directory
// --------------------------------------------------------------------------
static const char* get_temp_dir(void) {
#ifdef _WIN32
    const char* t = getenv("TEMP");
    if (!t) t = getenv("TMP");
    if (!t) t = ".";
    return t;
#else
    const char* t = getenv("TMPDIR");
    if (t && t[0]) return t;
    return "/tmp";
#endif
}

// --------------------------------------------------------------------------
// Toolchain state
// --------------------------------------------------------------------------

typedef struct {
    char root[1024];           // Aether root directory
    char compiler[2048];       // Path to aetherc (root + /bin/aetherc = up to 1036 bytes)
    char lib[1024];            // Path to libaether.a (if exists)
    char include_flags[4096];  // -I flags for GCC
    char runtime_srcs[8192];   // Runtime .c files (source fallback)
    bool has_lib;              // Whether precompiled lib exists
    bool dev_mode;             // Running from source tree
    bool verbose;              // Verbose output
} Toolchain;

static Toolchain tc = {0};

// --------------------------------------------------------------------------
// Cache infrastructure
// --------------------------------------------------------------------------

static void mkdirs(const char* path);  // forward declaration

static char s_cache_dir[512] = "";

// Portable home-directory lookup.
// On Windows: USERPROFILE (native shell) → HOME (MSYS2) → fallback.
// On POSIX:   HOME → /tmp fallback.
static const char* get_home_dir(void) {
#ifdef _WIN32
    const char* h = getenv("USERPROFILE");
    if (!h || !h[0]) h = getenv("HOME");
    return h ? h : "C:\\Users\\Public";
#else
    const char* h = getenv("HOME");
    return h ? h : "/tmp";
#endif
}

static void init_cache_dir(void) {
    if (s_cache_dir[0]) return;
    const char* home = get_home_dir();
    snprintf(s_cache_dir, sizeof(s_cache_dir), "%s/.aether/cache", home);
    mkdirs(s_cache_dir);
}

// FNV-64 hash of a string
static unsigned long long fnv64_str(const char* s) {
    unsigned long long h = 14695981039346656037ULL;
    while (*s) { h ^= (unsigned char)*s++; h *= 1099511628211ULL; }
    return h;
}

// FNV-64 hash of a file's contents
static unsigned long long fnv64_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    unsigned long long h = 14695981039346656037ULL;
    unsigned char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        for (size_t i = 0; i < n; i++) { h ^= buf[i]; h *= 1099511628211ULL; }
    }
    fclose(f);
    return h;
}

// Compute a cache key from: source content + compiler mtime + lib mtime + flags
// Returns 0 if source cannot be read (caching disabled)
static unsigned long long compute_cache_key(const char* ae_file, const char* flags) {
    unsigned long long src_hash = fnv64_file(ae_file);
    if (src_hash == 0) return 0;

    char key_buf[1024];
    int pos = 0;
    pos += snprintf(key_buf + pos, sizeof(key_buf) - pos, "%016llx", src_hash);

    struct stat st;
    if (stat(tc.compiler, &st) == 0)
        pos += snprintf(key_buf + pos, sizeof(key_buf) - pos, ":%lld", (long long)st.st_mtime);
    if (tc.has_lib && stat(tc.lib, &st) == 0)
        pos += snprintf(key_buf + pos, sizeof(key_buf) - pos, ":%lld", (long long)st.st_mtime);
    snprintf(key_buf + pos, sizeof(key_buf) - pos, ":%s:O0", flags ? flags : "");

    unsigned long long h = fnv64_str(key_buf);
    return h ? h : 1ULL;
}

// --------------------------------------------------------------------------
// Utility functions
// --------------------------------------------------------------------------

#ifndef _WIN32
// Run a command via posix_spawnp (faster than system() — no /bin/sh overhead)
// Space-splits the command string into argv (no shell quoting supported,
// but our controlled commands never need it).
// quiet=0: show all output, quiet=1: hide stdout+stderr, quiet=2: hide stdout only (keep stderr for warnings)
static int posix_run(const char* cmd_str, int quiet) {
    if (tc.verbose) fprintf(stderr, "[cmd] %s\n", cmd_str);
    char buf[16384];
    strncpy(buf, cmd_str, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* toks[512];
    int n = 0;
    for (char* p = buf; *p && n < 511; ) {
        while (*p == ' ') p++;
        if (!*p) break;
        if (*p == '"') {
            p++;  // skip opening quote
            toks[n++] = p;
            while (*p && *p != '"') p++;
            if (*p) *p++ = '\0';  // null-terminate and skip closing quote
        } else {
            toks[n++] = p;
            while (*p && *p != ' ') p++;
            if (*p) *p++ = '\0';
        }
    }
    toks[n] = NULL;
    if (n == 0) return 0;

    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    if (quiet == 1) {
        posix_spawn_file_actions_addopen(&fa, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
        posix_spawn_file_actions_addopen(&fa, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    } else if (quiet == 2) {
        // Hide stdout but keep stderr (so gcc warnings are visible)
        posix_spawn_file_actions_addopen(&fa, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
    }

    pid_t pid;
    int ret = posix_spawnp(&pid, toks[0], &fa, NULL, toks, environ);
    posix_spawn_file_actions_destroy(&fa);
    if (ret != 0) return -1;

    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        return -WTERMSIG(status);  // negative signal number
    return -1;
}
#endif

// Windows: use _spawnvp to avoid cmd.exe quoting issues with system()
#ifdef _WIN32
#include <process.h>
#include <io.h>
#ifndef _O_WRONLY
#define _O_WRONLY 1
#endif
static int win_run(const char* cmd_str, int quiet) {
    if (tc.verbose) fprintf(stderr, "[cmd] %s\n", cmd_str);
    char buf[16384];
    strncpy(buf, cmd_str, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* toks[512];
    int n = 0;
    for (char* p = buf; *p && n < 511; ) {
        while (*p == ' ') p++;
        if (!*p) break;
        if (*p == '"') {
            p++;
            toks[n++] = p;
            while (*p && *p != '"') p++;
            if (*p) *p++ = '\0';
        } else {
            toks[n++] = p;
            while (*p && *p != ' ') p++;
            if (*p) *p++ = '\0';
        }
    }
    toks[n] = NULL;
    if (n == 0) return 0;

    // Redirect stdout/stderr for quiet modes
    int saved_stdout = -1, saved_stderr = -1;
    if (quiet == 1 || quiet == 2) {
        fflush(stdout);
        saved_stdout = _dup(1);
        int nul = _open("nul", _O_WRONLY);
        if (nul >= 0) { _dup2(nul, 1); _close(nul); }
    }
    if (quiet == 1) {
        fflush(stderr);
        saved_stderr = _dup(2);
        int nul = _open("nul", _O_WRONLY);
        if (nul >= 0) { _dup2(nul, 2); _close(nul); }
    }

    int ret = (int)_spawnvp(_P_WAIT, toks[0], (const char* const*)toks);

    // Restore
    if (saved_stdout >= 0) { _dup2(saved_stdout, 1); _close(saved_stdout); }
    if (saved_stderr >= 0) { _dup2(saved_stderr, 2); _close(saved_stderr); }

    return ret;
}
#endif

static int run_cmd(const char* cmd) {
#ifndef _WIN32
    return posix_run(cmd, 0);
#else
    return win_run(cmd, 0);
#endif
}

// Run a command, suppressing all output (quiet mode)
static int run_cmd_quiet(const char* cmd) {
#ifndef _WIN32
    return posix_run(cmd, 1);
#else
    return win_run(cmd, 1);
#endif
}

// Run a command, showing stderr (warnings) but hiding stdout
static int run_cmd_show_warnings(const char* cmd) {
#ifndef _WIN32
    return posix_run(cmd, 2);
#else
    return win_run(cmd, 2);
#endif
}

// Validate that a path is safe for use in shell commands (no metacharacters)
static bool is_safe_path(const char* path) {
    if (!path) return false;
    for (const char* p = path; *p; p++) {
        // Reject shell metacharacters that could enable command injection
        if (*p == '`' || *p == '$' || *p == '|' || *p == ';' ||
            *p == '&' || *p == '\n' || *p == '\r' || *p == '\'' ||
            *p == '!' || *p == '(' || *p == ')') {
            return false;
        }
    }
    return true;
}

static bool path_exists(const char* path) {
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path);
    return (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY));
#else
    return access(path, F_OK) == 0;
#endif
}

// Validate a string contains only safe characters for shell commands.
// Allows: alphanumeric, '.', '/', '-', '_', '@'
static bool is_safe_shell_arg(const char* s) {
    if (!s || !*s) return false;
    for (const char* p = s; *p; p++) {
        char c = *p;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '.' || c == '/' ||
            c == '-' || c == '_' || c == '@') continue;
        return false;
    }
    return true;
}

static bool dir_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void mkdirs(const char* path) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char sep = *p;
            *p = '\0';
            mkdir_p(tmp);
            *p = sep;
        }
    }
    mkdir_p(tmp);
}

static char* get_basename(const char* path) {
    const char* fslash = strrchr(path, '/');
    const char* bslash = strrchr(path, '\\');
    const char* base = (!fslash) ? bslash : (!bslash) ? fslash : (fslash > bslash ? fslash : bslash);
    if (!base) base = path; else base++;
    static char result[256];
    strncpy(result, base, sizeof(result) - 1);
    result[sizeof(result) - 1] = '\0';
    char* dot = strrchr(result, '.');
    if (dot) *dot = '\0';
    return result;
}

// Get directory containing this executable
static bool get_exe_dir(char* buf, size_t size) {
#ifdef __APPLE__
    uint32_t sz = (uint32_t)size;
    if (_NSGetExecutablePath(buf, &sz) == 0) {
        char resolved[PATH_MAX];
        if (realpath(buf, resolved)) {
            char* slash = strrchr(resolved, '/');
            if (slash) { *slash = '\0'; strncpy(buf, resolved, size - 1); buf[size - 1] = '\0'; return true; }
        }
    }
#elif defined(__linux__)
    ssize_t len = readlink("/proc/self/exe", buf, size - 1);
    if (len > 0) {
        buf[len] = '\0';
        char* slash = strrchr(buf, '/');
        if (slash) { *slash = '\0'; return true; }
    }
#elif defined(_WIN32)
    DWORD len = GetModuleFileNameA(NULL, buf, (DWORD)size);
    if (len > 0 && len < (DWORD)size) {
        buf[len] = '\0';
        char* slash = strrchr(buf, '\\');
        if (slash) { *slash = '\0'; return true; }
    }
#endif
    return false;
}

// --------------------------------------------------------------------------
// Toolchain discovery
// --------------------------------------------------------------------------

// GCC's -Wformat-truncation flags the runtime_srcs snprintf because it
// multiplies the theoretical max of each %s arg (1023 bytes) by 34 copies,
// exceeding the buffer.  In practice src is ~30-50 bytes and snprintf
// truncates safely, so suppress the false positive.
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wformat-truncation"
#endif
static void discover_toolchain(void) {
    char exe_dir[1024] = {0};
    bool found_exe_dir = get_exe_dir(exe_dir, sizeof(exe_dir));

    // Strategy 1: Dev mode — ae sitting next to aetherc in build/
    // Checked first so that ./build/ae always uses ./build/aetherc,
    // even when $AETHER_HOME points to an older installed version.
    // GUARD: The installed layout also has aetherc next to ae (in bin/),
    // so we verify that the parent directory contains runtime/ (repo root)
    // rather than lib/ or share/ (installed prefix).
    if (found_exe_dir) {
        char candidate[1024];
        snprintf(candidate, sizeof(candidate), "%s/aetherc" EXE_EXT, exe_dir);
        if (path_exists(candidate)) {
            char parent_runtime[1024];
            snprintf(parent_runtime, sizeof(parent_runtime), "%s/../runtime", exe_dir);
            if (dir_exists(parent_runtime)) {
                snprintf(tc.root, sizeof(tc.root), "%s/..", exe_dir);
                strncpy(tc.compiler, candidate, sizeof(tc.compiler) - 1);
                tc.dev_mode = true;
                goto found_root;
            }
        }
    }

    // Strategy 2: $AETHER_HOME
    const char* home = getenv("AETHER_HOME");
    static char home_clean[1024];
    if (home) {
        strncpy(home_clean, home, sizeof(home_clean) - 1);
        home_clean[sizeof(home_clean) - 1] = '\0';
        size_t len = strlen(home_clean);
        while (len > 0 && (home_clean[len-1] == '\r' || home_clean[len-1] == '\n' || home_clean[len-1] == ' '))
            home_clean[--len] = '\0';
        home = home_clean;
    }
    if (home && home[0] && dir_exists(home)) {
        // Prefer ~/.aether/current/ if a version symlink exists (ae version use)
        char current_compiler[1024];
        snprintf(current_compiler, sizeof(current_compiler), "%s/current/bin/aetherc" EXE_EXT, home);
        if (path_exists(current_compiler)) {
            // Verify the installation has lib or share/aether — if neither,
            // the version was installed with a buggy ae that only extracted bin/.
            char share_probe[1024], lib_probe[1024];
            snprintf(share_probe, sizeof(share_probe), "%s/current/share/aether", home);
            snprintf(lib_probe, sizeof(lib_probe), "%s/current/lib/libaether.a", home);
            if (dir_exists(share_probe) || path_exists(lib_probe)) {
                snprintf(tc.root, sizeof(tc.root), "%s/current", home);
                strncpy(tc.compiler, current_compiler, sizeof(tc.compiler) - 1);
                if (tc.verbose) fprintf(stderr, "[toolchain] compiler=%s (via current symlink)\n", tc.compiler);
                goto found_root;
            }
            // Check if the direct ~/.aether/ layout will work before warning —
            // install.sh puts files directly in AETHER_HOME, not under current/.
            char direct_share[1024], direct_lib[1024];
            snprintf(direct_share, sizeof(direct_share), "%s/share/aether", home);
            snprintf(direct_lib, sizeof(direct_lib), "%s/lib/libaether.a", home);
            if (!dir_exists(direct_share) && !path_exists(direct_lib)) {
                fprintf(stderr, "Warning: %s/current has bin/aetherc but no lib/ or share/ — installation is incomplete.\n", home);
                fprintf(stderr, "Fix with: ae version install <version> or ./install.sh\n");
            }
            // Fall through to try other strategies
        }
        snprintf(current_compiler, sizeof(current_compiler), "%s/current/aetherc" EXE_EXT, home);
        if (path_exists(current_compiler)) {
            // Flat layout: aetherc at root of current/ with no bin/ subdirectory.
            // This is a broken install (old ae version install bug). Check if
            // share/aether/ exists — if not, warn and skip so we fall through
            // to a working toolchain.
            char share_check[1024];
            snprintf(share_check, sizeof(share_check), "%s/current/share/aether", home);
            if (dir_exists(share_check)) {
                snprintf(tc.root, sizeof(tc.root), "%s/current", home);
                strncpy(tc.compiler, current_compiler, sizeof(tc.compiler) - 1);
                if (tc.verbose) fprintf(stderr, "[toolchain] compiler=%s (via current symlink, flat layout)\n", tc.compiler);
                goto found_root;
            }
            // Also check for lib
            char lib_check[1024];
            snprintf(lib_check, sizeof(lib_check), "%s/current/lib/libaether.a", home);
            if (path_exists(lib_check)) {
                snprintf(tc.root, sizeof(tc.root), "%s/current", home);
                strncpy(tc.compiler, current_compiler, sizeof(tc.compiler) - 1);
                goto found_root;
            }
            // Check if the direct ~/.aether/ layout will work before warning
            char direct_share2[1024], direct_lib2[1024];
            snprintf(direct_share2, sizeof(direct_share2), "%s/share/aether", home);
            snprintf(direct_lib2, sizeof(direct_lib2), "%s/lib/libaether.a", home);
            if (!dir_exists(direct_share2) && !path_exists(direct_lib2)) {
                fprintf(stderr, "Warning: %s/current has aetherc but no lib/ or share/ — installation is incomplete.\n", home);
                fprintf(stderr, "Fix with: ae version install <version> or ./install.sh\n");
            }
            // Fall through to try other strategies
        }
        strncpy(tc.root, home, sizeof(tc.root) - 1);
        snprintf(tc.compiler, sizeof(tc.compiler), "%s/bin/aetherc" EXE_EXT, tc.root);
        if (tc.verbose) fprintf(stderr, "[toolchain] compiler=%s exists=%d\n", tc.compiler, path_exists(tc.compiler));
        if (path_exists(tc.compiler)) {
            // Verify AETHER_HOME has sources or lib — otherwise build will fail
            char share_check[1024], lib_check[1024];
            snprintf(share_check, sizeof(share_check), "%s/share/aether", home);
            snprintf(lib_check, sizeof(lib_check), "%s/lib/libaether.a", home);
            if (dir_exists(share_check) || path_exists(lib_check)) {
                goto found_root;
            }
            // AETHER_HOME is incomplete — fall through to other strategies
        }
    }

    // Strategy 3: Relative to ae binary — installed layout ($PREFIX/bin/ae)
    // Detect installed layout by checking for lib/aether/ (make install),
    // share/aether/ (release ZIP), or lib/libaether.a (either).
    if (found_exe_dir) {
        char candidate[1024];
        bool is_installed = false;
        snprintf(candidate, sizeof(candidate), "%s/../lib/aether", exe_dir);
        if (dir_exists(candidate)) is_installed = true;
        if (!is_installed) {
            snprintf(candidate, sizeof(candidate), "%s/../share/aether", exe_dir);
            if (dir_exists(candidate)) is_installed = true;
        }
        if (!is_installed) {
            snprintf(candidate, sizeof(candidate), "%s/../lib/libaether.a", exe_dir);
            if (path_exists(candidate)) is_installed = true;
        }
        if (is_installed) {
            // If a 'current' symlink exists (from ae version use), prefer it
            // so that version-managed stdlib files take priority over stale
            // files left by a previous install.sh in the parent directory.
            char current_root[1024];
            snprintf(current_root, sizeof(current_root), "%s/../current", exe_dir);
            if (dir_exists(current_root)) {
                char cs[1024], cl[1024];
                snprintf(cs, sizeof(cs), "%s/../current/share/aether", exe_dir);
                snprintf(cl, sizeof(cl), "%s/../current/lib/libaether.a", exe_dir);
                if (dir_exists(cs) || path_exists(cl)) {
                    snprintf(tc.root, sizeof(tc.root), "%s/../current", exe_dir);
                    snprintf(tc.compiler, sizeof(tc.compiler), "%s/aetherc" EXE_EXT, exe_dir);
                    if (path_exists(tc.compiler)) goto found_root;
                }
            }
            snprintf(tc.root, sizeof(tc.root), "%s/..", exe_dir);
            snprintf(tc.compiler, sizeof(tc.compiler), "%s/aetherc" EXE_EXT, exe_dir);
            if (path_exists(tc.compiler)) goto found_root;
        }
    }

    // Strategy 4: CWD dev mode — ./build/aetherc
    if (path_exists("build/aetherc" EXE_EXT)) {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd))) {
            strncpy(tc.root, cwd, sizeof(tc.root) - 1);
        } else {
            strcpy(tc.root, ".");
        }
        snprintf(tc.compiler, sizeof(tc.compiler), "%s/build/aetherc" EXE_EXT, tc.root);
        tc.dev_mode = true;
        goto found_root;
    }

    // Strategy 5: Standard install paths
    const char* standard_paths[] = {
        "/usr/local/bin/aetherc",
        "/usr/bin/aetherc",
        NULL
    };
    for (int i = 0; standard_paths[i]; i++) {
        if (path_exists(standard_paths[i])) {
            strncpy(tc.compiler, standard_paths[i], sizeof(tc.compiler) - 1);
            tc.compiler[sizeof(tc.compiler) - 1] = '\0';
            strncpy(tc.root, standard_paths[i], sizeof(tc.root) - 1);
            char* slash = strrchr(tc.root, '/');
            if (slash) *slash = '\0';
            slash = strrchr(tc.root, '/');
            if (slash) *slash = '\0';
            goto found_root;
        }
    }

    fprintf(stderr, "Error: Aether compiler not found.\n");
#ifdef _WIN32
    fprintf(stderr, "\n");
    fprintf(stderr, "If you downloaded a release ZIP, make sure to:\n");
    fprintf(stderr, "  1. Extract the ZIP (e.g. to C:\\aether)\n");
    fprintf(stderr, "  2. Add C:\\aether\\bin to your PATH\n");
    fprintf(stderr, "  3. Restart your terminal\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Or set AETHER_HOME to the extraction folder:\n");
    fprintf(stderr, "  set AETHER_HOME=C:\\aether\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Download: https://github.com/nicolasmd87/aether/releases\n");
#else
    fprintf(stderr, "Run 'make compiler' to build it, or set $AETHER_HOME.\n");
#endif
    exit(1);

found_root:
    // Propagate AETHER_HOME to child processes (aetherc) so module
    // resolution works even when the shell environment is not configured.
#ifdef _WIN32
    {
        char env_buf[1100];
        snprintf(env_buf, sizeof(env_buf), "AETHER_HOME=%s", tc.root);
        _putenv(env_buf);
    }
#else
    setenv("AETHER_HOME", tc.root, 0);
#endif

    if (tc.verbose) {
        fprintf(stderr, "[toolchain] root: %s\n", tc.root);
        fprintf(stderr, "[toolchain] compiler: %s\n", tc.compiler);
        fprintf(stderr, "[toolchain] dev_mode: %s\n", tc.dev_mode ? "yes" : "no");
    }

    // Check for precompiled library
    if (tc.dev_mode) {
        snprintf(tc.lib, sizeof(tc.lib), "%s/build/libaether.a", tc.root);
    } else {
        // install.sh puts lib at $root/lib/libaether.a
        // make install puts lib at $root/lib/aether/libaether.a
        snprintf(tc.lib, sizeof(tc.lib), "%s/lib/libaether.a", tc.root);
        if (!path_exists(tc.lib)) {
            snprintf(tc.lib, sizeof(tc.lib), "%s/lib/aether/libaether.a", tc.root);
        }
    }
    tc.has_lib = path_exists(tc.lib);

    if (tc.verbose) {
        fprintf(stderr, "[toolchain] lib: %s (%s)\n", tc.lib,
                tc.has_lib ? "found" : "not found, using source fallback");
    }

    // Build include flags and source file lists
    if (tc.dev_mode) {
        snprintf(tc.include_flags, sizeof(tc.include_flags),
            "-I%s/runtime -I%s/runtime/actors -I%s/runtime/scheduler "
            "-I%s/runtime/utils -I%s/runtime/memory -I%s/runtime/config "
            "-I%s/std -I%s/std/string -I%s/std/io -I%s/std/math "
            "-I%s/std/net -I%s/std/collections -I%s/std/json "
            "-I%s/std/fs -I%s/std/log",
            tc.root, tc.root, tc.root, tc.root, tc.root, tc.root,
            tc.root, tc.root, tc.root, tc.root, tc.root, tc.root,
            tc.root, tc.root, tc.root);

        if (!tc.has_lib) {
            snprintf(tc.runtime_srcs, sizeof(tc.runtime_srcs),
                "%s/runtime/scheduler/multicore_scheduler.c "
                "%s/runtime/scheduler/scheduler_optimizations.c "
                "%s/runtime/config/aether_optimization_config.c "
                "%s/runtime/memory/memory.c "
                "%s/runtime/memory/aether_arena.c "
                "%s/runtime/memory/aether_pool.c "
                "%s/runtime/memory/aether_memory_stats.c "
                "%s/runtime/utils/aether_tracing.c "
                "%s/runtime/utils/aether_bounds_check.c "
                "%s/runtime/utils/aether_test.c "
                "%s/runtime/memory/aether_arena_optimized.c "
                "%s/runtime/aether_runtime_types.c "
                "%s/runtime/utils/aether_cpu_detect.c "
                "%s/runtime/memory/aether_batch.c "
                "%s/runtime/utils/aether_simd_vectorized.c "
                "%s/runtime/aether_runtime.c "
                "%s/runtime/aether_numa.c "
                "%s/runtime/actors/aether_send_buffer.c "
                "%s/runtime/actors/aether_send_message.c "
                "%s/runtime/actors/aether_actor_thread.c "
                "%s/std/string/aether_string.c "
                "%s/std/math/aether_math.c "
                "%s/std/net/aether_http.c "
                "%s/std/net/aether_http_server.c "
                "%s/std/net/aether_net.c "
                "%s/std/collections/aether_collections.c "
                "%s/std/json/aether_json.c "
                "%s/std/io/aether_io.c "
                "%s/std/fs/aether_fs.c "
                "%s/std/log/aether_log.c "
                "%s/std/os/aether_os.c "
                "%s/std/collections/aether_hashmap.c "
                "%s/std/collections/aether_set.c "
                "%s/std/collections/aether_vector.c "
                "%s/std/collections/aether_pqueue.c",
                tc.root, tc.root, tc.root, tc.root, tc.root,
                tc.root, tc.root, tc.root, tc.root, tc.root,
                tc.root, tc.root, tc.root, tc.root, tc.root,
                tc.root, tc.root, tc.root, tc.root, tc.root,
                tc.root, tc.root, tc.root, tc.root, tc.root,
                tc.root, tc.root, tc.root, tc.root, tc.root,
                tc.root, tc.root, tc.root, tc.root, tc.root);
        }
    } else {
        // Installed layout: headers in include/aether/, source in share/aether/
        // Include both paths so source compilation can find headers via either route
        snprintf(tc.include_flags, sizeof(tc.include_flags),
            "-I%s/include/aether/runtime -I%s/include/aether/runtime/actors "
            "-I%s/include/aether/runtime/scheduler -I%s/include/aether/runtime/utils "
            "-I%s/include/aether/runtime/memory -I%s/include/aether/runtime/config "
            "-I%s/include/aether/std -I%s/include/aether/std/string "
            "-I%s/include/aether/std/io -I%s/include/aether/std/math "
            "-I%s/include/aether/std/net -I%s/include/aether/std/collections "
            "-I%s/include/aether/std/json -I%s/include/aether/std/fs "
            "-I%s/include/aether/std/log "
            "-I%s/share/aether/runtime -I%s/share/aether/runtime/actors "
            "-I%s/share/aether/runtime/scheduler -I%s/share/aether/runtime/utils "
            "-I%s/share/aether/runtime/memory -I%s/share/aether/runtime/config "
            "-I%s/share/aether/std -I%s/share/aether/std/string "
            "-I%s/share/aether/std/io -I%s/share/aether/std/math "
            "-I%s/share/aether/std/net -I%s/share/aether/std/collections "
            "-I%s/share/aether/std/json -I%s/share/aether/std/fs "
            "-I%s/share/aether/std/log",
            tc.root, tc.root, tc.root, tc.root, tc.root, tc.root,
            tc.root, tc.root, tc.root, tc.root, tc.root, tc.root,
            tc.root, tc.root, tc.root, tc.root, tc.root, tc.root,
            tc.root, tc.root, tc.root, tc.root, tc.root, tc.root,
            tc.root, tc.root, tc.root, tc.root, tc.root, tc.root);

        // Source fallback: when libaether.a is not available, compile from share/aether/
        if (!tc.has_lib) {
            char src[1024];
            snprintf(src, sizeof(src), "%s/share/aether", tc.root);
            snprintf(tc.runtime_srcs, sizeof(tc.runtime_srcs),
                "%s/runtime/scheduler/multicore_scheduler.c "
                "%s/runtime/scheduler/scheduler_optimizations.c "
                "%s/runtime/config/aether_optimization_config.c "
                "%s/runtime/memory/memory.c "
                "%s/runtime/memory/aether_arena.c "
                "%s/runtime/memory/aether_pool.c "
                "%s/runtime/memory/aether_memory_stats.c "
                "%s/runtime/utils/aether_tracing.c "
                "%s/runtime/utils/aether_bounds_check.c "
                "%s/runtime/utils/aether_test.c "
                "%s/runtime/memory/aether_arena_optimized.c "
                "%s/runtime/aether_runtime_types.c "
                "%s/runtime/utils/aether_cpu_detect.c "
                "%s/runtime/memory/aether_batch.c "
                "%s/runtime/utils/aether_simd_vectorized.c "
                "%s/runtime/aether_runtime.c "
                "%s/runtime/aether_numa.c "
                "%s/runtime/actors/aether_send_buffer.c "
                "%s/runtime/actors/aether_send_message.c "
                "%s/runtime/actors/aether_actor_thread.c "
                "%s/std/string/aether_string.c "
                "%s/std/math/aether_math.c "
                "%s/std/net/aether_http.c "
                "%s/std/net/aether_http_server.c "
                "%s/std/net/aether_net.c "
                "%s/std/collections/aether_collections.c "
                "%s/std/json/aether_json.c "
                "%s/std/io/aether_io.c "
                "%s/std/fs/aether_fs.c "
                "%s/std/log/aether_log.c "
                "%s/std/os/aether_os.c "
                "%s/std/collections/aether_hashmap.c "
                "%s/std/collections/aether_set.c "
                "%s/std/collections/aether_vector.c "
                "%s/std/collections/aether_pqueue.c",
                src, src, src, src, src,
                src, src, src, src, src,
                src, src, src, src, src,
                src, src, src, src, src,
                src, src, src, src, src,
                src, src, src, src, src,
                src, src, src, src, src);
        }
    }
}
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif

// Get link_flags from aether.toml [build] section
// Returns empty string if not found or no aether.toml
static const char* get_link_flags(void) {
    static char flags[1024] = "";
    static bool checked = false;

    if (checked) return flags;
    checked = true;

    if (!path_exists("aether.toml")) return flags;

    TomlDocument* doc = toml_parse_file("aether.toml");
    if (!doc) return flags;

    const char* val = toml_get_value(doc, "build", "link_flags");
    if (val) {
        strncpy(flags, val, sizeof(flags) - 1);
        flags[sizeof(flags) - 1] = '\0';
    }

    toml_free_document(doc);
    return flags;
}

// --------------------------------------------------------------------------
// Windows: auto-install bundled GCC (WinLibs) if none found on PATH
// --------------------------------------------------------------------------
#ifdef _WIN32

// Pinned WinLibs release — GCC 14.2.0 UCRT, x86-64, no LLVM (~250 MB).
// Update WINLIBS_TAG + WINLIBS_ZIP together when upgrading.
#define WINLIBS_TAG "14.2.0posix-12.0.0-ucrt-r3"
#define WINLIBS_ZIP "winlibs-x86_64-posix-seh-gcc-14.2.0-mingw-w64ucrt-12.0.0-r3.zip"
#define WINLIBS_URL \
    "https://github.com/brechtsanders/winlibs_mingw/releases/download/" \
    WINLIBS_TAG "/" WINLIBS_ZIP

static char s_gcc_bin[1024] = "gcc";  // path to gcc; updated by ensure_gcc_windows()
static bool s_gcc_ready      = false; // set after first successful check

// Checks PATH, then ~/.aether/tools/, then downloads WinLibs on demand.
// Returns true when gcc is usable; false means the user must intervene.
static bool ensure_gcc_windows(void) {
    if (s_gcc_ready) return true;

    // 1. Already on PATH?
    if (system("gcc --version >nul 2>&1") == 0) {
        s_gcc_ready = true;
        return true;
    }

    // 2. Already installed to ~/.aether/tools/ from a previous run?
    const char* home  = get_home_dir();
    char tools_dir[1024], tools_bin[1024], tools_gcc[1024];
    snprintf(tools_dir, sizeof(tools_dir), "%s\\.aether\\tools",           home);
    snprintf(tools_bin, sizeof(tools_bin), "%s\\mingw64\\bin",             tools_dir);
    snprintf(tools_gcc, sizeof(tools_gcc), "%s\\mingw64\\bin\\gcc.exe",    tools_dir);

    struct stat st;
    if (stat(tools_gcc, &st) == 0) goto found;

    // 3. Auto-download (one-time, ~250 MB).
    printf("[ae] GCC not found. Downloading MinGW-w64 GCC (~250 MB) -- one-time setup...\n");
    fflush(stdout);

    mkdirs(tools_dir);  // Create ~/.aether/tools/ (and parents)

    // Write a tiny PowerShell script to avoid shell-quoting nightmares.
    char ps_path[1024], zip_path[1024];
    snprintf(ps_path,  sizeof(ps_path),  "%s\\install_gcc.ps1", tools_dir);
    snprintf(zip_path, sizeof(zip_path), "%s\\mingw.zip",        tools_dir);

    FILE* ps = fopen(ps_path, "w");
    if (!ps) {
        fprintf(stderr, "[ae] Cannot write installer script to %s\n", tools_dir);
        goto fail;
    }
    fprintf(ps,
        "$ProgressPreference = 'SilentlyContinue'\n"
        "Write-Host '[ae] Downloading GCC...'\n"
        "Invoke-WebRequest -Uri '%s' -OutFile '%s'\n"
        "Write-Host '[ae] Extracting...'\n"
        "Expand-Archive -Path '%s' -DestinationPath '%s' -Force\n"
        "Remove-Item -Path '%s' -Force\n"
        "Write-Host '[ae] GCC ready.'\n",
        WINLIBS_URL, zip_path, zip_path, tools_dir, zip_path);
    fclose(ps);

    {
        char run_ps[2048];
        snprintf(run_ps, sizeof(run_ps),
            "powershell -NoProfile -ExecutionPolicy Bypass -File \"%s\"", ps_path);
        int ret = system(run_ps);
        remove(ps_path);
        if (ret != 0 || stat(tools_gcc, &st) != 0) goto fail;
    }

found:
    // Add bundled bin dir to PATH for this process so gcc is found by name too.
    {
        char cur[8192] = "", updated[8192];
        GetEnvironmentVariableA("PATH", cur, sizeof(cur));
        snprintf(updated, sizeof(updated), "%s;%s", tools_bin, cur);
        SetEnvironmentVariableA("PATH", updated);
    }
    snprintf(s_gcc_bin, sizeof(s_gcc_bin), "%s", tools_gcc);
    s_gcc_ready = true;
    return true;

fail:
    fprintf(stderr, "[ae] GCC auto-install failed. Install it manually:\n");
    fprintf(stderr, "[ae]   Option A: WinLibs (easiest) — https://winlibs.com\n");
    fprintf(stderr, "[ae]             Extract the zip, add the bin\\ folder to PATH.\n");
    fprintf(stderr, "[ae]   Option B: MSYS2 — https://www.msys2.org\n");
    fprintf(stderr, "[ae]             pacman -S mingw-w64-x86_64-gcc\n");
    return false;
}

#endif // _WIN32

// Get cflags from aether.toml [build] section (applied only for release/ae-build)
// Returns empty string if not found or no aether.toml
static const char* get_cflags(void) {
    static char flags[512] = "";
    static bool checked = false;

    if (checked) return flags;
    checked = true;

    if (!path_exists("aether.toml")) return flags;

    TomlDocument* doc = toml_parse_file("aether.toml");
    if (!doc) return flags;

    const char* val = toml_get_value(doc, "build", "cflags");
    if (val) {
        strncpy(flags, val, sizeof(flags) - 1);
        flags[sizeof(flags) - 1] = '\0';
    }

    toml_free_document(doc);
    return flags;
}

// Get extra_sources for the [[bin]] entry whose path matches ae_file.
// Writes space-separated C source paths into out[out_size].
// Only handles single-line arrays: extra_sources = ["a.c", "b.c"]
static void get_extra_sources_for_bin(const char* ae_file, char* out, size_t out_size) {
    out[0] = '\0';
    if (!ae_file || !path_exists("aether.toml")) return;

    FILE* f = fopen("aether.toml", "r");
    if (!f) return;

    char line[1024];
    int in_bin = 0;
    int matched = 0;

    while (fgets(line, sizeof(line), f)) {
        char* s = line;
        while (*s == ' ' || *s == '\t') s++;
        size_t ln = strlen(s);
        while (ln > 0 && (s[ln-1] == '\n' || s[ln-1] == '\r' || s[ln-1] == ' ')) s[--ln] = '\0';
        if (!s[0] || s[0] == '#') continue;

        // [[bin]] section marker
        if (strncmp(s, "[[bin]]", 7) == 0) {
            in_bin = 1;
            matched = 0;
            continue;
        }

        // Other section resets context
        if (s[0] == '[' && s[1] != '[') {
            in_bin = 0;
            matched = 0;
            continue;
        }

        if (!in_bin) continue;

        // path = "..." — check if this bin entry matches ae_file
        if (strncmp(s, "path", 4) == 0 && strchr(s, '=')) {
            char* eq = strchr(s, '=') + 1;
            while (*eq == ' ') eq++;
            if (*eq == '"') eq++;
            char* end = strrchr(eq, '"');
            if (end) *end = '\0';
            // Normalize: strip leading "./"
            const char* aef = ae_file;
            if (aef[0] == '.' && aef[1] == '/') aef += 2;
            if (eq[0] == '.' && eq[1] == '/') eq += 2;
            // Match if equal or ae_file ends with the path value
            size_t vlen = strlen(eq);
            size_t alen = strlen(aef);
            if (strcmp(aef, eq) == 0 ||
                (alen >= vlen && aef[alen - vlen - 1] == '/' &&
                 strcmp(aef + alen - vlen, eq) == 0)) {
                matched = 1;
            }
            continue;
        }

        // extra_sources = ["a.c", "b.c"] in a matched [[bin]]
        if (matched && strncmp(s, "extra_sources", 13) == 0 && strchr(s, '=')) {
            char* eq = strchr(s, '=') + 1;
            while (*eq == ' ') eq++;
            if (*eq != '[') continue;
            eq++; // skip '['
            while (*eq && *eq != ']') {
                while (*eq == ' ' || *eq == ',') eq++;
                if (*eq == ']' || !*eq) break;
                if (*eq == '"') {
                    eq++;
                    char* end = strchr(eq, '"');
                    if (!end) break;
                    *end = '\0';
                    if (out[0]) strncat(out, " ", out_size - strlen(out) - 1);
                    strncat(out, eq, out_size - strlen(out) - 1);
                    eq = end + 1;
                } else {
                    eq++;
                }
            }
            break;
        }
    }
    fclose(f);
}

// --------------------------------------------------------------------------
// Build GCC/MinGW command for linking an Aether-compiled C file
static void build_gcc_cmd(char* cmd, size_t size,
                          const char* c_file, const char* out_file,
                          bool optimize, const char* extra_files) {
    const char* link_flags = get_link_flags();
    const char* extra = extra_files ? extra_files : "";

    // User cflags from aether.toml applied only for release builds (ae build)
    const char* user_cflags = optimize ? get_cflags() : "";

#ifdef _WIN32
    // Ensure GCC is available (auto-downloads WinLibs on first run if needed).
    if (!ensure_gcc_windows()) {
        snprintf(cmd, size, "exit 1");  // will fail; error already printed
        return;
    }
    // Windows (MinGW): no -pthread (Win32 threads via aether_thread.h), no -lm (CRT).
    // -lws2_32 is required for Winsock2 (aether_http/net always compiled into runtime).
    // -static links libwinpthread/libgcc into the binary so it runs without MinGW DLLs.
    // Quote s_gcc_bin in case the path contains spaces.
    char opt[600];
    if (user_cflags[0])
        snprintf(opt, sizeof(opt), "-static %s %s", optimize ? "-O2" : "-O0 -g", user_cflags);
    else
        snprintf(opt, sizeof(opt), "-static %s", optimize ? "-O2" : "-O0 -g");
    char lib_dir[1024];
    if (tc.has_lib) {
        strncpy(lib_dir, tc.lib, sizeof(lib_dir) - 1);
        lib_dir[sizeof(lib_dir) - 1] = '\0';
        char* bs = strrchr(lib_dir, '\\');
        char* fs = strrchr(lib_dir, '/');
        char* slash = (!bs) ? fs : (!fs) ? bs : (bs > fs ? bs : fs);
        if (slash) *slash = '\0';
        snprintf(cmd, size,
            "\"%s\" %s %s \"%s\" %s -L\"%s\" -laether -o \"%s\" -lws2_32 %s",
            s_gcc_bin, opt, tc.include_flags, c_file, extra, lib_dir, out_file, link_flags);
    } else {
        snprintf(cmd, size,
            "\"%s\" %s %s \"%s\" %s %s -o \"%s\" -lws2_32 %s",
            s_gcc_bin, opt, tc.include_flags, c_file, extra, tc.runtime_srcs, out_file, link_flags);
    }
#else
    // POSIX (Linux/macOS): -pthread for POSIX threads, -lm for math
    // Pre-flight check: ensure gcc (or cc) is available
    if (system("command -v gcc >/dev/null 2>&1") != 0 &&
        system("command -v cc >/dev/null 2>&1") != 0) {
        fprintf(stderr, "Error: C compiler not found (gcc or cc).\n");
#ifdef __APPLE__
        fprintf(stderr, "Install Xcode Command Line Tools: xcode-select --install\n");
#else
        fprintf(stderr, "Install GCC: sudo apt install gcc  (Debian/Ubuntu)\n");
        fprintf(stderr, "             sudo dnf install gcc  (Fedora)\n");
#endif
        snprintf(cmd, size, "false");
        return;
    }
    char opt[600];
    if (user_cflags[0])
        snprintf(opt, sizeof(opt), "%s %s", optimize ? "-O2 -pipe" : "-O0 -g -pipe", user_cflags);
    else
        snprintf(opt, sizeof(opt), "%s", optimize ? "-O2 -pipe" : "-O0 -g -pipe");
    if (tc.has_lib) {
        char lib_dir[1024];
        strncpy(lib_dir, tc.lib, sizeof(lib_dir) - 1);
        lib_dir[sizeof(lib_dir) - 1] = '\0';
        char* slash = strrchr(lib_dir, '/');
        if (slash) *slash = '\0';

        snprintf(cmd, size,
            "gcc %s %s \"%s\" %s -L%s -laether -o \"%s\" -pthread -lm %s",
            opt, tc.include_flags, c_file, extra, lib_dir, out_file, link_flags);
    } else {
        snprintf(cmd, size,
            "gcc %s %s \"%s\" %s %s -o \"%s\" -pthread -lm %s",
            opt, tc.include_flags, c_file, extra, tc.runtime_srcs, out_file, link_flags);
    }
#endif
}

static int build_wasm_cmd(char* cmd, size_t size,
                          const char* c_file, const char* out_file) {
    // Build include paths from toolchain root
    char includes[8192];
    if (tc.include_flags[0]) {
        strncpy(includes, tc.include_flags, sizeof(includes) - 1);
        includes[sizeof(includes) - 1] = '\0';
    } else {
        static const char* include_dirs[] = {
            "runtime", "runtime/actors", "runtime/scheduler",
            "runtime/utils", "runtime/memory", "runtime/config",
            "std", "std/string", "std/io", "std/math",
            "std/net", "std/collections", "std/json", NULL
        };
        includes[0] = '\0';
        for (int i = 0; include_dirs[i]; i++) {
            char flag[2048];
            snprintf(flag, sizeof(flag), "-I%s/%s ", tc.root, include_dirs[i]);
            strncat(includes, flag, sizeof(includes) - strlen(includes) - 1);
        }
    }

    // Runtime source files (cooperative scheduler, not multicore)
    static const char* wasm_runtime_files[] = {
        "runtime/scheduler/aether_scheduler_coop.c",
        "runtime/scheduler/scheduler_optimizations.c",
        "runtime/config/aether_optimization_config.c",
        "runtime/memory/memory.c",
        "runtime/memory/aether_arena.c",
        "runtime/memory/aether_pool.c",
        "runtime/memory/aether_memory_stats.c",
        "runtime/utils/aether_tracing.c",
        "runtime/utils/aether_bounds_check.c",
        "runtime/utils/aether_test.c",
        "runtime/memory/aether_arena_optimized.c",
        "runtime/aether_runtime_types.c",
        "runtime/utils/aether_cpu_detect.c",
        "runtime/memory/aether_batch.c",
        "runtime/utils/aether_simd_vectorized.c",
        "runtime/aether_runtime.c",
        "runtime/aether_numa.c",
        "runtime/actors/aether_send_buffer.c",
        "runtime/actors/aether_send_message.c",
        "runtime/actors/aether_actor_thread.c",
        "std/string/aether_string.c",
        "std/math/aether_math.c",
        "std/net/aether_http.c",
        "std/net/aether_http_server.c",
        "std/net/aether_net.c",
        "std/collections/aether_collections.c",
        "std/json/aether_json.c",
        "std/fs/aether_fs.c",
        "std/log/aether_log.c",
        "std/io/aether_io.c",
        "std/os/aether_os.c",
        "std/collections/aether_hashmap.c",
        "std/collections/aether_set.c",
        "std/collections/aether_vector.c",
        "std/collections/aether_pqueue.c",
        NULL
    };
    char runtime[8192];
    runtime[0] = '\0';
    for (int i = 0; wasm_runtime_files[i]; i++) {
        char path[2048];
        snprintf(path, sizeof(path), "%s/%s ", tc.root, wasm_runtime_files[i]);
        strncat(runtime, path, sizeof(runtime) - strlen(runtime) - 1);
    }

    snprintf(cmd, size,
        "emcc -O2 -DAETHER_NO_THREADING -DAETHER_NO_FILESYSTEM -DAETHER_NO_NETWORKING "
        "%s \"%s\" %s -o \"%s\" -lm "
        "-Wall -Wextra -Wno-unused-parameter -Wno-unused-function "
        "-Wno-unused-variable -Wno-missing-field-initializers -Wno-unused-label",
        includes, c_file, runtime, out_file);

    return 1;
}

// --------------------------------------------------------------------------
// Commands
// --------------------------------------------------------------------------

static int cmd_run(int argc, char** argv) {
    const char* file = NULL;
    char extra_files[2048] = "";

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--extra") == 0 && i + 1 < argc) {
            if (extra_files[0]) strncat(extra_files, " ", sizeof(extra_files) - strlen(extra_files) - 1);
            strncat(extra_files, argv[++i], sizeof(extra_files) - strlen(extra_files) - 1);
        } else if (argv[i][0] != '-' && !file) {
            file = argv[i];
        }
    }

    // Resolve directory argument (e.g. "." or "myproject/") to src/main.ae
    if (file && dir_exists(file)) {
        static char resolved_run_file[512];
        snprintf(resolved_run_file, sizeof(resolved_run_file), "%s/src/main.ae", file);
        if (path_exists(resolved_run_file)) {
            file = resolved_run_file;
        } else {
            char toml_path[512];
            snprintf(toml_path, sizeof(toml_path), "%s/aether.toml", file);
            if (path_exists(toml_path))
                fprintf(stderr, "Error: No src/main.ae found in %s\n", file);
            else
                fprintf(stderr, "Error: '%s' is not an Aether project directory\n", file);
            return 1;
        }
    }

    // Project mode: no file argument, look for aether.toml
    if (!file && path_exists("aether.toml")) {
        if (path_exists("src/main.ae"))
            file = "src/main.ae";
        else {
            fprintf(stderr, "Error: aether.toml found but src/main.ae is missing.\n");
            fprintf(stderr, "Create src/main.ae or specify a file: ae run <file.ae>\n");
            return 1;
        }
    }

    if (!file) {
        fprintf(stderr, "Error: No input file specified.\n");
        fprintf(stderr, "Usage: ae run <file.ae>\n");
        fprintf(stderr, "   or: Create a project with 'ae init <name>'\n");
        return 1;
    }

    if (!path_exists(file)) {
        fprintf(stderr, "Error: File not found: %s\n", file);
        return 1;
    }

    char c_file[2048], exe_file[2048], cmd[16384];

    // --- Cache check ---
    // ae run uses -O0 (fast dev builds). Check if we have a cached exe for
    // this exact source + compiler combination.
    bool using_cache = false;
    char cached_exe[1024] = "";
    unsigned long long cache_key = compute_cache_key(file, "");
    if (cache_key != 0) {
        init_cache_dir();
        snprintf(cached_exe, sizeof(cached_exe), "%s/%016llx" EXE_EXT, s_cache_dir, cache_key);
        if (path_exists(cached_exe)) {
            if (tc.verbose) fprintf(stderr, "[cache] hit: %016llx\n", cache_key);
            snprintf(cmd, sizeof(cmd), "%s", cached_exe);
            int rc = run_cmd(cmd);
            if (rc < 0) {
                fprintf(stderr, "Program crashed (signal %d", -rc);
                if (-rc == 11) fprintf(stderr, ": segmentation fault");
                else if (-rc == 6) fprintf(stderr, ": aborted");
                fprintf(stderr, ")\n");
            }
            return rc;
        }
        if (tc.verbose) fprintf(stderr, "[cache] miss: %016llx\n", cache_key);
        using_cache = true;
    }

    // Determine temp .c file path and exe path
    // If caching: write exe directly to cache slot (no extra copy needed)
    // Use PID in temp filenames to avoid symlink attacks and collisions
    int pid = (int)getpid();
    if (tc.dev_mode) {
        snprintf(c_file, sizeof(c_file), "%s/build/_ae_%d.c", tc.root, pid);
    } else {
        snprintf(c_file, sizeof(c_file), "%s/_ae_%d.c", get_temp_dir(), pid);
    }
    if (using_cache) {
        strncpy(exe_file, cached_exe, sizeof(exe_file) - 1);
        exe_file[sizeof(exe_file) - 1] = '\0';
    } else if (tc.dev_mode) {
        snprintf(exe_file, sizeof(exe_file), "%s/build/_ae_%d" EXE_EXT, tc.root, pid);
    } else {
        snprintf(exe_file, sizeof(exe_file), "%s/_ae_%d" EXE_EXT, get_temp_dir(), pid);
    }

    // Step 1: Compile .ae to .c
    if (tc.verbose) printf("Compiling %s...\n", file);
    snprintf(cmd, sizeof(cmd), "\"%s\" \"%s\" \"%s\"", tc.compiler, file, c_file);

    int aetherc_ret = tc.verbose ? run_cmd(cmd) : run_cmd_quiet(cmd);
    if (aetherc_ret != 0) {
        // Re-run with output visible so user can see the error
        snprintf(cmd, sizeof(cmd), "\"%s\" \"%s\" \"%s\"", tc.compiler, file, c_file);
        run_cmd(cmd);
        fprintf(stderr, "Compilation failed.\n");
        return 1;
    }

    // Step 2: Compile .c to executable with runtime (-O0 for fast dev builds)
    // Merge extra_sources from aether.toml [[bin]] with any --extra CLI args
    char toml_extra[2048] = "";
    get_extra_sources_for_bin(file, toml_extra, sizeof(toml_extra));
    if (toml_extra[0]) {
        if (extra_files[0]) strncat(extra_files, " ", sizeof(extra_files) - strlen(extra_files) - 1);
        strncat(extra_files, toml_extra, sizeof(extra_files) - strlen(extra_files) - 1);
    }
    const char* run_extra = extra_files[0] ? extra_files : NULL;
    build_gcc_cmd(cmd, sizeof(cmd), c_file, exe_file, false, run_extra);
    // Show stderr (gcc warnings like -Wformat) even in non-verbose mode
    int gcc_ret = tc.verbose ? run_cmd(cmd) : run_cmd_show_warnings(cmd);
    if (gcc_ret != 0) {
        // Re-run with output for error diagnosis
        build_gcc_cmd(cmd, sizeof(cmd), c_file, exe_file, false, run_extra);
        run_cmd(cmd);
        fprintf(stderr, "Build failed.\n");
        remove(c_file);
        return 1;
    }

    // Clean up temp .c file (exe stays in cache if caching, else clean up too)
    remove(c_file);

    // Step 3: Run
    snprintf(cmd, sizeof(cmd), "\"%s\"", exe_file);
    int rc = run_cmd(cmd);

    if (rc < 0) {
        fprintf(stderr, "Program crashed (signal %d", -rc);
        if (-rc == 11) fprintf(stderr, ": segmentation fault");
        else if (-rc == 6) fprintf(stderr, ": aborted");
        fprintf(stderr, ")\n");
        // Remove crashed binary from cache so next run recompiles
        if (using_cache) remove(exe_file);
    }

    // If not cached, remove the temp exe
    if (!using_cache) remove(exe_file);

    return rc;
}

static int cmd_check(int argc, char** argv) {
    const char* file = NULL;

    for (int i = 0; i < argc; i++) {
        if (argv[i][0] != '-') {
            file = argv[i];
        }
    }

    // Project mode
    if (!file && path_exists("aether.toml")) {
        if (path_exists("src/main.ae"))
            file = "src/main.ae";
        else {
            fprintf(stderr, "Error: aether.toml found but src/main.ae is missing.\n");
            return 1;
        }
    }

    if (!file) {
        fprintf(stderr, "Usage: ae check <file.ae>\n");
        return 1;
    }

    if (!path_exists(file)) {
        fprintf(stderr, "Error: File not found: %s\n", file);
        return 1;
    }

    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "\"%s\" --check \"%s\"", tc.compiler, file);
    return run_cmd(cmd);
}

static int cmd_build(int argc, char** argv) {
    const char* file = NULL;
    const char* output_name = NULL;
    char extra_files[2048] = "";

    const char* target = NULL;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_name = argv[++i];
        } else if (strcmp(argv[i], "--target") == 0 && i + 1 < argc) {
            target = argv[++i];
        } else if (strcmp(argv[i], "--extra") == 0 && i + 1 < argc) {
            if (extra_files[0]) strncat(extra_files, " ", sizeof(extra_files) - strlen(extra_files) - 1);
            strncat(extra_files, argv[++i], sizeof(extra_files) - strlen(extra_files) - 1);
        } else if (argv[i][0] != '-') {
            file = argv[i];
        }
    }

    // Read target from aether.toml if not specified on CLI
    if (!target && path_exists("aether.toml")) {
        static char toml_target[64];
        TomlDocument* doc = toml_parse_file("aether.toml");
        if (doc) {
            const char* val = toml_get_value(doc, "build", "target");
            if (val && strcmp(val, "native") != 0) {
                strncpy(toml_target, val, sizeof(toml_target) - 1);
                toml_target[sizeof(toml_target) - 1] = '\0';
                target = toml_target;
            }
            toml_free_document(doc);
        }
    }

    // Validate target
    if (target && strcmp(target, "wasm") != 0 && strcmp(target, "native") != 0) {
        fprintf(stderr, "Error: Unknown target '%s'. Valid targets: native, wasm\n", target);
        return 1;
    }
    int is_wasm = target && strcmp(target, "wasm") == 0;

    // Resolve directory argument (e.g. "." or "myproject/") to src/main.ae
    if (file && dir_exists(file)) {
        static char resolved_build_file[512];
        snprintf(resolved_build_file, sizeof(resolved_build_file), "%s/src/main.ae", file);
        if (path_exists(resolved_build_file)) {
            file = resolved_build_file;
        } else {
            char toml_path[512];
            snprintf(toml_path, sizeof(toml_path), "%s/aether.toml", file);
            if (path_exists(toml_path))
                fprintf(stderr, "Error: No src/main.ae found in %s\n", file);
            else
                fprintf(stderr, "Error: '%s' is not an Aether project directory\n", file);
            return 1;
        }
    }

    // Project mode
    if (!file && path_exists("aether.toml")) {
        if (path_exists("src/main.ae"))
            file = "src/main.ae";
        else {
            fprintf(stderr, "Error: aether.toml found but src/main.ae is missing.\n");
            fprintf(stderr, "Create src/main.ae or specify a file: ae build <file.ae>\n");
            return 1;
        }
    }

    if (!file) {
        fprintf(stderr, "Error: No input file specified.\n");
        fprintf(stderr, "Usage: ae build <file.ae> [-o output] [--extra file.c]\n");
        return 1;
    }

    if (!path_exists(file)) {
        fprintf(stderr, "Error: File not found: %s\n", file);
        return 1;
    }

    const char* base = get_basename(file);
    char c_file[2048], exe_file[2048], cmd[16384];

    if (output_name) {
        // Explicit -o: use the path as-is
        snprintf(c_file, sizeof(c_file), "%s.c", output_name);
        snprintf(exe_file, sizeof(exe_file), "%s" EXE_EXT, output_name);
    } else if (path_exists("aether.toml")) {
        // Project mode: output to target/
        mkdirs("target");
        snprintf(c_file, sizeof(c_file), "target/%s.c", base);
        snprintf(exe_file, sizeof(exe_file), "target/%s" EXE_EXT, base);
    } else if (tc.dev_mode) {
        snprintf(c_file, sizeof(c_file), "%s/build/%s.c", tc.root, base);
        snprintf(exe_file, sizeof(exe_file), "%s/build/%s" EXE_EXT, tc.root, base);
    } else {
        snprintf(c_file, sizeof(c_file), "%s.c", base);
        snprintf(exe_file, sizeof(exe_file), "%s" EXE_EXT, base);
    }

    // Override output extension for wasm target
    if (is_wasm) {
        // Replace .exe or binary with .js (emcc produces .js + .wasm pair)
        char* dot = strrchr(exe_file, '.');
        if (dot && strcmp(dot, EXE_EXT) == 0) {
            strcpy(dot, ".js");
        } else {
            strncat(exe_file, ".js", sizeof(exe_file) - strlen(exe_file) - 1);
        }
    }

    // Pre-flight: verify emcc for wasm target before starting compilation
    if (is_wasm && run_cmd_quiet("emcc --version") != 0) {
        fprintf(stderr, "Error: Emscripten (emcc) not found on PATH.\n");
        fprintf(stderr, "Install: https://emscripten.org/docs/getting_started/downloads.html\n");
        fprintf(stderr, "  git clone https://github.com/emscripten-core/emsdk.git\n");
        fprintf(stderr, "  cd emsdk && ./emsdk install latest && ./emsdk activate latest\n");
        fprintf(stderr, "  source ./emsdk_env.sh\n");
        return 1;
    }

    printf("Building %s%s...\n", file, is_wasm ? " (wasm)" : "");

    // Step 1: .ae to .c
    snprintf(cmd, sizeof(cmd), "\"%s\" \"%s\" \"%s\"", tc.compiler, file, c_file);

    // Always run visible on failure; print diagnostic on Windows
    int aetherc_ret = tc.verbose ? run_cmd(cmd) : run_cmd_quiet(cmd);
    if (aetherc_ret != 0) {
        fprintf(stderr, "[diag] aetherc returned %d for: %s\n", aetherc_ret, file);
        fprintf(stderr, "[diag] cmd: %s\n", cmd);
        // Retry visible
        snprintf(cmd, sizeof(cmd), "\"%s\" \"%s\" \"%s\"", tc.compiler, file, c_file);
        int retry_ret = run_cmd(cmd);
        fprintf(stderr, "[diag] retry returned %d\n", retry_ret);
        fprintf(stderr, "Compilation failed.\n");
        return 1;
    }

    // Step 2: .c to executable (or wasm) with runtime
    if (is_wasm) {
        if (!build_wasm_cmd(cmd, sizeof(cmd), c_file, exe_file)) {
            return 1;
        }
    } else {
        // Merge extra_sources from aether.toml [[bin]] with any --extra CLI args
        {
            char toml_extra[2048] = "";
            get_extra_sources_for_bin(file, toml_extra, sizeof(toml_extra));
            if (toml_extra[0]) {
                if (extra_files[0]) strncat(extra_files, " ", sizeof(extra_files) - strlen(extra_files) - 1);
                strncat(extra_files, toml_extra, sizeof(extra_files) - strlen(extra_files) - 1);
            }
        }
        const char* extra = extra_files[0] ? extra_files : NULL;
        build_gcc_cmd(cmd, sizeof(cmd), c_file, exe_file, true, extra);
    }

    int build_ret = tc.verbose ? run_cmd(cmd) : run_cmd_quiet(cmd);
    if (build_ret != 0) {
        // Retry with visible output for error messages
        if (is_wasm) {
            build_wasm_cmd(cmd, sizeof(cmd), c_file, exe_file);
        } else {
            const char* extra = extra_files[0] ? extra_files : NULL;
            build_gcc_cmd(cmd, sizeof(cmd), c_file, exe_file, true, extra);
        }
        run_cmd(cmd);
        fprintf(stderr, "Build failed.\n");
        return 1;
    }

    // Clean up intermediate C file — ae build produces a binary, not C source
    remove(c_file);

    printf("Built: %s\n", exe_file);
    if (is_wasm) {
        // .wasm file is co-located with .js
        char wasm_file[2048];
        strncpy(wasm_file, exe_file, sizeof(wasm_file) - 1);
        char* js_ext = strrchr(wasm_file, '.');
        if (js_ext) strcpy(js_ext, ".wasm");
        printf("       %s\n", wasm_file);
        printf("Run with: node %s\n", exe_file);
    }
    return 0;
}

static int cmd_init(int argc, char** argv) {
    if (argc < 1 || argv[0][0] == '-') {
        fprintf(stderr, "Usage: ae init <name>\n");
        return 1;
    }

    const char* name = argv[0];

    if (dir_exists(name)) {
        fprintf(stderr, "Error: Directory '%s' already exists.\n", name);
        return 1;
    }

    printf("Creating new Aether project '%s'...\n\n", name);
    mkdirs(name);

    char path[1024];
    FILE* f;

    // aether.toml
    snprintf(path, sizeof(path), "%s/aether.toml", name);
    f = fopen(path, "w");
    if (!f) { fprintf(stderr, "Error: Could not create %s\n", path); return 1; }
    fprintf(f, "[package]\n");
    fprintf(f, "name = \"%s\"\n", name);
    fprintf(f, "version = \"0.1.0\"\n");
    fprintf(f, "description = \"A new Aether project\"\n");
    fprintf(f, "license = \"MIT\"\n\n");
    fprintf(f, "[[bin]]\n");
    fprintf(f, "name = \"%s\"\n", name);
    fprintf(f, "path = \"src/main.ae\"\n\n");
    fprintf(f, "[dependencies]\n\n");
    fprintf(f, "[build]\n");
    fprintf(f, "target = \"native\"\n");
    fprintf(f, "# link_flags = \"-lsqlite3 -lcurl\"  # Add extra linker flags\n");
    fclose(f);

    // src/main.ae
    snprintf(path, sizeof(path), "%s/src", name);
    mkdirs(path);
    snprintf(path, sizeof(path), "%s/src/main.ae", name);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "main() {\n");
        fprintf(f, "    print(\"Hello from %s!\\n\");\n", name);
        fprintf(f, "}\n");
        fclose(f);
    }

    // tests/
    snprintf(path, sizeof(path), "%s/tests", name);
    mkdirs(path);

    // README.md
    snprintf(path, sizeof(path), "%s/README.md", name);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "# %s\n\nAn Aether project.\n\n", name);
        fprintf(f, "## Quick Start\n\n```bash\nae run\n```\n\n");
        fprintf(f, "## Build\n\n```bash\nae build\n```\n\n");
        fprintf(f, "## Test\n\n```bash\nae test\n```\n");
        fclose(f);
    }

    // .gitignore
    snprintf(path, sizeof(path), "%s/.gitignore", name);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "target/\nbuild/\n*.o\naether.lock\n");
        fclose(f);
    }

    printf("  Created %s/aether.toml\n", name);
    printf("  Created %s/src/main.ae\n", name);
    printf("  Created %s/tests/\n", name);
    printf("  Created %s/README.md\n", name);
    printf("  Created %s/.gitignore\n\n", name);
    printf("Get started:\n");
    printf("  cd %s\n", name);
    printf("  ae run\n");

    return 0;
}

static int cmd_test(int argc, char** argv) {
    const char* target = NULL;
    for (int i = 0; i < argc; i++) {
        if (argv[i][0] != '-') {
            if (!is_safe_path(argv[i])) {
                fprintf(stderr, "Error: Invalid characters in path\n");
                return 1;
            }
            target = argv[i];
            break;
        }
    }

    // Collect test files
    char test_files[256][512];
    int test_count = 0;

    if (target && path_exists(target) && !dir_exists(target)) {
        // Single file
        strncpy(test_files[0], target, sizeof(test_files[0]) - 1);
        test_files[0][sizeof(test_files[0]) - 1] = '\0';
        test_count = 1;
    } else {
        // Discover from directory
        const char* test_dir = "tests";
        if (target && dir_exists(target)) {
            static char resolved_test_dir[512];
            snprintf(resolved_test_dir, sizeof(resolved_test_dir), "%s/tests", target);
            test_dir = dir_exists(resolved_test_dir) ? resolved_test_dir : target;
        }

        if (!dir_exists(test_dir)) {
            printf("No tests/ directory found.\n");
            printf("Create tests in tests/ or run: ae test <file.ae>\n");
            return 0;
        }

        char find_cmd[1024];
#ifdef _WIN32
        snprintf(find_cmd, sizeof(find_cmd),
            "dir /b /s \"%s\\*.ae\" 2>nul", test_dir);
#else
        snprintf(find_cmd, sizeof(find_cmd),
            "find \"%s\" \\( -name 'test_*.ae' -o -name '*_test.ae' \\) -type f 2>/dev/null | sort",
            test_dir);
#endif
        FILE* pipe = popen(find_cmd, "r");
        if (pipe) {
            char line[512];
            while (fgets(line, sizeof(line), pipe) && test_count < 256) {
                line[strcspn(line, "\r\n")] = '\0';
                if (strlen(line) == 0) continue;
                // Convention: only files named test_*.ae or *_test.ae are tests
                // (like pytest's test_*.py or Go's *_test.go)
                const char* base = strrchr(line, '/');
                if (!base) base = strrchr(line, '\\');
                base = base ? base + 1 : line;
                if (strncmp(base, "test_", 5) != 0) {
                    // Check *_test.ae pattern
                    const char* ext = strstr(base, "_test.ae");
                    if (!ext || strcmp(ext, "_test.ae") != 0) continue;
                }
                strncpy(test_files[test_count], line, sizeof(test_files[0]) - 1);
                test_files[test_count][sizeof(test_files[0]) - 1] = '\0';
                test_count++;
            }
            pclose(pipe);
        }
    }

    if (test_count == 0) {
        printf("No test files found.\n");
        return 0;
    }

    printf("Running %d test(s)...\n\n", test_count);

    int passed = 0, failed = 0;

    for (int i = 0; i < test_count; i++) {
        const char* test = test_files[i];
        printf("  %-45s ", test);
        fflush(stdout);

        char c_file[2048], exe_file[2048], cmd[16384];

        if (tc.dev_mode) {
            snprintf(c_file, sizeof(c_file), "%s/build/_test_%d.c", tc.root, i);
            snprintf(exe_file, sizeof(exe_file), "%s/build/_test_%d" EXE_EXT, tc.root, i);
        } else {
            snprintf(c_file, sizeof(c_file), "%s/_ae_test_%d.c", get_temp_dir(), i);
            snprintf(exe_file, sizeof(exe_file), "%s/_ae_test_%d" EXE_EXT, get_temp_dir(), i);
        }

        // Compile .ae to .c
        // GCC conservatively assumes argv paths may be PATH_MAX-sized; cmd[8192]
        // is sufficient for real-world paths (compiler + test + c_file < 8KB).
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wformat-truncation"
#endif
        snprintf(cmd, sizeof(cmd), "\"%s\" \"%s\" \"%s\"", tc.compiler, test, c_file);
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
        if (run_cmd_quiet(cmd) != 0) {
            printf("FAIL (compile)\n");
            failed++;
            continue;
        }

        // Compile .c to executable
        build_gcc_cmd(cmd, sizeof(cmd), c_file, exe_file, false, NULL);
        if (run_cmd_quiet(cmd) != 0) {
            printf("FAIL (build)\n");
            failed++;
            remove(c_file);
            continue;
        }

        // Run
        snprintf(cmd, sizeof(cmd), "\"%s\"", exe_file);
        int rc = run_cmd_quiet(cmd);
        if (rc == 0) {
            printf("PASS\n");
            passed++;
        } else {
            printf("FAIL (exit %d)\n", rc);
            failed++;
        }

        remove(c_file);
        remove(exe_file);
    }

    printf("\n%d passed, %d failed, %d total\n", passed, failed, test_count);
    return (failed > 0) ? 1 : 0;
}

static int cmd_add(int argc, char** argv) {
    if (argc < 1 || argv[0][0] == '-') {
        fprintf(stderr, "Usage: ae add <host>/<user>/<repo>[@version]\n");
        fprintf(stderr, "Examples:\n");
        fprintf(stderr, "  ae add github.com/user/repo\n");
        fprintf(stderr, "  ae add github.com/user/repo@v1.2.0\n");
        fprintf(stderr, "  ae add gitlab.com/user/repo\n");
        return 1;
    }

    // Parse package@version
    char pkg_buf[1024];
    strncpy(pkg_buf, argv[0], sizeof(pkg_buf) - 1);
    pkg_buf[sizeof(pkg_buf) - 1] = '\0';

    const char* version = NULL;
    char* at = strchr(pkg_buf, '@');
    if (at) {
        *at = '\0';
        version = at + 1;
    }
    const char* package = pkg_buf;

    if (!path_exists("aether.toml")) {
        fprintf(stderr, "Error: No aether.toml found. Run 'ae init <name>' first.\n");
        return 1;
    }

    // Validate: must look like a git-hostable URL (host.tld/user/repo)
    // Supports GitHub, GitLab, Bitbucket, Codeberg, self-hosted, etc.
    if (!strchr(package, '/') || !strchr(package, '.')) {
        fprintf(stderr, "Error: Package must be a git-hostable path.\n");
        fprintf(stderr, "Format: ae add <host>/<user>/<repo>[@version]\n");
        fprintf(stderr, "Examples:\n");
        fprintf(stderr, "  ae add github.com/user/repo\n");
        fprintf(stderr, "  ae add gitlab.com/user/repo@v1.0.0\n");
        fprintf(stderr, "  ae add codeberg.org/user/repo\n");
        return 1;
    }

    // Validate package name to prevent command injection
    if (!is_safe_shell_arg(package)) {
        fprintf(stderr, "Error: Package name contains invalid characters.\n");
        return 1;
    }

    printf("Adding %s%s%s...\n", package, version ? "@" : "", version ? version : "");

    // Cache directory — sized generously so GCC's -Wformat-truncation
    // doesn't complain (real paths are ~60 bytes, never close to limits)
    char cache_dir[512];
    snprintf(cache_dir, sizeof(cache_dir), "%s/.aether/packages", get_home_dir());

    char pkg_dir[1024];
    snprintf(pkg_dir, sizeof(pkg_dir), "%.511s/%.511s", cache_dir, package);

    if (!dir_exists(pkg_dir)) {
        printf("Downloading...\n");
        char parent[1024];
        strncpy(parent, pkg_dir, sizeof(parent) - 1);
        parent[sizeof(parent) - 1] = '\0';
        char* slash = strrchr(parent, '/');
        if (slash) { *slash = '\0'; mkdirs(parent); }

        char cmd[4096];
        if (version) {
            snprintf(cmd, sizeof(cmd), "git clone https://%s %s", package, pkg_dir);
        } else {
            snprintf(cmd, sizeof(cmd), "git clone --depth 1 https://%s %s", package, pkg_dir);
        }
        if (run_cmd(cmd) != 0) {
            fprintf(stderr, "Failed to download package.\n");
            fprintf(stderr, "Check that the repository exists: https://%s\n", package);
            return 1;
        }

        // Checkout specific version tag if requested
        if (version) {
            char tag[128];
            if (version[0] == 'v') {
                snprintf(tag, sizeof(tag), "%s", version);
            } else {
                snprintf(tag, sizeof(tag), "v%s", version);
            }
            snprintf(cmd, sizeof(cmd), "cd \"%s\" && git checkout %s 2>/dev/null || git checkout v%s 2>/dev/null",
                     pkg_dir, tag, version);
            if (run_cmd_quiet(cmd) != 0) {
                fprintf(stderr, "Error: Version '%s' not found.\n", version);
                // List available tags
                snprintf(cmd, sizeof(cmd), "cd \"%s\" && git tag -l 'v*' | sort -V | tail -10", pkg_dir);
                fprintf(stderr, "Available versions:\n");
                (void)run_cmd(cmd);
                return 1;
            }
            printf("Checked out %s\n", tag);
        }
    }

    // Add to aether.toml
    FILE* f = fopen("aether.toml", "r");
    if (!f) {
        fprintf(stderr, "Error: Could not read aether.toml\n");
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        fprintf(stderr, "Error: Could not determine file size\n");
        return 1;
    }
    fseek(f, 0, SEEK_SET);
    char* content = malloc((size_t)sz + 1);
    if (!content) {
        fclose(f);
        fprintf(stderr, "Error: Out of memory\n");
        return 1;
    }
    size_t nread = fread(content, 1, (size_t)sz, f);
    content[nread] = '\0';
    fclose(f);

    if (strstr(content, package)) {
        printf("Already in dependencies.\n");
        free(content);
        return 0;
    }

    char* deps = strstr(content, "[dependencies]");
    if (deps) {
        char* next_sect = strchr(deps + 14, '[');
        f = fopen("aether.toml", "w");
        if (!f) {
            fprintf(stderr, "Error: Could not write aether.toml\n");
            free(content);
            return 1;
        }
        if (next_sect) {
            fwrite(content, 1, next_sect - content, f);
            fprintf(f, "%s = \"%s\"\n", package, version ? version : "latest");
            fputs(next_sect, f);
        } else {
            fputs(content, f);
            fprintf(f, "%s = \"%s\"\n", package, version ? version : "latest");
        }
        fclose(f);
    } else {
        // No [dependencies] section — append one
        f = fopen("aether.toml", "a");
        if (!f) {
            fprintf(stderr, "Error: Could not write aether.toml\n");
            free(content);
            return 1;
        }
        fprintf(f, "\n[dependencies]\n");
        fprintf(f, "%s = \"%s\"\n", package, version ? version : "latest");
        fclose(f);
    }

    free(content);
    printf("Added %s to dependencies.\n", package);
    return 0;
}

static int cmd_examples(int argc, char** argv) {
    const char* examples_dir = "examples";
    if (argc > 0 && argv[0][0] != '-') {
        if (!is_safe_path(argv[0])) {
            fprintf(stderr, "Error: Invalid characters in path\n");
            return 1;
        }
        examples_dir = argv[0];
    }

    char files[512][512];
    int file_count = 0;

    char find_cmd[1024];
#ifdef _WIN32
    snprintf(find_cmd, sizeof(find_cmd), "dir /b /s \"%s\\*.ae\" 2>nul", examples_dir);
#else
    snprintf(find_cmd, sizeof(find_cmd), "find \"%s\" -name '*.ae' -type f 2>/dev/null | sort", examples_dir);
#endif
    FILE* pipe = popen(find_cmd, "r");
    if (pipe) {
        char line[512];
        while (fgets(line, sizeof(line), pipe) && file_count < 512) {
            line[strcspn(line, "\n\r")] = '\0';
            if (strlen(line) > 0) {
                strncpy(files[file_count], line, sizeof(files[0]) - 1);
                files[file_count][sizeof(files[0]) - 1] = '\0';
                file_count++;
            }
        }
        pclose(pipe);
    }

    if (file_count == 0) {
        printf("No .ae files found in %s/\n", examples_dir);
        return 0;
    }

    printf("Building %d example(s)...\n\n", file_count);

    mkdirs("build/examples");

    int pass = 0, fail = 0, skipped = 0;

    for (int i = 0; i < file_count; i++) {
        const char* src = files[i];

        // Skip module files (lib/) and project mains (packages/) —
        // these need `ae run` with module orchestration, not bare aetherc.
        if (strstr(src, "/lib/") || strstr(src, "\\lib\\") ||
            strstr(src, "/packages/") || strstr(src, "\\packages\\")) {
            skipped++;
            continue;
        }

        const char* slash = strrchr(src, '/');
        if (!slash) slash = strrchr(src, '\\');
        const char* name = slash ? slash + 1 : src;
        char base[256];
        strncpy(base, name, sizeof(base) - 1);
        base[sizeof(base) - 1] = '\0';
        char* dot = strrchr(base, '.');
        if (dot) *dot = '\0';

        printf("  %-30s ", base);
        fflush(stdout);

        char c_file[2048], exe_file[2048], cmd[16384];
        snprintf(c_file, sizeof(c_file), "build/examples/%s.c", base);
        snprintf(exe_file, sizeof(exe_file), "build/examples/%s" EXE_EXT, base);

        // Find extra .c files in the same directory as the .ae source
        char src_dir[512];
        strncpy(src_dir, src, sizeof(src_dir) - 1);
        src_dir[sizeof(src_dir) - 1] = '\0';
        char* last_sep = strrchr(src_dir, '/');
        if (!last_sep) last_sep = strrchr(src_dir, '\\');
        if (last_sep) *last_sep = '\0';
        else strcpy(src_dir, ".");

        char extra_c[2048] = "";
        char find_c[1024];
#ifdef _WIN32
        snprintf(find_c, sizeof(find_c), "dir /b \"%s\\*.c\" 2>nul", src_dir);
#else
        snprintf(find_c, sizeof(find_c), "find \"%s\" -maxdepth 1 -name '*.c' 2>/dev/null", src_dir);
#endif
        FILE* c_pipe = popen(find_c, "r");
        if (c_pipe) {
            char c_line[512];
            while (fgets(c_line, sizeof(c_line), c_pipe)) {
                c_line[strcspn(c_line, "\n\r")] = '\0';
                if (strlen(c_line) == 0) continue;
                char c_path[512];
#ifdef _WIN32
                snprintf(c_path, sizeof(c_path), "%s\\%s", src_dir, c_line);
#else
                snprintf(c_path, sizeof(c_path), "%s", c_line);
#endif
                if (strlen(extra_c) + strlen(c_path) + 2 < sizeof(extra_c)) {
                    strcat(extra_c, " ");
                    strcat(extra_c, c_path);
                }
            }
            pclose(c_pipe);
        }

        // Step 1: compile .ae -> .c
        // GCC conservatively assumes src (char* from glob) may be PATH_MAX-sized;
        // cmd[8192] is sufficient for real-world paths.
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wformat-truncation"
#endif
        snprintf(cmd, sizeof(cmd), "%s %s %s", tc.compiler, src, c_file);
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
        if (run_cmd_quiet(cmd) != 0) {
            printf("FAIL (compile)\n");
            fail++;
            continue;
        }

        // Step 2: link .c + extra -> exe
        const char* extra = extra_c[0] ? extra_c : NULL;
        build_gcc_cmd(cmd, sizeof(cmd), c_file, exe_file, true, extra);
        if (run_cmd_quiet(cmd) != 0) {
            printf("FAIL (build)\n");
            fail++;
            remove(c_file);
            continue;
        }

        printf("OK\n");
        pass++;
        remove(c_file);
    }

    printf("\n%d passed, %d failed, %d total\n", pass, fail, file_count - skipped);
    printf("Binaries in build/examples/\n");
    return (fail > 0) ? 1 : 0;
}

// REPL session: accumulated lines that persist across evaluations.
// Each entry is a statement (assignment, function def, etc.) that gets
// replayed before the current input so variables/functions stay in scope.
#define REPL_MAX_LINES 256
#define REPL_LINE_LEN  1024

// Compile and run the REPL input. Returns 1 on success, 0 on failure.
static int repl_eval(const char* ae_file, const char* c_file,
                     const char* exe_file, char** history,
                     int history_count, const char* input) {
    FILE* f = fopen(ae_file, "w");
    if (!f) return 0;
    fprintf(f, "main() {\n");
    for (int i = 0; i < history_count; i++)
        fprintf(f, "    %s\n", history[i]);
    const char* rest = input;
    const char* nl;
    while ((nl = strchr(rest, '\n')) != NULL) {
        fprintf(f, "    %.*s\n", (int)(nl - rest), rest);
        rest = nl + 1;
    }
    if (*rest) fprintf(f, "    %s\n", rest);
    fprintf(f, "}\n");
    fclose(f);

    char cmd[16384];
    snprintf(cmd, sizeof(cmd), "\"%s\" \"%s\" \"%s\"", tc.compiler, ae_file, c_file);
    if (run_cmd_quiet(cmd) != 0) {
        run_cmd(cmd);
        remove(c_file);
        return 0;
    }
    build_gcc_cmd(cmd, sizeof(cmd), c_file, exe_file, false, NULL);
    if (run_cmd_quiet(cmd) != 0) {
        build_gcc_cmd(cmd, sizeof(cmd), c_file, exe_file, false, NULL);
        run_cmd(cmd);
        remove(c_file);
        remove(exe_file);
        return 0;
    }
    snprintf(cmd, sizeof(cmd), "\"%s\"", exe_file);
    run_cmd(cmd);
    remove(c_file);
    remove(exe_file);
    return 1;
}

// Persist an assignment or const into session history, replacing
// previous assignments to the same variable name.
static void repl_persist(char** history, int* history_count, const char* input) {
    char* eq = strchr(input, '=');
    int has_assign = (eq && (eq == input ||
        (eq[-1] != '=' && eq[-1] != '!' && eq[-1] != '<' && eq[-1] != '>'))
        && eq[1] != '=');
    int has_const = (strncmp(input, "const ", 6) == 0);
    if (!has_assign && !has_const) return;

    int replaced = 0;
    if (has_assign && eq) {
        int name_len = (int)(eq - input);
        while (name_len > 0 && input[name_len - 1] == ' ') name_len--;
        for (int i = 0; i < *history_count; i++) {
            char* heq = strchr(history[i], '=');
            if (heq) {
                int hlen = (int)(heq - history[i]);
                while (hlen > 0 && history[i][hlen - 1] == ' ') hlen--;
                if (hlen == name_len && strncmp(input, history[i], name_len) == 0) {
                    free(history[i]);
                    history[i] = strdup(input);
                    replaced = 1;
                    break;
                }
            }
        }
    }
    if (!replaced && *history_count < REPL_MAX_LINES)
        history[(*history_count)++] = strdup(input);
}

// Check if a single line is a complete statement (no open braces).
// Single-line statements execute immediately without waiting for blank line.
static int repl_is_complete_line(const char* line) {
    int depth = 0;
    for (const char* p = line; *p; p++) {
        if (*p == '{') depth++;
        else if (*p == '}') depth--;
    }
    return depth == 0;
}

static int cmd_repl(void) {
    printf("\n");
    // Dynamic box: "   Aether X.Y.Z REPL   "
    int ver_len = (int)strlen(AE_VERSION);
    int title_len = 15 + ver_len;  // "   Aether " (10) + ver + " REPL" (5)
    int help_len  = 21;            // "   :help for commands"
    int inner = title_len + 3;     // 3 chars right padding
    if (inner < help_len + 3) inner = help_len + 3;
    printf("  ┌"); for (int i = 0; i < inner; i++) printf("─"); printf("┐\n");
    printf("  │   Aether %s REPL", AE_VERSION);
    for (int i = title_len; i < inner; i++) printf(" "); printf("│\n");
    printf("  │   :help for commands");
    for (int i = help_len; i < inner; i++) printf(" "); printf("│\n");
    printf("  └"); for (int i = 0; i < inner; i++) printf("─"); printf("┘\n");
    printf("\n");

    char* history[REPL_MAX_LINES];
    int history_count = 0;
    char input[16384] = {0};
    char line[REPL_LINE_LEN];
    int brace_depth = 0;

    char ae_file[1024], c_file[1024], exe_file[1024];
    snprintf(ae_file,  sizeof(ae_file),  "%s/_aether_repl_%d.ae",  get_temp_dir(), (int)getpid());
    snprintf(c_file,   sizeof(c_file),   "%s/_aether_repl_%d.c",   get_temp_dir(), (int)getpid());
    snprintf(exe_file, sizeof(exe_file), "%s/_aether_repl_%d" EXE_EXT, get_temp_dir(), (int)getpid());

    while (1) {
        if (brace_depth > 0)
            printf("...  ");
        else if (input[0])
            printf("  .. ");
        else
            printf("  ae> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\r\n")] = '\0';

        // Commands (only at top level, not mid-block)
        if (brace_depth == 0 && !input[0]) {
            if (strcmp(line, ":quit") == 0 || strcmp(line, ":q") == 0 ||
                strcmp(line, "exit") == 0  || strcmp(line, "quit") == 0) break;
            if (strcmp(line, ":help") == 0 || strcmp(line, ":h") == 0) {
                printf("\n");
                printf("  Commands:\n");
                printf("    :help  :h    Show this help\n");
                printf("    :reset :r    Clear session state\n");
                printf("    :show  :s    Show session code\n");
                printf("    :quit  :q    Exit (also: quit, exit)\n");
                printf("\n");
                printf("  Usage:\n");
                printf("    Single lines run immediately:\n");
                printf("      ae> println(\"hello\")\n");
                printf("      hello\n");
                printf("\n");
                printf("    Assignments persist across evaluations:\n");
                printf("      ae> x = 5\n");
                printf("      ae> println(x + 1)\n");
                printf("      6\n");
                printf("\n");
                printf("    Multi-line blocks auto-continue until braces close:\n");
                printf("      ae> if x > 3 {\n");
                printf("      ...   println(\"big\")\n");
                printf("      ... }\n");
                printf("      big\n");
                printf("\n");
                continue;
            }
            if (strcmp(line, ":reset") == 0 || strcmp(line, ":r") == 0) {
                for (int i = 0; i < history_count; i++) free(history[i]);
                history_count = 0;
                printf("  Session reset.\n");
                continue;
            }
            if (strcmp(line, ":show") == 0 || strcmp(line, ":s") == 0) {
                if (history_count == 0) { printf("  (empty session)\n"); continue; }
                printf("\n");
                for (int i = 0; i < history_count; i++)
                    printf("    %s\n", history[i]);
                printf("\n");
                continue;
            }
        }

        // Track brace depth
        int prev_depth = brace_depth;
        for (char* p = line; *p; p++) {
            if (*p == '{') brace_depth++;
            else if (*p == '}' && brace_depth > 0) brace_depth--;
        }
        int is_empty = (strlen(line) == 0);
        int block_closed = (prev_depth > 0 && brace_depth == 0);

        // Accumulate non-empty lines
        if (!is_empty) {
            if (input[0]) strncat(input, "\n", sizeof(input) - strlen(input) - 1);
            strncat(input, line, sizeof(input) - strlen(input) - 1);
        }

        // Decide when to execute:
        // 1. Block just closed (multi-line if/while/for)
        // 2. Empty line with pending input (explicit trigger)
        // 3. Single complete line (no open braces, no prior accumulation)
        int should_run = 0;
        if (block_closed && input[0])
            should_run = 1;
        else if (is_empty && input[0])
            should_run = 1;
        else if (!is_empty && brace_depth == 0 && prev_depth == 0 &&
                 !strchr(input, '\n') && repl_is_complete_line(input))
            should_run = 1;

        if (should_run) {
            if (repl_eval(ae_file, c_file, exe_file, history,
                          history_count, input)) {
                repl_persist(history, &history_count, input);
            }
            input[0] = '\0';
            brace_depth = 0;
        }
    }

    for (int i = 0; i < history_count; i++) free(history[i]);
    remove(ae_file);
    remove(c_file);
    remove(exe_file);
    printf("\n  Goodbye!\n\n");
    return 0;
}

// --------------------------------------------------------------------------
// Version manager: list available releases, install, and switch versions
// --------------------------------------------------------------------------

// Compile-time platform string used to pick the right release archive.
#if defined(_WIN32)
#  if defined(__aarch64__) || defined(_M_ARM64)
#    define AE_PLATFORM "windows-arm64"
#  else
#    define AE_PLATFORM "windows-x86_64"
#  endif
#  define AE_ARCHIVE_EXT ".zip"
#elif defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
#  define AE_PLATFORM "macos-arm64"
#  define AE_ARCHIVE_EXT ".tar.gz"
#elif defined(__APPLE__)
#  define AE_PLATFORM "macos-x86_64"
#  define AE_ARCHIVE_EXT ".tar.gz"
#elif defined(__linux__) && (defined(__aarch64__) || defined(__arm64__))
#  define AE_PLATFORM "linux-arm64"
#  define AE_ARCHIVE_EXT ".tar.gz"
#else
#  define AE_PLATFORM "linux-x86_64"
#  define AE_ARCHIVE_EXT ".tar.gz"
#endif

#define AE_GITHUB_REPO "nicolasmd87/aether"

// Download url → dest file. Uses curl/wget on POSIX, PowerShell on Windows.
// Creates parent directories of dest if they don't exist.
static int ae_download(const char* url, const char* dest) {
    // Ensure parent directory exists (e.g. ~/.aether/ for releases.json)
    {
        char parent[1024];
        strncpy(parent, dest, sizeof(parent) - 1);
        parent[sizeof(parent) - 1] = '\0';
        char* slash = strrchr(parent, '/');
        if (!slash) slash = strrchr(parent, '\\');
        if (slash) { *slash = '\0'; mkdirs(parent); }
    }
#ifdef _WIN32
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "ae_dl_%u.ps1", (unsigned)GetCurrentProcessId());
    char ps_path[1024];
    snprintf(ps_path, sizeof(ps_path), "%s\\%s", get_temp_dir(), tmp);
    FILE* ps = fopen(ps_path, "w");
    if (!ps) return 1;
    fprintf(ps,
        "$ProgressPreference='SilentlyContinue'\n"
        "Invoke-WebRequest -Uri '%s' -OutFile '%s' "
        "-Headers @{'User-Agent'='ae-cli'}\n",
        url, dest);
    fclose(ps);
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
        "powershell -NoProfile -ExecutionPolicy Bypass -File \"%s\" >nul 2>&1", ps_path);
    int r = system(cmd);
    remove(ps_path);
    return r;
#else
    char cmd[2048];
    if (system("curl --version >/dev/null 2>&1") == 0)
        snprintf(cmd, sizeof(cmd), "curl -fsSL -o \"%s\" \"%s\" 2>/dev/null", dest, url);
    else
        snprintf(cmd, sizeof(cmd), "wget -q --no-verbose -O \"%s\" \"%s\" 2>/dev/null", dest, url);
    return system(cmd);
#endif
}

// Extract archive → dest_dir.
static int ae_extract(const char* archive, const char* dest_dir) {
#ifdef _WIN32
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "ae_ex_%u.ps1", (unsigned)GetCurrentProcessId());
    char ps_path[1024];
    snprintf(ps_path, sizeof(ps_path), "%s\\%s", get_temp_dir(), tmp);
    FILE* ps = fopen(ps_path, "w");
    if (!ps) return 1;
    fprintf(ps,
        "$ProgressPreference='SilentlyContinue'\n"
        "Expand-Archive -Path '%s' -DestinationPath '%s' -Force\n",
        archive, dest_dir);
    fclose(ps);
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
        "powershell -NoProfile -ExecutionPolicy Bypass -File \"%s\" >nul 2>&1", ps_path);
    int r = system(cmd);
    remove(ps_path);
    return r;
#else
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "tar -xzf \"%s\" -C \"%s\"", archive, dest_dir);
    return system(cmd);
#endif
}

// List available releases from GitHub. Marks installed + current versions.
static int cmd_version_list(void) {
    const char* home = get_home_dir();

    // Determine which version is actually active.
    // Priority: 1) ~/.aether/active_version file (authoritative — written by install.sh and ae version use)
    //           2) ~/.aether/current symlink (legacy fallback)
    //           3) compiled-in AE_VERSION (fallback)
    char active_ver[64] = "";

    // Check active_version file first (always authoritative)
    {
        char avpath[512];
#ifdef _WIN32
        snprintf(avpath, sizeof(avpath), "%s\\.aether\\active_version", home);
#else
        snprintf(avpath, sizeof(avpath), "%s/.aether/active_version", home);
#endif
        FILE* avf = fopen(avpath, "r");
        if (avf) {
            if (fgets(active_ver, sizeof(active_ver), avf)) {
                char* nl = strchr(active_ver, '\n'); if (nl) *nl = '\0';
                char* cr = strchr(active_ver, '\r'); if (cr) *cr = '\0';
            }
            fclose(avf);
        }
    }

#ifndef _WIN32
    // Legacy fallback: resolve ~/.aether/current symlink
    if (active_ver[0] == '\0') {
        char current_link[512], target[1024];
        snprintf(current_link, sizeof(current_link), "%s/.aether/current", home);
        ssize_t rlen = readlink(current_link, target, sizeof(target) - 1);
        if (rlen > 0) {
            target[rlen] = '\0';
            const char* last = strrchr(target, '/');
            if (last) last++; else last = target;
            if (last[0] == 'v') last++;
            strncpy(active_ver, last, sizeof(active_ver) - 1);
            active_ver[sizeof(active_ver) - 1] = '\0';
        }
    }
#endif

    // Fallback: use compiled-in version
    if (active_ver[0] == '\0') {
        strncpy(active_ver, AE_VERSION, sizeof(active_ver) - 1);
        active_ver[sizeof(active_ver) - 1] = '\0';
    }

    // Fetch the GitHub releases JSON into a temp file
    char json_path[512];
#ifdef _WIN32
    snprintf(json_path, sizeof(json_path), "%s\\.aether\\releases.json", home);
#else
    snprintf(json_path, sizeof(json_path), "%s/ae_releases_%d.json", get_temp_dir(), (int)getpid());
#endif
    char url[256];
    snprintf(url, sizeof(url),
        "https://api.github.com/repos/" AE_GITHUB_REPO "/releases?per_page=20");

    printf("Fetching release list...\n");
    if (ae_download(url, json_path) != 0) {
        fprintf(stderr, "Failed to fetch releases. Check your internet connection.\n");
        return 1;
    }

    FILE* f = fopen(json_path, "r");
    if (!f) {
        fprintf(stderr, "Failed to read release data.\n");
        return 1;
    }

    printf("\nAvailable Aether releases  (platform: " AE_PLATFORM "):\n\n");
    printf("  %-16s  %s\n", "Version", "Status");
    printf("  %-16s  %s\n", "-------", "------");

    // Read whole file, scan for "tag_name" occurrences
    char buf[131072];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    remove(json_path);
    buf[n] = '\0';

    int found = 0;
    char* p = buf;
    while ((p = strstr(p, "\"tag_name\"")) != NULL) {
        p += 10;  // skip past "tag_name"
        char* q = strchr(p, '"'); if (!q) break; q++;   // opening "
        char* end = strchr(q, '"'); if (!end) break;
        size_t len = (size_t)(end - q);
        if (len == 0 || len > 32) { p = end + 1; continue; }

        char tag[33];
        memcpy(tag, q, len);
        tag[len] = '\0';
        p = end + 1;

        // v-prefix normalisation: strip 'v' to compare with active version
        const char* ver = (tag[0] == 'v') ? tag + 1 : tag;
        bool is_current = strcmp(ver, active_ver) == 0;

        // Check locally installed
        char ver_dir[512];
        snprintf(ver_dir, sizeof(ver_dir), "%s/.aether/versions/%s", home, tag);
        bool installed = dir_exists(ver_dir);

        const char* status = is_current ? "* current"
                           : installed  ? "  installed"
                                        : "";
        printf("  %-16s  %s\n", tag, status);
        found++;
    }

    if (!found) {
        printf("  (no releases found)\n");
    }
    printf("\n");
    // Show latest found tag in examples (or fallback)
    if (found > 0) {
        // First tag found is the latest (GitHub returns newest first)
        // Re-scan to get it
        char latest[33] = "v0.1.0";
        char* lp = strstr(buf, "\"tag_name\"");
        if (lp) {
            lp += 10;
            char* lq = strchr(lp, '"'); if (lq) { lq++;
            char* le = strchr(lq, '"'); if (le) {
                size_t ll = (size_t)(le - lq);
                if (ll > 0 && ll < sizeof(latest)) { memcpy(latest, lq, ll); latest[ll] = '\0'; }
            }}
        }
        printf("Install a version:  ae version install %s\n", latest);
        printf("Switch versions:    ae version use %s\n", latest);
    } else {
        printf("Install a version:  ae version install <version>\n");
        printf("Switch versions:    ae version use <version>\n");
    }
    return 0;
}

// Download and install a specific version into ~/.aether/versions/<tag>/
static int cmd_version_install(const char* version) {
    char vtag[64];
    if (version[0] != 'v') snprintf(vtag, sizeof(vtag), "v%s", version);
    else { strncpy(vtag, version, sizeof(vtag) - 1); vtag[sizeof(vtag)-1] = '\0'; }

    const char* ver = vtag + 1;  // strip leading 'v'
    const char* home = get_home_dir();

    char ver_dir[512];
    snprintf(ver_dir, sizeof(ver_dir), "%s/.aether/versions/%s", home, vtag);
    if (dir_exists(ver_dir)) {
        // Verify the install is complete by checking for binaries
        char probe[1024];
        int has_binary = 0;
#ifdef _WIN32
        snprintf(probe, sizeof(probe), "%s\\bin\\aetherc.exe", ver_dir);
        if (path_exists(probe)) has_binary = 1;
        snprintf(probe, sizeof(probe), "%s\\aetherc.exe", ver_dir);
        if (path_exists(probe)) has_binary = 1;
#else
        snprintf(probe, sizeof(probe), "%s/bin/aetherc", ver_dir);
        if (path_exists(probe)) has_binary = 1;
        snprintf(probe, sizeof(probe), "%s/aetherc", ver_dir);
        if (path_exists(probe)) has_binary = 1;
#endif
        if (has_binary) {
            // Also verify the install has sources or a prebuilt lib —
            // old ae versions had an extraction bug that only copied bin/
            char lib_probe[1024], share_probe[1024];
            int has_sources = 0;
            snprintf(lib_probe, sizeof(lib_probe), "%s/lib/libaether.a", ver_dir);
            if (path_exists(lib_probe)) has_sources = 1;
            snprintf(share_probe, sizeof(share_probe), "%s/share/aether/runtime", ver_dir);
            if (dir_exists(share_probe)) has_sources = 1;
            if (has_sources) {
                printf("Version %s is already installed.\n", vtag);
                printf("Switch to it with: ae version use %s\n", vtag);
                return 0;
            }
            printf("Version %s has binaries but missing lib/share — reinstalling...\n", vtag);
            // Fall through to remove and re-download
        }
        // Incomplete install — remove and re-download
        printf("Incomplete installation of %s detected, reinstalling...\n", vtag);
#ifdef _WIN32
        char rm_cmd[1024];
        snprintf(rm_cmd, sizeof(rm_cmd), "rmdir /S /Q \"%s\"", ver_dir);
        if (system(rm_cmd) != 0) {
            fprintf(stderr, "Warning: failed to remove incomplete install at %s\n", ver_dir);
        }
#else
        char rm_cmd[1024];
        snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf '%s'", ver_dir);
        if (system(rm_cmd) != 0) {
            fprintf(stderr, "Warning: failed to remove incomplete install at %s\n", ver_dir);
        }
#endif
    }

    // Build URL and local archive path
    char filename[256], url[1024], archive[512];
    snprintf(filename, sizeof(filename),
        "aether-%s-" AE_PLATFORM AE_ARCHIVE_EXT, ver);
    snprintf(url, sizeof(url),
        "https://github.com/" AE_GITHUB_REPO "/releases/download/%s/%s",
        vtag, filename);
    snprintf(archive, sizeof(archive), "%s/.aether/%s", home, filename);

    printf("Downloading Aether %s for " AE_PLATFORM "...\n", vtag);
    fflush(stdout);
    if (ae_download(url, archive) != 0) {
        fprintf(stderr, "Error: Version %s not found for " AE_PLATFORM ".\n", vtag);
        fprintf(stderr, "Run 'ae version list' to see available versions.\n");
        return 1;
    }

    // Verify the downloaded file is a real archive, not a 404 HTML page.
    // Valid archives are at least 10KB; GitHub 404 pages are ~10-20KB HTML
    // but tar.gz/zip archives for Aether are always >100KB.
    {
        FILE* af = fopen(archive, "rb");
        if (!af) {
            fprintf(stderr, "Error: Downloaded file not found.\n");
            return 1;
        }
        fseek(af, 0, SEEK_END);
        long asize = ftell(af);
        // Also check the first bytes for archive magic
        fseek(af, 0, SEEK_SET);
        unsigned char magic[4] = {0};
        fread(magic, 1, 4, af);
        fclose(af);

        int is_gzip = (magic[0] == 0x1f && magic[1] == 0x8b);  // .tar.gz
        int is_zip  = (magic[0] == 'P' && magic[1] == 'K');     // .zip
        int is_xz   = (magic[0] == 0xFD && magic[1] == '7');    // .tar.xz

        if (!is_gzip && !is_zip && !is_xz) {
            remove(archive);
            fprintf(stderr, "Error: Version %s not found for platform " AE_PLATFORM ".\n", vtag);
            fprintf(stderr, "The download returned an error page, not a release archive.\n");
            fprintf(stderr, "Available versions: ae version list\n");
            return 1;
        }
        if (asize < 1024) {
            remove(archive);
            fprintf(stderr, "Error: Downloaded archive is too small (%ld bytes) — likely corrupt.\n", asize);
            return 1;
        }
    }

    mkdirs(ver_dir);
    printf("Extracting...\n");

    // The release archive contains a top-level directory (e.g. "release/").
    // Extract to a temp dir first, then move the contents into ver_dir.
    char tmp_dir[512];
    snprintf(tmp_dir, sizeof(tmp_dir), "%s/.aether/_tmp_install", home);
    mkdirs(tmp_dir);

    if (ae_extract(archive, tmp_dir) != 0) {
        fprintf(stderr, "Extraction failed.\n");
        remove(archive);
        return 1;
    }
    remove(archive);

    // Move extracted contents into ver_dir.
    // Release archives may have a single wrapper directory (e.g. "aether-v0.21.0-macos-arm64/")
    // OR may have bin/, lib/, share/, include/ directly at root. Handle both cases.
#ifdef _WIN32
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "xcopy /E /Y /Q \"%s\\*\" \"%s\\\"", tmp_dir, ver_dir);
    if (system(cmd) != 0) {
        fprintf(stderr, "Error: Failed to copy installation files.\n");
        return 1;
    }
    snprintf(cmd, sizeof(cmd), "rmdir /S /Q \"%s\"", tmp_dir);
    if (system(cmd) != 0) { /* non-fatal: temp dir cleanup */ }
#else
    {
        char cmd[4096];
        // If there is exactly one top-level entry and it is a directory,
        // treat it as a wrapper and copy its contents. Otherwise copy
        // everything directly (the archive has bin/, lib/, etc. at root).
        snprintf(cmd, sizeof(cmd),
            "entries=$(ls '%s' | wc -l | tr -d ' '); "
            "single=$(ls -d '%s'/*/ 2>/dev/null | wc -l | tr -d ' '); "
            "if [ \"$entries\" = \"1\" ] && [ \"$single\" = \"1\" ]; then "
            "  src=$(ls -d '%s'/*/); cp -r \"$src\"* '%s/'; "
            "else "
            "  cp -r '%s'/* '%s/'; "
            "fi",
            tmp_dir, tmp_dir, tmp_dir, ver_dir, tmp_dir, ver_dir);
        if (system(cmd) != 0) {
            fprintf(stderr, "Error: Failed to copy installation files.\n");
            return 1;
        }
        snprintf(cmd, sizeof(cmd), "rm -rf '%s'", tmp_dir);
        if (system(cmd) != 0) { /* non-fatal: temp dir cleanup */ }
    }
#endif

    // Verify the installation has the expected structure.
    // Releases should have bin/, lib/ or share/aether/ — if we only see
    // flat binaries, the extraction went wrong.
    {
        char probe[1024];
        int has_structure = 0;
        snprintf(probe, sizeof(probe), "%s/bin", ver_dir);
        if (dir_exists(probe)) has_structure = 1;
        snprintf(probe, sizeof(probe), "%s/lib", ver_dir);
        if (dir_exists(probe)) has_structure = 1;
        snprintf(probe, sizeof(probe), "%s/share/aether", ver_dir);
        if (dir_exists(probe)) has_structure = 1;
        if (!has_structure) {
            // Clean up the empty/broken install directory
#ifdef _WIN32
            snprintf(probe, sizeof(probe), "rmdir /S /Q \"%s\"", ver_dir);
#else
            snprintf(probe, sizeof(probe), "rm -rf '%s'", ver_dir);
#endif
            (void)system(probe);
            fprintf(stderr, "Error: Installation of %s failed — no bin/, lib/, or share/ found.\n", vtag);
            fprintf(stderr, "This version may not have a release for " AE_PLATFORM ".\n");
            fprintf(stderr, "Available versions: ae version list\n");
            return 1;
        }
    }

    printf("Installed Aether %s → %s\n", vtag, ver_dir);
    printf("Switch to it with: ae version use %s\n", vtag);
    return 0;
}

// Determine where binaries live inside a version directory.
// Release archives may have a bin/ subdirectory or binaries at root.
static void resolve_version_bin_dir(const char* ver_dir, char* out, size_t outsz) {
    char probe[1024];
#ifdef _WIN32
    snprintf(probe, sizeof(probe), "%s\\bin\\aetherc" EXE_EXT, ver_dir);
    if (path_exists(probe)) { snprintf(out, outsz, "%s\\bin", ver_dir); return; }
    snprintf(probe, sizeof(probe), "%s\\bin\\ae" EXE_EXT, ver_dir);
    if (path_exists(probe)) { snprintf(out, outsz, "%s\\bin", ver_dir); return; }
    snprintf(out, outsz, "%s", ver_dir);
#else
    snprintf(probe, sizeof(probe), "%s/bin/aetherc" EXE_EXT, ver_dir);
    if (path_exists(probe)) { snprintf(out, outsz, "%s/bin", ver_dir); return; }
    snprintf(probe, sizeof(probe), "%s/bin/ae" EXE_EXT, ver_dir);
    if (path_exists(probe)) { snprintf(out, outsz, "%s/bin", ver_dir); return; }
    snprintf(out, outsz, "%s", ver_dir);
#endif
}

static int cmd_version_use(const char* version) {
    char vtag[64];
    if (version[0] != 'v') snprintf(vtag, sizeof(vtag), "v%s", version);
    else { strncpy(vtag, version, sizeof(vtag) - 1); vtag[sizeof(vtag)-1] = '\0'; }

    const char* home = get_home_dir();
    char ver_dir[512];
    snprintf(ver_dir, sizeof(ver_dir), "%s/.aether/versions/%s", home, vtag);

    if (!dir_exists(ver_dir)) {
        fprintf(stderr, "Version %s is not installed.\n", vtag);
        fprintf(stderr, "Install it first: ae version install %s\n", vtag);
        return 1;
    }

    char src_bin[1024];
    resolve_version_bin_dir(ver_dir, src_bin, sizeof(src_bin));

#ifdef _WIN32
    // Backup the currently active version to versions/ before overwriting.
    // This preserves the initial install (e.g., v0.30.0 installed via install.sh
    // lives in ~/.aether/ directly, not in versions/).
    {
        char avpath_bak[512];
        snprintf(avpath_bak, sizeof(avpath_bak), "%s\\.aether\\active_version", home);
        FILE* avf_bak = fopen(avpath_bak, "r");
        if (avf_bak) {
            char cur_ver[64] = "";
            if (fgets(cur_ver, sizeof(cur_ver), avf_bak)) {
                char* nl = strchr(cur_ver, '\n');
                if (nl) *nl = '\0';
            }
            fclose(avf_bak);
            if (cur_ver[0]) {
                char cur_vtag[64];
                if (cur_ver[0] != 'v') snprintf(cur_vtag, sizeof(cur_vtag), "v%s", cur_ver);
                else { strncpy(cur_vtag, cur_ver, sizeof(cur_vtag) - 1); cur_vtag[sizeof(cur_vtag)-1] = '\0'; }
                char cur_ver_dir[512];
                snprintf(cur_ver_dir, sizeof(cur_ver_dir), "%s\\.aether\\versions\\%s", home, cur_vtag);
                if (!dir_exists(cur_ver_dir)) {
                    // Current version not in versions/ — back it up
                    char bak_cmd[2048];
                    char dest_root_bak[512];
                    snprintf(dest_root_bak, sizeof(dest_root_bak), "%s\\.aether", home);
                    mkdirs(cur_ver_dir);
                    snprintf(bak_cmd, sizeof(bak_cmd),
                        "robocopy \"%s\" \"%s\" /E /NFL /NDL /NJH /NJS /IS /IT /XD versions cache >nul 2>&1",
                        dest_root_bak, cur_ver_dir);
                    (void)system(bak_cmd);
                }
            }
        }
    }

    // Copy the entire version directory to ~/.aether/ so lib/, include/,
    // share/ are available alongside bin/.
    char dest_root[512];
    snprintf(dest_root, sizeof(dest_root), "%s\\.aether", home);
    char cmd[2048];
    // robocopy /E copies all subdirectories; /NFL /NDL /NJH /NJS suppress output
    snprintf(cmd, sizeof(cmd),
        "robocopy \"%s\" \"%s\" /E /NFL /NDL /NJH /NJS /IS /IT >nul 2>&1",
        ver_dir, dest_root);
    int rc = system(cmd);
    // robocopy returns 0-7 for success, >=8 for failure
    if (rc >= 8) {
        // Fall back to xcopy
        snprintf(cmd, sizeof(cmd),
            "xcopy /E /Y /Q \"%s\\*\" \"%s\\\"", ver_dir, dest_root);
        if (system(cmd) != 0) {
            fprintf(stderr, "Failed to copy version files from %s to %s\n", ver_dir, dest_root);
            return 1;
        }
    }
#else
    // Backup current version to versions/ before switching (preserves initial install)
    {
        char avpath_bak[512];
        snprintf(avpath_bak, sizeof(avpath_bak), "%s/.aether/active_version", home);
        FILE* avf_bak = fopen(avpath_bak, "r");
        if (avf_bak) {
            char cur_ver[64] = "";
            if (fgets(cur_ver, sizeof(cur_ver), avf_bak)) {
                char* nl = strchr(cur_ver, '\n'); if (nl) *nl = '\0';
            }
            fclose(avf_bak);
            if (cur_ver[0]) {
                char cur_vtag[64];
                if (cur_ver[0] != 'v') snprintf(cur_vtag, sizeof(cur_vtag), "v%s", cur_ver);
                else { strncpy(cur_vtag, cur_ver, sizeof(cur_vtag) - 1); cur_vtag[sizeof(cur_vtag)-1] = '\0'; }
                char cur_ver_dir[512];
                snprintf(cur_ver_dir, sizeof(cur_ver_dir), "%s/.aether/versions/%s", home, cur_vtag);
                if (!dir_exists(cur_ver_dir)) {
                    char bak_cmd[4096];
                    mkdirs(cur_ver_dir);
                    // Copy bin/, lib/, include/, share/ but NOT versions/ or cache/
                    snprintf(bak_cmd, sizeof(bak_cmd),
                        "cp -r \"%s/.aether/bin\" \"%s/\" 2>/dev/null; "
                        "cp -r \"%s/.aether/lib\" \"%s/\" 2>/dev/null; "
                        "cp -r \"%s/.aether/include\" \"%s/\" 2>/dev/null; "
                        "cp -r \"%s/.aether/share\" \"%s/\" 2>/dev/null; true",
                        home, cur_ver_dir, home, cur_ver_dir,
                        home, cur_ver_dir, home, cur_ver_dir);
                    (void)system(bak_cmd);
                }
            }
        }
    }

    // POSIX: update ~/.aether/current symlink
    char current[512];
    snprintf(current, sizeof(current), "%s/.aether/current", home);
    remove(current);
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "ln -sf \"%s\" \"%s\"", ver_dir, current);
    if (system(cmd) != 0) {
        fprintf(stderr, "Failed to create symlink. Try manually:\n");
        fprintf(stderr, "  ln -sf %s %s\n", ver_dir, current);
        return 1;
    }
    char dest_bin[512];
    snprintf(dest_bin, sizeof(dest_bin), "%s/.aether/bin", home);
    snprintf(cmd, sizeof(cmd),
        "mkdir -p \"%s\" && cp -f \"%s\"/* \"%s/\" 2>/dev/null; true",
        dest_bin, src_bin, dest_bin);
    if (system(cmd) != 0) {
        fprintf(stderr, "Error: failed to copy binaries from %s to %s\n", src_bin, dest_bin);
        return 1;
    }
#ifdef __APPLE__
    // Remove macOS quarantine so Gatekeeper doesn't kill unsigned binaries
    snprintf(cmd, sizeof(cmd), "xattr -cr \"%s\"/* 2>/dev/null; true", dest_bin);
    (void)system(cmd);
#endif

    // Sync lib/, include/, and share/ from the version directory to ~/.aether/
    // so that stale files left by a previous install.sh don't shadow the
    // version-managed files.  The 'current' symlink alone is not enough
    // because toolchain discovery may resolve the parent directory first.
    //
    // Bootstrap problem: if the user upgrades from an old ae that lacks this
    // sync code, the old binary won't sync. To handle this, we first try
    // invoking the NEW ae binary (just copied to dest_bin) to do the sync.
    // If the new binary supports --sync-version, it handles everything.
    // If it doesn't (old binary format), the call fails harmlessly and
    // we fall through to the in-process sync below.
    {
        char new_ae[1024];
        snprintf(new_ae, sizeof(new_ae), "%s/ae" EXE_EXT, dest_bin);
        if (path_exists(new_ae)) {
            snprintf(cmd, sizeof(cmd),
                "\"%s\" version --sync-from \"%s\" 2>/dev/null", new_ae, ver_dir);
            int _sync_rc = system(cmd);  // best-effort; in-process sync below handles failure
            (void)_sync_rc;
        }
    }
    {
        char dest[512];
        const char* subdirs[] = {"lib", "include", "share"};
        for (int i = 0; i < 3; i++) {
            char src_sub[1024];
            snprintf(src_sub, sizeof(src_sub), "%s/%s", ver_dir, subdirs[i]);
            if (dir_exists(src_sub)) {
                snprintf(dest, sizeof(dest), "%s/.aether/%s", home, subdirs[i]);
                snprintf(cmd, sizeof(cmd),
                    "rm -rf \"%s\" && cp -r \"%s\" \"%s\"",
                    dest, src_sub, dest);
                if (system(cmd) != 0) {
                    fprintf(stderr, "Warning: failed to sync %s to %s\n", src_sub, dest);
                }
            }
        }
    }
#endif

    // Update active_version file so 'ae version list' shows the correct current
    // even if the symlink approach fails or on Windows.
    {
        char avpath[512];
#ifdef _WIN32
        snprintf(avpath, sizeof(avpath), "%s\\.aether\\active_version", home);
#else
        snprintf(avpath, sizeof(avpath), "%s/.aether/active_version", home);
#endif
        FILE* avf = fopen(avpath, "w");
        if (avf) {
            // Write version without 'v' prefix
            const char* v = (vtag[0] == 'v') ? vtag + 1 : vtag;
            fprintf(avf, "%s\n", v);
            fclose(avf);
        }
    }

    printf("Switched to Aether %s.\n", vtag);
    return 0;
}

// "ae version [list|install|use]"
// Read the active version from ~/.aether/active_version, fall back to compiled-in
static const char* get_active_version(void) {
    static char active[64];
    const char* home = get_home_dir();
    char avpath[512];
#ifdef _WIN32
    snprintf(avpath, sizeof(avpath), "%s\\.aether\\active_version", home);
#else
    snprintf(avpath, sizeof(avpath), "%s/.aether/active_version", home);
#endif
    FILE* f = fopen(avpath, "r");
    if (f) {
        if (fgets(active, sizeof(active), f)) {
            // Trim newline
            char* nl = strchr(active, '\n');
            if (nl) *nl = '\0';
            fclose(f);
            if (active[0]) return active;
        }
        fclose(f);
    }
    return AE_VERSION;
}

static int cmd_version(int argc, char** argv) {
    if (argc == 0) {
        printf("ae %s (Aether Language)\n", get_active_version());
        printf("Platform: " AE_PLATFORM "\n");
        printf("\nSubcommands:\n");
        printf("  ae version list              List all available releases\n");
        printf("  ae version install <v>       Download and install a release\n");
        printf("  ae version use <v>           Switch to an installed release\n");
        return 0;
    }
    const char* sub = argv[0];
    if (strcmp(sub, "list") == 0)    return cmd_version_list();
    if (strcmp(sub, "install") == 0) {
        if (argc < 2) { fprintf(stderr, "Usage: ae version install <v>\n"); return 1; }
        return cmd_version_install(argv[1]);
    }
    if (strcmp(sub, "use") == 0) {
        if (argc < 2) { fprintf(stderr, "Usage: ae version use <v>\n"); return 1; }
        return cmd_version_use(argv[1]);
    }
    // Internal: called by old ae binaries after copying new ae to ~/.aether/bin/
    // Syncs lib/, include/, share/ from a version directory to ~/.aether/
    if (strcmp(sub, "--sync-from") == 0) {
        if (argc < 2) return 1;
        const char* ver_dir = argv[1];
        const char* h = get_home_dir();
        const char* subdirs[] = {"lib", "include", "share"};
        for (int i = 0; i < 3; i++) {
            char src_sub[1024], dest[512], cmd[4096];
            snprintf(src_sub, sizeof(src_sub), "%s/%s", ver_dir, subdirs[i]);
            if (dir_exists(src_sub)) {
                snprintf(dest, sizeof(dest), "%s/.aether/%s", h, subdirs[i]);
                snprintf(cmd, sizeof(cmd),
                    "rm -rf \"%s\" && cp -r \"%s\" \"%s\"", dest, src_sub, dest);
                if (system(cmd) != 0) {
                    fprintf(stderr, "Warning: failed to sync %s to %s\n", src_sub, dest);
                }
            }
        }
        return 0;
    }
    // Fall-through: treat unknown sub as "ae version" (backward compat)
    printf("ae %s (Aether Language)\n", AE_VERSION);
    return 0;
}


// --------------------------------------------------------------------------
// Cache management command
// --------------------------------------------------------------------------

static int cmd_cache(int argc, char** argv) {
    const char* sub = argc > 0 ? argv[0] : "info";

    const char* home = get_home_dir();
    char cache_path[512];
    snprintf(cache_path, sizeof(cache_path), "%s/.aether/cache", home);

    if (strcmp(sub, "clear") == 0) {
#ifdef _WIN32
        char pattern[600];
        snprintf(pattern, sizeof(pattern), "%s\\*", cache_path);
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(pattern, &fd);
        if (h == INVALID_HANDLE_VALUE) {
            printf("Cache is empty (no cache directory).\n");
            return 0;
        }
        int count = 0;
        do {
            if (fd.cFileName[0] == '.') continue;
            char full[1024];
            snprintf(full, sizeof(full), "%s\\%s", cache_path, fd.cFileName);
            remove(full);
            count++;
        } while (FindNextFileA(h, &fd));
        FindClose(h);
#else
        DIR* d = opendir(cache_path);
        if (!d) {
            printf("Cache is empty (no cache directory).\n");
            return 0;
        }
        int count = 0;
        struct dirent* entry;
        while ((entry = readdir(d)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            char full[1024];
            snprintf(full, sizeof(full), "%s/%s", cache_path, entry->d_name);
            remove(full);
            count++;
        }
        closedir(d);
#endif
        printf("Cleared %d cached build(s) from %s\n", count, cache_path);
        return 0;
    }

    // Default: show cache info
#ifdef _WIN32
    {
        char pattern[600];
        snprintf(pattern, sizeof(pattern), "%s\\*", cache_path);
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(pattern, &fd);
        if (h == INVALID_HANDLE_VALUE) {
            printf("Cache: empty\nLocation: %s\n", cache_path);
            return 0;
        }
        int count = 0;
        long long total_bytes = 0;
        do {
            if (fd.cFileName[0] == '.') continue;
            char full[1024];
            snprintf(full, sizeof(full), "%s\\%s", cache_path, fd.cFileName);
            struct stat st;
            if (stat(full, &st) == 0) { total_bytes += st.st_size; count++; }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
        printf("Cache: %d build(s), %.1f MB\nLocation: %s\n",
               count, (double)total_bytes / (1024.0 * 1024.0), cache_path);
    }
#else
    {
        DIR* d = opendir(cache_path);
        if (!d) {
            printf("Cache: empty\nLocation: %s\n", cache_path);
            return 0;
        }
        int count = 0;
        long long total_bytes = 0;
        struct dirent* entry;
        while ((entry = readdir(d)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            char full[1024];
            snprintf(full, sizeof(full), "%s/%s", cache_path, entry->d_name);
            struct stat st;
            if (stat(full, &st) == 0) { total_bytes += st.st_size; count++; }
        }
        closedir(d);
        printf("Cache: %d build(s), %.1f MB\nLocation: %s\n",
               count, (double)total_bytes / (1024.0 * 1024.0), cache_path);
    }
#endif
    printf("Use 'ae cache clear' to free space.\n");
    return 0;
}

// --------------------------------------------------------------------------
// Help and main
// --------------------------------------------------------------------------

static void print_usage(void) {
    printf("Aether %s - Actor-based systems programming language\n\n", AE_VERSION);
    printf("Usage:\n");
    printf("  ae <command> [arguments]\n\n");
    printf("Commands:\n");
    printf("  init <name>          Create a new Aether project\n");
    printf("  run [file.ae]        Compile and run a program\n");
    printf("  build [file.ae]      Compile to executable\n");
    printf("  build --target wasm  Compile to WebAssembly (.js + .wasm)\n");
    printf("  check [file.ae]      Type-check without compiling\n");
    printf("  test [file|dir]      Discover and run tests\n");
    printf("  add <package>        Add a dependency\n");
    printf("  cache [clear]        Show or clear build cache\n");
    printf("  examples             List and run example programs\n");
    printf("  repl                 Start interactive REPL\n");
    printf("  version              Show version / manage installed versions\n");
    printf("  version list         List all available releases\n");
    printf("  version install <v>  Download and install a specific version\n");
    printf("  version use <v>      Switch to an installed version\n");
    printf("  help                 Show this help\n");
    printf("\nExamples:\n");
    printf("  ae init myproject          Create a new project\n");
    printf("  ae run hello.ae            Run a single file\n");
    printf("  ae run                     Run project (uses aether.toml)\n");
    printf("  ae build app.ae -o myapp   Build an executable\n");
    printf("  ae test                    Run all tests in tests/\n");
    printf("  ae add github.com/u/pkg    Add a dependency\n");
    printf("\nOptions:\n");
    printf("  -v, --verbose        Show detailed output\n");
    printf("\nEnvironment:\n");
    printf("  AETHER_HOME          Aether installation directory\n");
}

int main(int argc, char** argv) {
#ifdef _WIN32
    // Set UTF-8 console codepage so Aether programs can print Unicode correctly
    // on Windows CMD and PowerShell (default CP1252/OEM is not UTF-8).
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    if (argc < 2) {
        print_usage();
        return 1;
    }

    // Parse global flags before command
    int cmd_idx = 1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            tc.verbose = true;
        } else {
            cmd_idx = i;
            break;
        }
    }

    const char* cmd = argv[cmd_idx];
    int sub_argc = argc - cmd_idx - 1;
    char** sub_argv = argv + cmd_idx + 1;

    // Parse verbose flag after command too
    for (int i = 0; i < sub_argc; i++) {
        if (strcmp(sub_argv[i], "-v") == 0 || strcmp(sub_argv[i], "--verbose") == 0) {
            tc.verbose = true;
        }
    }

    // Commands that don't need toolchain
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
        print_usage();
        return 0;
    }
    if (strcmp(cmd, "version") == 0 || strcmp(cmd, "--version") == 0) {
        return cmd_version(sub_argc, sub_argv);
    }
    if (strcmp(cmd, "init") == 0) {
        return cmd_init(sub_argc, sub_argv);
    }
    if (strcmp(cmd, "fmt") == 0) {
        printf("Formatter not yet implemented.\n");
        return 0;
    }
    // All other commands need the toolchain
    discover_toolchain();

    if (strcmp(cmd, "run") == 0)      return cmd_run(sub_argc, sub_argv);
    if (strcmp(cmd, "build") == 0)    return cmd_build(sub_argc, sub_argv);
    if (strcmp(cmd, "check") == 0)    return cmd_check(sub_argc, sub_argv);
    if (strcmp(cmd, "test") == 0)     return cmd_test(sub_argc, sub_argv);
    if (strcmp(cmd, "examples") == 0) return cmd_examples(sub_argc, sub_argv);
    if (strcmp(cmd, "add") == 0)      return cmd_add(sub_argc, sub_argv);
    if (strcmp(cmd, "cache") == 0)    return cmd_cache(sub_argc, sub_argv);
    if (strcmp(cmd, "repl") == 0)     return cmd_repl();

    fprintf(stderr, "Unknown command '%s'. Run 'ae help' for usage.\n", cmd);
    return 1;
}
