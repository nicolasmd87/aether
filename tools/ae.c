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
#include <sys/stat.h>


#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define PATH_SEP "\\"
#define EXE_EXT ".exe"
#define mkdir_p(path) _mkdir(path)
#else
#include <unistd.h>
#include <sys/wait.h>
#include <libgen.h>
#include <dirent.h>
#define PATH_SEP "/"
#define EXE_EXT ""
#define mkdir_p(path) mkdir(path, 0755)
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#include "apkg/toml_parser.h"

#define AE_VERSION "1.0.0"

// --------------------------------------------------------------------------
// Toolchain state
// --------------------------------------------------------------------------

typedef struct {
    char root[1024];           // Aether root directory
    char compiler[1024];       // Path to aetherc
    char lib[1024];            // Path to libaether.a (if exists)
    char include_flags[2048];  // -I flags for GCC
    char runtime_srcs[4096];   // Runtime .c files (source fallback)
    bool has_lib;              // Whether precompiled lib exists
    bool dev_mode;             // Running from source tree
    bool verbose;              // Verbose output
} Toolchain;

static Toolchain tc = {0};

// --------------------------------------------------------------------------
// Utility functions
// --------------------------------------------------------------------------

static int run_cmd(const char* cmd) {
    if (tc.verbose) fprintf(stderr, "[cmd] %s\n", cmd);
    return system(cmd);
}

static bool path_exists(const char* path) {
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path);
    return (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY));
#else
    return access(path, F_OK) == 0;
#endif
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
            *p = '\0';
            mkdir_p(tmp);
            *p = '/';
        }
    }
    mkdir_p(tmp);
}

