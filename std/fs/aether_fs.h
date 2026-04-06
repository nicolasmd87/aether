#ifndef AETHER_FS_H
#define AETHER_FS_H

#include <stddef.h>

// File operations
typedef struct {
    void* handle;
    int is_open;
    const char* path;
} File;

File* file_open(const char* path, const char* mode);
char* file_read_all(File* file);
int file_write(File* file, const char* data, int length);
int file_close(File* file);
int file_exists(const char* path);
int file_delete(const char* path);
int file_size(const char* path);
int file_mtime(const char* path);

// Directory operations
int dir_exists(const char* path);
int dir_create(const char* path);
int dir_delete(const char* path);

// Path operations
char* path_join(const char* path1, const char* path2);
char* path_dirname(const char* path);
char* path_basename(const char* path);
char* path_extension(const char* path);
int path_is_absolute(const char* path);

// Directory listing
typedef struct {
    char** entries;
    int count;
} DirList;

DirList* dir_list(const char* path);
int dir_list_count(DirList* list);
const char* dir_list_get(DirList* list, int index);
void dir_list_free(DirList* list);

// Glob: match files by pattern (e.g., "src/**/*.c")
// Returns a DirList with full paths of matching files.
DirList* fs_glob(const char* pattern);

#endif // AETHER_FS_H

