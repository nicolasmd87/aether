#include "apkg.h"
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#endif

#define APKG_VERSION "0.1.0"

int apkg_init(const char* name) {
    printf("Initializing new Aether package '%s'...\n", name);
    
    FILE* f = fopen("aether.toml", "w");
    if (!f) {
        fprintf(stderr, "Error: Could not create aether.toml\n");
        return 1;
    }
    
    fprintf(f, "[package]\n");
    fprintf(f, "name = \"%s\"\n", name);
    fprintf(f, "version = \"0.1.0\"\n");
    fprintf(f, "authors = [\"Your Name <email@example.com>\"]\n");
    fprintf(f, "license = \"MIT\"\n");
    fprintf(f, "description = \"A new Aether project\"\n");
    fprintf(f, "\n");
    fprintf(f, "[dependencies]\n");
    fprintf(f, "# Add dependencies here\n");
    fprintf(f, "\n");
    fprintf(f, "[dev-dependencies]\n");
    fprintf(f, "# Add dev dependencies here\n");
    fprintf(f, "\n");
    fprintf(f, "[build]\n");
    fprintf(f, "target = \"native\"\n");
    fprintf(f, "optimizations = \"aggressive\"\n");
    fprintf(f, "\n");
    fprintf(f, "[[bin]]\n");
    fprintf(f, "name = \"%s\"\n", name);
    fprintf(f, "path = \"src/main.ae\"\n");
    
    fclose(f);
    
    #ifdef _WIN32
        _mkdir("src");
    #else
        mkdir("src", 0755);
    #endif
    
    FILE* main_file = fopen("src/main.ae", "w");
    if (main_file) {
        fprintf(main_file, "main() {\n");
        fprintf(main_file, "    print(\"Hello from %s!\")\n", name);
        fprintf(main_file, "}\n");
        fclose(main_file);
    }
    
    FILE* readme = fopen("README.md", "w");
    if (readme) {
        fprintf(readme, "# %s\n\n", name);
        fprintf(readme, "A new Aether project.\n\n");
        fprintf(readme, "## Building\n\n");
        fprintf(readme, "```bash\n");
        fprintf(readme, "apkg build\n");
        fprintf(readme, "```\n\n");
        fprintf(readme, "## Running\n\n");
        fprintf(readme, "```bash\n");
        fprintf(readme, "apkg run\n");
        fprintf(readme, "```\n");
        fclose(readme);
    }
    
    FILE* gitignore = fopen(".gitignore", "w");
    if (gitignore) {
        fprintf(gitignore, "target/\n");
        fprintf(gitignore, "*.c\n");
        fprintf(gitignore, "*.o\n");
        fprintf(gitignore, "*.exe\n");
        fprintf(gitignore, "aether.lock\n");
        fclose(gitignore);
    }
    
    printf("✓ Created package '%s'\n", name);
    printf("  - aether.toml\n");
    printf("  - src/main.ae\n");
    printf("  - README.md\n");
    printf("  - .gitignore\n");
    printf("\nNext steps:\n");
    printf("  cd %s\n", name);
    printf("  apkg build\n");
    printf("  apkg run\n");
    
    return 0;
}

int apkg_install(const char* package) {
    printf("Installing package '%s'...\n", package);
    
    FILE* manifest = fopen("aether.toml", "r");
    if (!manifest) {
        fprintf(stderr, "Error: No aether.toml found. Run 'apkg init' first.\n");
        return 1;
    }
    fclose(manifest);
    
    // Check if package is in cache
    PackageInfo info = apkg_find_package(package);
    
    if (!info.exists) {
        printf("Package not found in cache, downloading...\n");
        if (apkg_download_package(package, "latest") != 0) {
            fprintf(stderr, "Failed to download package\n");
            free(info.name);
            free(info.path);
            return 1;
        }
    } else {
        printf("Using cached package: %s\n", info.path);
    }
    
    // Add to aether.toml dependencies
    FILE* toml = fopen("aether.toml", "a");
    if (toml) {
        fprintf(toml, "\n# Added by apkg install\n");
        fprintf(toml, "%s = \"latest\"\n", package);
        fclose(toml);
        printf("✓ Added %s to dependencies\n", package);
    }
    
    free(info.name);
    free(info.path);
    
    printf("✓ Package '%s' installed\n", package);
    return 0;
}