static char* get_basename(const char* path) {
    const char* base = strrchr(path, '/');
    if (!base) base = strrchr(path, '\\');
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
        char resolved[1024];
        if (realpath(buf, resolved)) {
            char* slash = strrchr(resolved, '/');
            if (slash) { *slash = '\0'; strncpy(buf, resolved, size); return true; }
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
    if (len > 0) {
        char* slash = strrchr(buf, '\\');
        if (slash) { *slash = '\0'; return true; }
    }
#endif
    return false;
}

// --------------------------------------------------------------------------
// Toolchain discovery
// --------------------------------------------------------------------------

static void discover_toolchain(void) {
    char exe_dir[1024] = {0};
    bool found_exe_dir = get_exe_dir(exe_dir, sizeof(exe_dir));

    // Strategy 1: $AETHER_HOME
    const char* home = getenv("AETHER_HOME");
    // Strip trailing \r or whitespace (shell config with CRLF line endings)
    static char home_clean[1024];
    if (home) {
        strncpy(home_clean, home, sizeof(home_clean) - 1);
        home_clean[sizeof(home_clean) - 1] = '\0';
        size_t len = strlen(home_clean);
        while (len > 0 && (home_clean[len-1] == '\r' || home_clean[len-1] == '\n' || home_clean[len-1] == ' '))
            home_clean[--len] = '\0';
        home = home_clean;
    }
    if (home && dir_exists(home)) {
        strncpy(tc.root, home, sizeof(tc.root) - 1);
        snprintf(tc.compiler, sizeof(tc.compiler), "%s/bin/aetherc" EXE_EXT, tc.root);
        if (tc.verbose) fprintf(stderr, "[toolchain] compiler=%s exists=%d\n", tc.compiler, path_exists(tc.compiler));
        if (path_exists(tc.compiler)) goto found_root;
    }

    // Strategy 2: Relative to ae binary
    if (found_exe_dir) {
        // Case A: installed — ae in $PREFIX/bin/, lib in $PREFIX/lib/aether/
        char candidate[1024];
        snprintf(candidate, sizeof(candidate), "%s/../lib/aether", exe_dir);
        if (dir_exists(candidate)) {
            snprintf(tc.root, sizeof(tc.root), "%s/..", exe_dir);
            snprintf(tc.compiler, sizeof(tc.compiler), "%s/aetherc" EXE_EXT, exe_dir);
            if (path_exists(tc.compiler)) goto found_root;
        }

        // Case B: dev mode — ae in build/, aetherc in build/
        snprintf(candidate, sizeof(candidate), "%s/aetherc" EXE_EXT, exe_dir);
        if (path_exists(candidate)) {
            snprintf(tc.root, sizeof(tc.root), "%s/..", exe_dir);
            strncpy(tc.compiler, candidate, sizeof(tc.compiler) - 1);
            tc.dev_mode = true;
            goto found_root;
        }
    }

    // Strategy 3: CWD dev mode — ./build/aetherc
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

    // Strategy 4: Standard install paths
    const char* standard_paths[] = {
        "/usr/local/bin/aetherc",
        "/usr/bin/aetherc",
        NULL
    };
    for (int i = 0; standard_paths[i]; i++) {
        if (path_exists(standard_paths[i])) {
            strcpy(tc.compiler, standard_paths[i]);
            strncpy(tc.root, standard_paths[i], sizeof(tc.root) - 1);
            char* slash = strrchr(tc.root, '/');
            if (slash) *slash = '\0';
            slash = strrchr(tc.root, '/');
            if (slash) *slash = '\0';
            goto found_root;
        }
    }

    fprintf(stderr, "Error: Aether compiler not found.\n");
    fprintf(stderr, "Run 'make compiler' to build it, or set $AETHER_HOME.\n");
    exit(1);

found_root:
    if (tc.verbose) {
        fprintf(stderr, "[toolchain] root: %s\n", tc.root);
        fprintf(stderr, "[toolchain] compiler: %s\n", tc.compiler);
        fprintf(stderr, "[toolchain] dev_mode: %s\n", tc.dev_mode ? "yes" : "no");
    }

    // Check for precompiled library
    if (tc.dev_mode) {
        snprintf(tc.lib, sizeof(tc.lib), "%s/build/libaether.a", tc.root);
    } else {
        snprintf(tc.lib, sizeof(tc.lib), "%s/lib/libaether.a", tc.root);
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
            "-I%s/std/net -I%s/std/collections -I%s/std/json",
            tc.root, tc.root, tc.root, tc.root, tc.root, tc.root,
            tc.root, tc.root, tc.root, tc.root, tc.root, tc.root, tc.root);

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
                "%s/std/fs/aether_fs.c "
                "%s/std/log/aether_log.c "
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
                tc.root, tc.root, tc.root);
        }
    } else {
        snprintf(tc.include_flags, sizeof(tc.include_flags),
            "-I%s/include/aether/runtime -I%s/include/aether/runtime/actors "
            "-I%s/include/aether/runtime/scheduler -I%s/include/aether/runtime/utils "
            "-I%s/include/aether/runtime/memory -I%s/include/aether/runtime/config "
            "-I%s/include/aether/std -I%s/include/aether/std/string "
            "-I%s/include/aether/std/io -I%s/include/aether/std/math "
            "-I%s/include/aether/std/net -I%s/include/aether/std/collections "
            "-I%s/include/aether/std/json",
            tc.root, tc.root, tc.root, tc.root, tc.root, tc.root,
            tc.root, tc.root, tc.root, tc.root, tc.root, tc.root, tc.root);
    }
}

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

// Build GCC command for linking an Aether-compiled C file
static void build_gcc_cmd(char* cmd, size_t size,
                          const char* c_file, const char* out_file,
                          bool optimize) {
    const char* opt = optimize ? "-O2" : "-O0 -g";
    const char* link_flags = get_link_flags();

    if (tc.has_lib) {
        char lib_dir[1024];
        strncpy(lib_dir, tc.lib, sizeof(lib_dir) - 1);
        lib_dir[sizeof(lib_dir) - 1] = '\0';
        char* slash = strrchr(lib_dir, '/');
        if (slash) *slash = '\0';

        snprintf(cmd, size,
            "gcc %s %s %s -L%s -laether -o %s -pthread -lm %s",
            opt, tc.include_flags, c_file, lib_dir, out_file, link_flags);
    } else {
        snprintf(cmd, size,
            "gcc %s %s %s %s -o %s -pthread -lm %s",
            opt, tc.include_flags, c_file, tc.runtime_srcs, out_file, link_flags);
    }
}

