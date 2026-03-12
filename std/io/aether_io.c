#include "aether_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif
#ifndef stat
#define stat _stat
#endif
#endif

// Console I/O
void io_print(const char* str) {
    if (str) {
        printf("%s", str);
        fflush(stdout);
    }
}

void io_print_line(const char* str) {
    if (str) {
        printf("%s\n", str);
    } else {
        printf("\n");
    }
    fflush(stdout);
}

void io_print_int(int value) {
    printf("%d\n", value);
    fflush(stdout);
}

void io_print_float(double value) {
    printf("%g\n", value);
    fflush(stdout);
}

// File I/O
char* io_read_file(const char* path) {
    if (!path) return NULL;

    FILE* file = fopen(path, "rb");
    if (!file) return NULL;

    // Get file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Read file
    char* buffer = (char*)malloc(size + 1);
    size_t read = fread(buffer, 1, size, file);
    buffer[read] = '\0';
    fclose(file);

    return buffer;
}

int io_write_file(const char* path, const char* content) {
    if (!path || !content) return 0;

    FILE* file = fopen(path, "wb");
    if (!file) return 0;

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, file);
    fclose(file);

    return written == len ? 1 : 0;
}

int io_append_file(const char* path, const char* content) {
    if (!path || !content) return 0;

    FILE* file = fopen(path, "ab");
    if (!file) return 0;

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, file);
    fclose(file);

    return written == len ? 1 : 0;
}

int io_file_exists(const char* path) {
    if (!path) return 0;

    FILE* file = fopen(path, "r");
    if (file) {
        fclose(file);
        return 1;
    }
    return 0;
}

int io_delete_file(const char* path) {
    if (!path) return 0;
    return remove(path) == 0 ? 1 : 0;
}

// File info
FileInfo* io_file_info(const char* path) {
    if (!path) return NULL;

    struct stat st;
    if (stat(path, &st) != 0) return NULL;

    FileInfo* info = (FileInfo*)malloc(sizeof(FileInfo));
    info->size = st.st_size;
    info->is_directory = S_ISDIR(st.st_mode) ? 1 : 0;
    info->modified_time = (long)st.st_mtime;
    return info;
}

void io_file_info_free(FileInfo* info) {
    if (info) free(info);
}

// Environment variables
char* io_getenv(const char* name) {
    if (!name) return NULL;
    const char* value = getenv(name);
    if (!value) return NULL;
    return strdup(value);
}

int io_setenv(const char* name, const char* value) {
    if (!name || !value) return 0;
#ifdef _WIN32
    // Windows uses _putenv_s
    return _putenv_s(name, value) == 0 ? 1 : 0;
#else
    // POSIX setenv (1 = overwrite existing)
    return setenv(name, value, 1) == 0 ? 1 : 0;
#endif
}

int io_unsetenv(const char* name) {
    if (!name) return 0;
#ifdef _WIN32
    return _putenv_s(name, "") == 0 ? 1 : 0;
#else
    return unsetenv(name) == 0 ? 1 : 0;
#endif
}

