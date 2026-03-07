#include "aether_fs.h"
#include "../../runtime/utils/aether_compiler.h"
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

    FILE* fp = fopen(path, mode);
    if (!fp) return NULL;

    File* file = (File*)malloc(sizeof(File));
    file->handle = fp;
    file->is_open = 1;
    file->path = strdup(path);
    return file;
}

AetherString* file_read_all(File* file) {
    if (!file || !file->is_open) return NULL;

    FILE* fp = (FILE*)file->handle;
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char* buffer = (char*)malloc(size + 1);
    size_t read = fread(buffer, 1, size, fp);
    buffer[read] = '\0';

    AetherString* result = string_new_with_length(buffer, read);
    free(buffer);
    return result;
}

int file_write(File* file, const char* data, size_t length) {
    if (!file || !file->is_open || !data) return -1;

    FILE* fp = (FILE*)file->handle;
    size_t written = fwrite(data, 1, length, fp);
    return (written == length) ? 0 : -1;
}

int file_close(File* file) {
    if (!file) return -1;

    if (file->is_open) {
        fclose((FILE*)file->handle);
        file->is_open = 0;
    }

    free((void*)file->path);
    free(file);
    return 0;
}

int file_exists(const char* path) {
    if (!path) return 0;

    struct stat st;
    return (stat(path, &st) == 0 && !S_ISDIR(st.st_mode));
}

int file_delete(const char* path) {
    if (!path) return -1;
    return remove(path);
}

size_t file_size(const char* path) {
    if (!path) return 0;

    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return st.st_size;
}

// Directory operations
int dir_exists(const char* path) {
    if (!path) return 0;

    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

int dir_create(const char* path) {
    if (!path) return -1;
    return mkdir(path, 0755);
}

int dir_delete(const char* path) {
    if (!path) return -1;
    return rmdir(path);
}

// Path operations
AetherString* path_join(const char* path1, const char* path2) {
    if (!path1 || !path2) return NULL;

    size_t len1 = strlen(path1);
    size_t len2 = strlen(path2);

    #ifdef _WIN32
    char sep = '\\';
    #else
    char sep = '/';
    #endif

    int needs_sep = (len1 > 0 && path1[len1-1] != sep && path1[len1-1] != '/');
    size_t total = len1 + len2 + (needs_sep ? 1 : 0);

    char* result = (char*)malloc(total + 1);
    strcpy(result, path1);
    if (needs_sep) {
        result[len1] = sep;
        strcpy(result + len1 + 1, path2);
    } else {
        strcpy(result + len1, path2);
    }

    AetherString* str = string_new(result);
    free(result);
    return str;
}

AetherString* path_dirname(const char* path) {
    if (!path) return NULL;

    const char* last_sep = strrchr(path, '/');
    const char* last_sep_win = strrchr(path, '\\');

    if (last_sep_win && (!last_sep || last_sep_win > last_sep)) {
        last_sep = last_sep_win;
    }

    if (!last_sep) {
        return string_new(".");
    }

    size_t len = last_sep - path;
    if (len == 0) len = 1;  // Root directory

    char* result = (char*)malloc(len + 1);
    strncpy(result, path, len);
    result[len] = '\0';

    AetherString* str = string_new(result);
    free(result);
    return str;
}

AetherString* path_basename(const char* path) {
    if (!path) return NULL;

    const char* last_sep = strrchr(path, '/');
    const char* last_sep_win = strrchr(path, '\\');

    if (last_sep_win && (!last_sep || last_sep_win > last_sep)) {
        last_sep = last_sep_win;
    }

    const char* basename = last_sep ? last_sep + 1 : path;
    return string_new(basename);
}

AetherString* path_extension(const char* path) {
    if (!path) return NULL;

    const char* last_dot = strrchr(path, '.');
    const char* last_sep = strrchr(path, '/');
    const char* last_sep_win = strrchr(path, '\\');

    if (last_sep_win && (!last_sep || last_sep_win > last_sep)) {
        last_sep = last_sep_win;
    }

    if (!last_dot || (last_sep && last_dot < last_sep)) {
        return string_new("");
    }

    return string_new(last_dot);
}

int path_is_absolute(const char* path) {
    if (!path || path[0] == '\0') return 0;
    
    #ifdef _WIN32
    // Windows: C:\ or \\server\share
    if ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) {
        if (path[1] == ':' && (path[2] == '\\' || path[2] == '/')) {
            return 1;
        }
    }
    if (path[0] == '\\' && path[1] == '\\') return 1;
    #else
    // Unix: /path
    if (path[0] == '/') return 1;
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
            list->entries = (char**)realloc(list->entries, (list->count + 1) * sizeof(char*));
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
            list->entries = (char**)realloc(list->entries, (list->count + 1) * sizeof(char*));
            list->entries[list->count] = strdup(entry->d_name);
            list->count++;
        }
    }

    closedir(dir);
    #endif

    return list;
}

void dir_list_free(DirList* list) {
    if (!list) return;

    for (int i = 0; i < list->count; i++) {
        free(list->entries[i]);
    }
    free(list->entries);
    free(list);
}