// --------------------------------------------------------------------------
// Commands
// --------------------------------------------------------------------------

static int cmd_run(int argc, char** argv) {
    const char* file = NULL;

    for (int i = 0; i < argc; i++) {
        if (argv[i][0] != '-') { file = argv[i]; break; }
    }

    // Project mode: no file argument, look for aether.toml
    if (!file && path_exists("aether.toml")) {
        if (path_exists("src/main.ae")) file = "src/main.ae";
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

    char c_file[1024], exe_file[1024], cmd[8192];

    if (tc.dev_mode) {
        snprintf(c_file, sizeof(c_file), "%s/build/_ae_tmp.c", tc.root);
        snprintf(exe_file, sizeof(exe_file), "%s/build/_ae_tmp" EXE_EXT, tc.root);
    } else {
        snprintf(c_file, sizeof(c_file), "/tmp/_ae_tmp.c");
        snprintf(exe_file, sizeof(exe_file), "/tmp/_ae_tmp" EXE_EXT);
    }

    // Step 1: Compile .ae to .c
    if (tc.verbose) printf("Compiling %s...\n", file);
    snprintf(cmd, sizeof(cmd), "%s %s %s %s",
        tc.compiler, file, c_file,
        tc.verbose ? "" : ">/dev/null 2>&1");
    if (run_cmd(cmd) != 0) {
        // Re-run with output visible so user can see the error
        snprintf(cmd, sizeof(cmd), "%s %s %s 2>&1", tc.compiler, file, c_file);
        run_cmd(cmd);
        fprintf(stderr, "Compilation failed.\n");
        return 1;
    }

    // Step 2: Compile .c to executable with runtime
    build_gcc_cmd(cmd, sizeof(cmd), c_file, exe_file, true);
    if (!tc.verbose) {
        strncat(cmd, " >/dev/null 2>&1", sizeof(cmd) - strlen(cmd) - 1);
    }
    if (run_cmd(cmd) != 0) {
        // Re-run with output for error diagnosis
        build_gcc_cmd(cmd, sizeof(cmd), c_file, exe_file, true);
        run_cmd(cmd);
        fprintf(stderr, "Build failed.\n");
        return 1;
    }

    // Step 3: Run
    snprintf(cmd, sizeof(cmd), "%s", exe_file);
    int rc = run_cmd(cmd);

    // Clean up
    remove(c_file);
    remove(exe_file);

    return rc;
}

static int cmd_build(int argc, char** argv) {
    const char* file = NULL;
    const char* output_name = NULL;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_name = argv[++i];
        } else if (argv[i][0] != '-') {
            file = argv[i];
        }
    }

    // Project mode
    if (!file && path_exists("aether.toml")) {
        if (path_exists("src/main.ae")) file = "src/main.ae";
    }

    if (!file) {
        fprintf(stderr, "Error: No input file specified.\n");
        fprintf(stderr, "Usage: ae build <file.ae> [-o output]\n");
        return 1;
    }

    if (!path_exists(file)) {
        fprintf(stderr, "Error: File not found: %s\n", file);
        return 1;
    }

    const char* base = get_basename(file);
    char c_file[1024], exe_file[1024], cmd[8192];

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

    printf("Building %s...\n", file);

    // Step 1: .ae to .c
    snprintf(cmd, sizeof(cmd), "%s %s %s %s",
        tc.compiler, file, c_file,
        tc.verbose ? "" : ">/dev/null 2>&1");
    if (run_cmd(cmd) != 0) {
        snprintf(cmd, sizeof(cmd), "%s %s %s 2>&1", tc.compiler, file, c_file);
        run_cmd(cmd);
        fprintf(stderr, "Compilation failed.\n");
        return 1;
    }

    // Step 2: .c to executable with runtime
    build_gcc_cmd(cmd, sizeof(cmd), c_file, exe_file, true);
    if (!tc.verbose) {
        strncat(cmd, " >/dev/null 2>&1", sizeof(cmd) - strlen(cmd) - 1);
    }
    if (run_cmd(cmd) != 0) {
        build_gcc_cmd(cmd, sizeof(cmd), c_file, exe_file, true);
        run_cmd(cmd);
        fprintf(stderr, "Build failed.\n");
        return 1;
    }

    printf("Built: %s\n", exe_file);
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
        if (argv[i][0] != '-') { target = argv[i]; break; }
    }

    // Collect test files
    char test_files[256][512];
    int test_count = 0;

    if (target && path_exists(target) && !dir_exists(target)) {
        // Single file
        strncpy(test_files[0], target, sizeof(test_files[0]) - 1);
        test_count = 1;
    } else {
        // Discover from directory
        const char* test_dir = "tests";
        if (target && dir_exists(target)) test_dir = target;

        if (!dir_exists(test_dir)) {
            printf("No tests/ directory found.\n");
            printf("Create tests in tests/ or run: ae test <file.ae>\n");
            return 0;
        }

        char find_cmd[1024];
#ifdef _WIN32
        snprintf(find_cmd, sizeof(find_cmd), "dir /b /s \"%s\\*.ae\" 2>nul", test_dir);
#else
        snprintf(find_cmd, sizeof(find_cmd), "find %s -name '*.ae' -type f 2>/dev/null | sort", test_dir);
#endif
        FILE* pipe = popen(find_cmd, "r");
        if (pipe) {
            char line[512];
            while (fgets(line, sizeof(line), pipe) && test_count < 256) {
                line[strcspn(line, "\n")] = '\0';
                if (strlen(line) > 0) {
                    strncpy(test_files[test_count], line, sizeof(test_files[0]) - 1);
                    test_count++;
                }
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

        char c_file[1024], exe_file[1024], cmd[8192];

        if (tc.dev_mode) {
            snprintf(c_file, sizeof(c_file), "%s/build/_test_%d.c", tc.root, i);
            snprintf(exe_file, sizeof(exe_file), "%s/build/_test_%d" EXE_EXT, tc.root, i);
        } else {
            snprintf(c_file, sizeof(c_file), "/tmp/_ae_test_%d.c", i);
            snprintf(exe_file, sizeof(exe_file), "/tmp/_ae_test_%d" EXE_EXT, i);
        }

        // Compile .ae to .c
        snprintf(cmd, sizeof(cmd), "%s %s %s >/dev/null 2>&1", tc.compiler, test, c_file);
        if (run_cmd(cmd) != 0) {
            printf("FAIL (compile)\n");
            failed++;
            continue;
        }

        // Compile .c to executable
        build_gcc_cmd(cmd, sizeof(cmd), c_file, exe_file, false);
        char full_cmd[8192];
        snprintf(full_cmd, sizeof(full_cmd), "%s >/dev/null 2>&1", cmd);
        if (run_cmd(full_cmd) != 0) {
            printf("FAIL (build)\n");
            failed++;
            remove(c_file);
            continue;
        }

        // Run
        snprintf(cmd, sizeof(cmd), "%s >/dev/null 2>&1", exe_file);
        int rc = run_cmd(cmd);
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
        fprintf(stderr, "Usage: ae add <package>\n");
        fprintf(stderr, "Example: ae add github.com/user/repo\n");
        return 1;
    }

    const char* package = argv[0];

    if (!path_exists("aether.toml")) {
        fprintf(stderr, "Error: No aether.toml found. Run 'ae init <name>' first.\n");
        return 1;
    }

    if (strncmp(package, "github.com/", 11) != 0) {
        fprintf(stderr, "Error: Only GitHub packages supported.\n");
        fprintf(stderr, "Format: ae add github.com/user/repo\n");
        return 1;
    }

    printf("Adding %s...\n", package);

    // Cache directory
    char cache_dir[1024];
#ifdef _WIN32
    const char* user_home = getenv("USERPROFILE");
    snprintf(cache_dir, sizeof(cache_dir), "%s\\.aether\\packages", user_home ? user_home : ".");
#else
    const char* user_home = getenv("HOME");
    snprintf(cache_dir, sizeof(cache_dir), "%s/.aether/packages", user_home ? user_home : ".");
#endif

    char pkg_dir[1024];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s/%s", cache_dir, package);

    if (!dir_exists(pkg_dir)) {
        printf("Downloading...\n");
        char parent[1024];
        strncpy(parent, pkg_dir, sizeof(parent) - 1);
        parent[sizeof(parent) - 1] = '\0';
        char* slash = strrchr(parent, '/');
        if (slash) { *slash = '\0'; mkdirs(parent); }

        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "git clone --depth 1 https://%s \"%s\" 2>&1", package, pkg_dir);
        if (run_cmd(cmd) != 0) {
            fprintf(stderr, "Failed to download package.\n");
            return 1;
        }
    }

    // Add to aether.toml
    FILE* f = fopen("aether.toml", "r");
    if (!f) return 1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* content = malloc(sz + 1);
    fread(content, 1, sz, f);
    content[sz] = '\0';
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
        if (next_sect) {
            fwrite(content, 1, next_sect - content, f);
            fprintf(f, "%s = \"latest\"\n", package);
            fputs(next_sect, f);
        } else {
            fputs(content, f);
            fprintf(f, "%s = \"latest\"\n", package);
        }
        fclose(f);
    }

    free(content);
    printf("Added %s to dependencies.\n", package);
    return 0;
}