int apkg_publish() {
    printf("Publishing package...\n");
    
    FILE* manifest = fopen("aether.toml", "r");
    if (!manifest) {
        fprintf(stderr, "Error: No aether.toml found.\n");
        return 1;
    }
    fclose(manifest);
    
    printf("TODO: Publishing not yet implemented.\n");
    printf("Will publish to registry after validation:\n");
    printf("  1. Validate aether.toml\n");
    printf("  2. Run tests\n");
    printf("  3. Build package\n");
    printf("  4. Create tarball\n");
    printf("  5. Upload to registry\n");
    
    return 0;
}

int apkg_build() {
    printf("Building package...\n");
    
    FILE* manifest = fopen("aether.toml", "r");
    if (!manifest) {
        fprintf(stderr, "Error: No aether.toml found.\n");
        return 1;
    }
    fclose(manifest);
    
    printf("Compiling src/main.ae...\n");
    
    int result = system("aetherc src/main.ae target/main.c");
    if (result != 0) {
        fprintf(stderr, "Error: Compilation failed\n");
        return 1;
    }
    
    printf("✓ Build complete\n");
    return 0;
}

int apkg_test() {
    printf("Running tests...\n");
    
    printf("TODO: Test runner not yet implemented.\n");
    printf("Will run:\n");
    printf("  1. Unit tests\n");
    printf("  2. Integration tests\n");
    printf("  3. Documentation tests\n");
    
    return 0;
}

int apkg_search(const char* query) {
    printf("Searching for '%s'...\n", query);
    
    printf("TODO: Package search not yet implemented.\n");
    printf("Will search registry for packages matching '%s'\n", query);
    
    return 0;
}

int apkg_update() {
    printf("Updating dependencies...\n");
    
    FILE* manifest = fopen("aether.toml", "r");
    if (!manifest) {
        fprintf(stderr, "Error: No aether.toml found.\n");
        return 1;
    }
    fclose(manifest);
    
    printf("TODO: Dependency updates not yet implemented.\n");
    printf("Will update all dependencies to latest compatible versions.\n");
    
    return 0;
}

int apkg_run() {
    printf("Running package...\n");
    
    if (access("target/main.c", F_OK) != 0) {
        printf("Building first...\n");
        if (apkg_build() != 0) {
            return 1;
        }
    }
    
    printf("\nTODO: Execute compiled binary\n");
    
    return 0;
}

void apkg_print_help() {
    printf("apkg - Aether Package Manager v%s\n\n", APKG_VERSION);
    printf("USAGE:\n");
    printf("    apkg <command> [options]\n\n");
    printf("COMMANDS:\n");
    printf("    init <name>       Initialize a new package\n");
    printf("    install <pkg>     Install a package\n");
    printf("    update            Update dependencies\n");
    printf("    build             Build the package\n");
    printf("    run               Build and run the package\n");
    printf("    test              Run tests\n");
    printf("    publish           Publish package to registry\n");
    printf("    search <query>    Search for packages\n");
    printf("    help              Show this help message\n");
    printf("    version           Show version information\n\n");
    printf("For more information, see: https://docs.aetherlang.org/apkg\n");
}

void apkg_print_version() {
    printf("apkg %s\n", APKG_VERSION);
}

Package* apkg_parse_manifest(const char* path) {
    return NULL;
}

void apkg_free_package(Package* pkg) {
    if (!pkg) return;
    free(pkg->name);
    free(pkg->version);
    free(pkg->description);
    free(pkg->license);
    for (int i = 0; i < pkg->author_count; i++) {
        free(pkg->authors[i]);
    }
    free(pkg->authors);
    for (int i = 0; i < pkg->dependency_count; i++) {
        free(pkg->dependencies[i]);
    }
    free(pkg->dependencies);
    free(pkg);
}

int apkg_save_manifest(Package* pkg, const char* path) {
    return 0;
}

