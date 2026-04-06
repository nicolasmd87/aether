#include "aether_fs.h"
#include "../../runtime/config/aether_optimization_config.h"
#include "../../runtime/utils/aether_compiler.h"
#include "../../runtime/aether_sandbox.h"

#if !AETHER_HAS_FILESYSTEM
// Stubs when filesystem is unavailable (WASM, embedded)
File* file_open(const char* p, const char* m) { (void)p; (void)m; return NULL; }
char* file_read_all(File* f) { (void)f; return NULL; }
int file_write(File* f, const char* d, int l) { (void)f; (void)d; (void)l; return 0; }
int file_close(File* f) { (void)f; return 0; }
int file_exists(const char* p) { (void)p; return 0; }
int file_delete(const char* p) { (void)p; return 0; }
int file_size(const char* p) { (void)p; return -1; }
int file_mtime(const char* p) { (void)p; return 0; }
int dir_exists(const char* p) { (void)p; return 0; }
int dir_create(const char* p) { (void)p; return 0; }
int dir_delete(const char* p) { (void)p; return 0; }
char* path_join(const char* a, const char* b) { (void)a; (void)b; return NULL; }
char* path_dirname(const char* p) { (void)p; return NULL; }
char* path_basename(const char* p) { (void)p; return NULL; }
char* path_extension(const char* p) { (void)p; return NULL; }
int path_is_absolute(const char* p) { (void)p; return 0; }
DirList* dir_list(const char* p) { (void)p; return NULL; }
int dir_list_count(DirList* l) { (void)l; return 0; }
const char* dir_list_get(DirList* l, int i) { (void)l; (void)i; return NULL; }
void dir_list_free(DirList* l) { (void)l; }
DirList* fs_glob(const char* p) { (void)p; return NULL; }
#else

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
    #include <direct.h>
    #include <windows.h>
    #define mkdir(path, mode) _mkdir(path)
    #define rmdir _rmdir
    #define stat _stat
    #define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#else
    #include <dirent.h>
    #include <unistd.h>
#endif

// File operations
File* file_open(const char* path, const char* mode) {
    if (!path || !mode) return NULL;

    // Sandbox check: determine read vs write from mode
    if (mode[0] == 'r') {
        if (!aether_sandbox_check("fs_read", path)) return NULL;
    } else {
        if (!aether_sandbox_check("fs_write", path)) return NULL;
    }

    FILE* fp = fopen(path, mode);
    if (!fp) return NULL;

    File* file = (File*)malloc(sizeof(File));
    if (!file) { fclose(fp); return NULL; }
    file->handle = fp;
    file->is_open = 1;
    file->path = strdup(path);
    return file;
}

char* file_read_all(File* file) {
    if (!file || !file->is_open) return NULL;

    FILE* fp = (FILE*)file->handle;
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    if (size < 0) return NULL;
    fseek(fp, 0, SEEK_SET);

    char* buffer = (char*)malloc(size + 1);
    if (!buffer) return NULL;
    size_t read = fread(buffer, 1, size, fp);
    buffer[read] = '\0';

    return buffer;
}

int file_write(File* file, const char* data, int length) {
    if (!file || !file->is_open || !data) return 0;

    FILE* fp = (FILE*)file->handle;
    size_t written = fwrite(data, 1, (size_t)length, fp);
    return (written == (size_t)length) ? 1 : 0;
}

int file_close(File* file) {
    if (!file) return 0;

    if (file->is_open) {
        fclose((FILE*)file->handle);
        file->is_open = 0;
    }

    free((void*)file->path);
    free(file);
    return 1;
}

int file_exists(const char* path) {
    if (!path) return 0;
    if (!aether_sandbox_check("fs_read", path)) return 0;

    struct stat st;
    return (stat(path, &st) == 0 && !S_ISDIR(st.st_mode));
}

int file_delete(const char* path) {
    if (!path) return 0;
    if (!aether_sandbox_check("fs_write", path)) return 0;
    return remove(path) == 0 ? 1 : 0;
}

