#ifndef AETHER_IO_H
#define AETHER_IO_H

#include <stddef.h>

// Console I/O
void io_print(const char* str);
void io_print_line(const char* str);
void io_print_int(int value);
void io_print_float(double value);

// File I/O (raw)
char* io_read_file_raw(const char* path);
int io_write_file_raw(const char* path, const char* content);
int io_append_file_raw(const char* path, const char* content);
int io_file_exists(const char* path);
int io_delete_file_raw(const char* path);

// File info
typedef struct {
    long size;
    int is_directory;
    long modified_time;
} FileInfo;

FileInfo* io_file_info_raw(const char* path);
void io_file_info_free(FileInfo* info);

// Environment variables
char* io_getenv(const char* name);
int io_setenv_raw(const char* name, const char* value);
int io_unsetenv_raw(const char* name);

// Unbuffered fd-1 / fd-2 writes — bypass stdio buffering so output is
// guaranteed on the wire before the process exits / crashes / aborts.
// `length` is the count of bytes to write; data may contain NULs.
// Returns the number of bytes written, or -1 on error. Loops on
// partial writes and retries on EINTR.
//
// `println` / `io.print` go through stdio (line-buffered on tty,
// block-buffered when stdout is piped) — fine for normal output but
// loses the last few lines if the process is killed mid-buffer.
// Reach for these in crash-trace paths where you need the bytes
// flushed NOW. Section A.3 (minimal scope) of aether_changes_needed.md.
int io_stderr_write(const char* data, int length);
int io_stdout_write(const char* data, int length);

#endif // AETHER_IO_H