PackageInfo apkg_find_package(const char* name) {
    PackageInfo info = {0};
    info.name = strdup(name);
    
    // Determine cache location
    char cache_dir[512];
    #ifdef _WIN32
        const char* home = getenv("USERPROFILE");
        snprintf(cache_dir, sizeof(cache_dir), "%s\\.aether\\packages", home ? home : ".");
    #else
        const char* home = getenv("HOME");
        snprintf(cache_dir, sizeof(cache_dir), "%s/.aether/packages", home ? home : ".");
    #endif
    
    // Build full path
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s/%s", cache_dir, name);
    
    info.path = strdup(full_path);
    info.exists = (access(full_path, F_OK) == 0);
    
    return info;
}

int apkg_download_package(const char* name, const char* version) {
    // Parse GitHub URL from package name
    // Expected format: github.com/user/repo
    if (strncmp(name, "github.com/", 11) != 0) {
        fprintf(stderr, "Error: Only GitHub packages supported currently\n");
        fprintf(stderr, "Package name must start with 'github.com/'\n");
        return 1;
    }
    
    const char* repo_path = name + 11;  // Skip "github.com/"
    
    // Create package cache directory
    char cache_dir[512];
    #ifdef _WIN32
        const char* home = getenv("USERPROFILE");
        snprintf(cache_dir, sizeof(cache_dir), "%s\\.aether\\packages", home ? home : ".");
    #else
        const char* home = getenv("HOME");
        snprintf(cache_dir, sizeof(cache_dir), "%s/.aether/packages", home ? home : ".");
    #endif
    
    // Create nested directories for github.com/user/repo structure
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s/%s", cache_dir, name);
    
    // Check if already downloaded
    if (access(full_path, F_OK) == 0) {
        printf("Package already cached: %s\n", full_path);
        return 0;
    }
    
    printf("Fetching package: %s\n", name);
    
    // Create parent directories
    char mkdir_cmd[1024];
    char parent_dir[1024];
    snprintf(parent_dir, sizeof(parent_dir), "%s/github.com", cache_dir);
    
    #ifdef _WIN32
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "if not exist \"%s\" mkdir \"%s\"", cache_dir, cache_dir);
        system(mkdir_cmd);
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "if not exist \"%s\" mkdir \"%s\"", parent_dir, parent_dir);
        system(mkdir_cmd);
    #else
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", parent_dir);
        system(mkdir_cmd);
    #endif
    
    // Extract user part for parent directory
    char user_dir[1024];
    const char* slash = strchr(repo_path, '/');
    if (slash) {
        int user_len = slash - repo_path;
        snprintf(user_dir, sizeof(user_dir), "%s/github.com/%.*s", cache_dir, user_len, repo_path);
        #ifdef _WIN32
            snprintf(mkdir_cmd, sizeof(mkdir_cmd), "if not exist \"%s\" mkdir \"%s\"", user_dir, user_dir);
        #else
            snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", user_dir);
        #endif
        system(mkdir_cmd);
    }
    
    // Clone repository
    char clone_cmd[1024];
    if (version && strcmp(version, "latest") != 0) {
        // Clone specific version/tag
        snprintf(clone_cmd, sizeof(clone_cmd), 
                 "git clone --depth 1 --branch %s https://%s \"%s\"",
                 version, name, full_path);
    } else {
        // Clone latest
        snprintf(clone_cmd, sizeof(clone_cmd), 
                 "git clone --depth 1 https://%s \"%s\"",
                 name, full_path);
    }
    
    printf("Running: %s\n", clone_cmd);
    int result = system(clone_cmd);
    
    if (result != 0) {
        fprintf(stderr, "Error: Failed to clone package\n");
        return 1;
    }
    
    printf("✓ Package downloaded to: %s\n", full_path);
    
    // Check for aether.toml
    char manifest_path[1024];
    snprintf(manifest_path, sizeof(manifest_path), "%s/aether.toml", full_path);
    if (access(manifest_path, F_OK) == 0) {
        printf("✓ Found aether.toml\n");
    } else {
        fprintf(stderr, "Warning: No aether.toml found in package\n");
    }
    
    return 0;
}