int file_size(const char* path) {
    if (!path) return 0;
    if (!aether_sandbox_check("fs_read", path)) return 0;

    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (int)st.st_size;
}

int file_mtime(const char* path) {
    if (!path) return 0;

    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (int)st.st_mtime;
}

// Directory operations
int dir_exists(const char* path) {
    if (!path) return 0;

    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

int dir_create(const char* path) {
    if (!path) return 0;
    return mkdir(path, 0755) == 0 ? 1 : 0;
}

int dir_delete(const char* path) {
    if (!path) return 0;
    return rmdir(path) == 0 ? 1 : 0;
}

// Path operations
char* path_join(const char* path1, const char* path2) {
    if (!path1 || !path2) return NULL;

    size_t len1 = strlen(path1);
    size_t len2 = strlen(path2);

    // Always use '/' — it works on all platforms (Windows C stdlib accepts '/')
    // and keeps paths consistent with Aether's module system.
    char sep = '/';

    int needs_sep = (len1 > 0 && path1[len1-1] != '/' && path1[len1-1] != '\\');
    size_t total = len1 + len2 + (needs_sep ? 1 : 0);

    char* result = (char*)malloc(total + 1);
    strcpy(result, path1);
    if (needs_sep) {
        result[len1] = sep;
        strcpy(result + len1 + 1, path2);
    } else {
        strcpy(result + len1, path2);
    }

    return result;
}

char* path_dirname(const char* path) {
    if (!path) return NULL;

    const char* last_sep = strrchr(path, '/');
    const char* last_sep_win = strrchr(path, '\\');

    if (last_sep_win && (!last_sep || last_sep_win > last_sep)) {
        last_sep = last_sep_win;
    }

    if (!last_sep) {
        return strdup(".");
    }

    size_t len = last_sep - path;
    if (len == 0) len = 1;  // Root directory

    char* result = (char*)malloc(len + 1);
    strncpy(result, path, len);
    result[len] = '\0';

    return result;
}

char* path_basename(const char* path) {
    if (!path) return NULL;

    const char* last_sep = strrchr(path, '/');
    const char* last_sep_win = strrchr(path, '\\');

    if (last_sep_win && (!last_sep || last_sep_win > last_sep)) {
        last_sep = last_sep_win;
    }

    const char* base = last_sep ? last_sep + 1 : path;
    return strdup(base);
}

char* path_extension(const char* path) {
    if (!path) return NULL;

    const char* last_dot = strrchr(path, '.');
    const char* last_sep = strrchr(path, '/');
    const char* last_sep_win = strrchr(path, '\\');

    if (last_sep_win && (!last_sep || last_sep_win > last_sep)) {
        last_sep = last_sep_win;
    }

    if (!last_dot || (last_sep && last_dot < last_sep)) {
        return strdup("");
    }

    return strdup(last_dot);
}

int path_is_absolute(const char* path) {
    if (!path || path[0] == '\0') return 0;

    // Unix-style absolute: /path (works on all platforms)
    if (path[0] == '/') return 1;

    #ifdef _WIN32
    // Windows: C:\ or C:/ or \\server\share
    if ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) {
        if (path[1] && path[1] == ':' && path[2] && (path[2] == '\\' || path[2] == '/')) {
            return 1;
        }
    }
    if (path[0] == '\\' && path[1] && path[1] == '\\') return 1;
    #endif

    return 0;
}

// Directory listing
DirList* dir_list(const char* path) {
    if (!path) return NULL;

    DirList* list = (DirList*)malloc(sizeof(DirList));
    list->entries = NULL;
    list->count = 0;

    #ifdef _WIN32
    WIN32_FIND_DATAA find_data;
    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*", path);

    HANDLE hFind = FindFirstFileA(search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        return list;
    }

    do {
        if (strcmp(find_data.cFileName, ".") != 0 &&
            strcmp(find_data.cFileName, "..") != 0) {
            char** new_entries = (char**)realloc(list->entries, (list->count + 1) * sizeof(char*));
            if (!new_entries) break;
            list->entries = new_entries;
            list->entries[list->count] = strdup(find_data.cFileName);
            list->count++;
        }
    } while (FindNextFileA(hFind, &find_data));

    FindClose(hFind);
    #else
    DIR* dir = opendir(path);
    if (!dir) return list;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char** new_entries = (char**)realloc(list->entries, (list->count + 1) * sizeof(char*));
            if (!new_entries) break;
            list->entries = new_entries;
            list->entries[list->count] = strdup(entry->d_name);
            list->count++;
        }
    }

    closedir(dir);
    #endif

    return list;
}

