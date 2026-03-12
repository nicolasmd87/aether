#ifndef AETHER_IO_H
#define AETHER_IO_H

#include <stddef.h>

// Console I/O
void io_print(const char* str);
void io_print_line(const char* str);
void io_print_int(int value);
void io_print_float(double value);

// File I/O
char* io_read_file(const char* path);
int io_write_file(const char* path, const char* content);
int io_append_file(const char* path, const char* content);
int io_file_exists(const char* path);
int io_delete_file(const char* path);

// File info
typedef struct {
    long size;
    int is_directory;
    long modified_time;
} FileInfo;

FileInfo* io_file_info(const char* path);
void io_file_info_free(FileInfo* info);

// Environment variables
char* io_getenv(const char* name);
int io_setenv(const char* name, const char* value);
int io_unsetenv(const char* name);

#endif // AETHER_IO_H

