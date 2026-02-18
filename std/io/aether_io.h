#ifndef AETHER_IO_H
#define AETHER_IO_H

#include "../string/aether_string.h"

// Console I/O
void io_print(AetherString* str);
void io_print_line(AetherString* str);
void io_print_int(int value);
void io_print_float(float value);

// File I/O
AetherString* io_read_file(AetherString* path);
int io_write_file(AetherString* path, AetherString* content);
int io_append_file(AetherString* path, AetherString* content);
int io_file_exists(AetherString* path);
int io_delete_file(AetherString* path);

// File info
typedef struct {
    long size;
    int is_directory;
    long modified_time;
} FileInfo;

FileInfo* io_file_info(AetherString* path);
void io_file_info_free(FileInfo* info);

// Environment variables
AetherString* io_getenv(const char* name);
int io_setenv(const char* name, const char* value);
int io_unsetenv(const char* name);

#endif // AETHER_IO_H

