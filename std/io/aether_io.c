#include "aether_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

// Console I/O
void io_print(AetherString* str) {
    if (str && str->data) {
        printf("%s", str->data);
        fflush(stdout);
    }
}

void io_print_line(AetherString* str) {
    if (str && str->data) {
        printf("%s\n", str->data);
    } else {
        printf("\n");
    }
    fflush(stdout);
}

void io_print_int(int value) {
    printf("%d\n", value);
    fflush(stdout);
}

void io_print_float(float value) {
    printf("%g\n", value);
    fflush(stdout);
}

// File I/O
AetherString* io_read_file(AetherString* path) {
    if (!path || !path->data) return NULL;

    FILE* file = fopen(path->data, "rb");
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

    AetherString* content = string_new_with_length(buffer, read);
    free(buffer);
    return content;
}

int io_write_file(AetherString* path, AetherString* content) {
    if (!path || !path->data || !content) return 0;

    FILE* file = fopen(path->data, "wb");
    if (!file) return 0;

    size_t written = fwrite(content->data, 1, content->length, file);
    fclose(file);

    return written == content->length ? 1 : 0;
}

int io_append_file(AetherString* path, AetherString* content) {
    if (!path || !path->data || !content) return 0;

    FILE* file = fopen(path->data, "ab");
    if (!file) return 0;

    size_t written = fwrite(content->data, 1, content->length, file);
    fclose(file);

    return written == content->length ? 1 : 0;
}

int io_file_exists(AetherString* path) {
    if (!path || !path->data) return 0;

    FILE* file = fopen(path->data, "r");
    if (file) {
        fclose(file);
        return 1;
    }
    return 0;
}

int io_delete_file(AetherString* path) {
    if (!path || !path->data) return 0;
    return remove(path->data) == 0 ? 1 : 0;
}

// File info
FileInfo* io_file_info(AetherString* path) {
    if (!path || !path->data) return NULL;

    struct stat st;
    if (stat(path->data, &st) != 0) return NULL;

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
AetherString* io_getenv(const char* name) {
    if (!name) return NULL;
    const char* value = getenv(name);
    if (!value) return NULL;
    return string_new(value);
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