int dir_list_count(DirList* list) {
    return list ? list->count : 0;
}

const char* dir_list_get(DirList* list, int index) {
    if (!list || index < 0 || index >= list->count) return NULL;
    return list->entries[index];
}

void dir_list_free(DirList* list) {
    if (!list) return;

    for (int i = 0; i < list->count; i++) {
        free(list->entries[i]);
    }
    free(list->entries);
    free(list);
}

// --- Glob: pattern matching for file discovery ---

#ifndef _WIN32
#include <glob.h>
#include <fnmatch.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

// Helper: add a path to a DirList
static void dirlist_add(DirList* list, const char* path) {
    char** new_entries = (char**)realloc(list->entries, (list->count + 1) * sizeof(char*));
    if (!new_entries) return;
    list->entries = new_entries;
    list->entries[list->count] = strdup(path);
    list->count++;
}

#ifndef _WIN32
// Recursive walk for ** patterns (POSIX only)
static void walk_recursive(const char* dir, const char* suffix_pattern, DirList* result) {
    DIR* d = opendir(dir);
    if (!d) return;

    struct dirent* entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;  // skip hidden + . and ..

        char fullpath[4096];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, entry->d_name);

        struct stat st;
        if (stat(fullpath, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            // Recurse into subdirectory
            walk_recursive(fullpath, suffix_pattern, result);
        } else {
            // Check if filename matches the suffix pattern (e.g., "*.c")
            if (fnmatch(suffix_pattern, entry->d_name, 0) == 0) {
                dirlist_add(result, fullpath);
            }
        }
    }
    closedir(d);
}
#endif // !_WIN32

DirList* fs_glob(const char* pattern) {
    if (!pattern) return NULL;

    DirList* result = (DirList*)malloc(sizeof(DirList));
    if (!result) return NULL;
    result->entries = NULL;
    result->count = 0;

#ifdef _WIN32
    // Windows: basic glob via FindFirstFile (no ** support)
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                dirlist_add(result, fd.cFileName);
            }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#else
    // Check for ** (recursive glob)
    const char* dstar = strstr(pattern, "/**/");
    if (dstar) {
        // Split: prefix is the directory, suffix is the file pattern
        // e.g., "src/**/*.c" → dir="src", suffix="*.c"
        char dir[4096];
        int dirlen = (int)(dstar - pattern);
        if (dirlen == 0) {
            strcpy(dir, ".");
        } else {
            strncpy(dir, pattern, dirlen);
            dir[dirlen] = '\0';
        }
        const char* suffix = dstar + 4;  // skip "/**/"

        // Also match files directly in the base directory
        char direct[8192];
        snprintf(direct, sizeof(direct), "%s/%s", dir, suffix);
        glob_t g;
        if (glob(direct, 0, NULL, &g) == 0) {
            for (size_t i = 0; i < g.gl_pathc; i++) {
                dirlist_add(result, g.gl_pathv[i]);
            }
            globfree(&g);
        }

        // Recursive walk
        walk_recursive(dir, suffix, result);
    } else {
        // Simple glob (no **)
        glob_t g;
        if (glob(pattern, 0, NULL, &g) == 0) {
            for (size_t i = 0; i < g.gl_pathc; i++) {
                dirlist_add(result, g.gl_pathv[i]);
            }
            globfree(&g);
        }
    }
#endif

    return result;
}

#endif // AETHER_HAS_FILESYSTEM

