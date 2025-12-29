#ifndef AETHER_IO_H
#define AETHER_IO_H

#include "../string/aether_string.h"

// Console I/O
void aether_print(AetherString* str);
void aether_print_line(AetherString* str);
void aether_print_int(int value);
void aether_print_float(float value);

// File I/O
AetherString* aether_read_file(AetherString* path);
int aether_write_file(AetherString* path, AetherString* content);
int aether_append_file(AetherString* path, AetherString* content);
int aether_file_exists(AetherString* path);
int aether_delete_file(AetherString* path);

// File info
typedef struct {
    long size;
    int is_directory;
    long modified_time;
} AetherFileInfo;

AetherFileInfo* aether_file_info(AetherString* path);
void aether_file_info_free(AetherFileInfo* info);

#endif // AETHER_IO_H

