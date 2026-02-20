#ifndef AETHER_FS_H
#define AETHER_FS_H

#include "../string/aether_string.h"

// File operations
typedef struct {
    void* handle;
    int is_open;
    const char* path;
} File;

File* file_open(const char* path, const char* mode);
AetherString* file_read_all(File* file);
int file_write(File* file, const char* data, size_t length);
int file_close(File* file);
int file_exists(const char* path);
int file_delete(const char* path);
size_t file_size(const char* path);

// Directory operations
int dir_exists(const char* path);
int dir_create(const char* path);
int dir_delete(const char* path);

// Path operations
AetherString* path_join(const char* path1, const char* path2);
AetherString* path_dirname(const char* path);
AetherString* path_basename(const char* path);
AetherString* path_extension(const char* path);
int path_is_absolute(const char* path);

// Directory listing
typedef struct {
    char** entries;
    int count;
} DirList;

DirList* dir_list(const char* path);
void dir_list_free(DirList* list);

#endif // AETHER_FS_H