static int cmd_repl(void) {
    char repl_path[1024];

    if (tc.dev_mode) {
        snprintf(repl_path, sizeof(repl_path), "%s/build/aether_repl" EXE_EXT, tc.root);
    } else {
        snprintf(repl_path, sizeof(repl_path), "%s/bin/aether_repl" EXE_EXT, tc.root);
    }

    // Set env vars so the REPL can find compiler and link flags
    setenv("AETHERC", tc.compiler, 1);
    setenv("AETHER_CFLAGS", tc.include_flags, 1);

    char ldflags[2048];
    if (tc.has_lib) {
        char lib_dir[1024];
        strncpy(lib_dir, tc.lib, sizeof(lib_dir) - 1);
        lib_dir[sizeof(lib_dir) - 1] = '\0';
        char* slash = strrchr(lib_dir, '/');
        if (slash) *slash = '\0';
        snprintf(ldflags, sizeof(ldflags), "-L%s -laether", lib_dir);
    } else {
        strncpy(ldflags, tc.runtime_srcs, sizeof(ldflags) - 1);
        ldflags[sizeof(ldflags) - 1] = '\0';
    }
    setenv("AETHER_LDFLAGS", ldflags, 1);

    if (path_exists(repl_path)) {
        return run_cmd(repl_path);
    }

    // Try building it in dev mode
    if (tc.dev_mode) {
        printf("Building REPL...\n");
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "make -C %s repl 2>&1", tc.root);
        if (run_cmd(cmd) == 0 && path_exists(repl_path)) {
            return run_cmd(repl_path);
        }
    }

    fprintf(stderr, "Error: REPL binary not found at %s\n", repl_path);
    fprintf(stderr, "Build it with: make repl\n");
    return 1;
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
    printf("  test [file|dir]      Discover and run tests\n");
    printf("  add <package>        Add a dependency\n");
    printf("  repl                 Start interactive REPL\n");
    printf("  fmt [file]           Format source code\n");
    printf("  version              Show version\n");
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
        printf("ae %s (Aether Language)\n", AE_VERSION);
        return 0;
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

    if (strcmp(cmd, "run") == 0)     return cmd_run(sub_argc, sub_argv);
    if (strcmp(cmd, "build") == 0)   return cmd_build(sub_argc, sub_argv);
    if (strcmp(cmd, "test") == 0)    return cmd_test(sub_argc, sub_argv);
    if (strcmp(cmd, "add") == 0)     return cmd_add(sub_argc, sub_argv);
    if (strcmp(cmd, "repl") == 0)    return cmd_repl();

    fprintf(stderr, "Unknown command '%s'. Run 'ae help' for usage.\n", cmd);
    return 1;
}
