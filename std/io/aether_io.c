#include "aether_io.h"
#include "../../runtime/config/aether_optimization_config.h"

#if !AETHER_HAS_FILESYSTEM
// Console I/O always works; file ops return errors
#include <stdio.h>
#include <stdlib.h>
void io_print(const char* s) { if (s) fputs(s, stdout); }
void io_print_line(const char* s) { if (s) puts(s); else puts(""); }
void io_print_int(int v) { printf("%d", v); }
void io_print_float(double v) { printf("%g", v); }
char* io_read_file_raw(const char* p) { (void)p; return NULL; }
int io_write_file_raw(const char* p, const char* c) { (void)p; (void)c; return 0; }
int io_append_file_raw(const char* p, const char* c) { (void)p; (void)c; return 0; }
int io_file_exists(const char* p) { (void)p; return 0; }
int io_delete_file_raw(const char* p) { (void)p; return 0; }
FileInfo* io_file_info_raw(const char* p) { (void)p; return NULL; }
void io_file_info_free(FileInfo* i) { (void)i; }
char* io_getenv(const char* n) { (void)n; return NULL; }
int io_setenv_raw(const char* n, const char* v) { (void)n; (void)v; return 0; }
int io_unsetenv_raw(const char* n) { (void)n; return 0; }
int io_stderr_write(const char* d, int n) {
    if (!d || n <= 0) return 0;
    fflush(stderr);
    size_t w = fwrite(d, 1, (size_t)n, stderr);
    fflush(stderr);
    return (int)w;
}
int io_stdout_write(const char* d, int n) {
    if (!d || n <= 0) return 0;
    fflush(stdout);
    size_t w = fwrite(d, 1, (size_t)n, stdout);
    fflush(stdout);
    return (int)w;
}
#else

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef _WIN32
#include <io.h>      // _write
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif
#ifndef stat
#define stat _stat
#endif
#define AE_FD_WRITE(fd, buf, n) _write((fd), (buf), (unsigned int)(n))
#else
#include <unistd.h>  // write
#define AE_FD_WRITE(fd, buf, n) write((fd), (buf), (size_t)(n))
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
char* io_read_file_raw(const char* path) {
    if (!path) return NULL;

    FILE* file = fopen(path, "rb");
    if (!file) return NULL;

    // Get file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    if (size < 0) { fclose(file); return NULL; }
    fseek(file, 0, SEEK_SET);

    // Read file
    char* buffer = (char*)malloc(size + 1);
    if (!buffer) { fclose(file); return NULL; }
    size_t read = fread(buffer, 1, size, file);
    buffer[read] = '\0';
    fclose(file);

    return buffer;
}

int io_write_file_raw(const char* path, const char* content) {
    if (!path || !content) return 0;

    FILE* file = fopen(path, "wb");
    if (!file) return 0;

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, file);
    fclose(file);

    return written == len ? 1 : 0;
}

int io_append_file_raw(const char* path, const char* content) {
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

int io_delete_file_raw(const char* path) {
    if (!path) return 0;
    return remove(path) == 0 ? 1 : 0;
}

// File info
FileInfo* io_file_info_raw(const char* path) {
    if (!path) return NULL;

    struct stat st;
    if (stat(path, &st) != 0) return NULL;

    FileInfo* info = (FileInfo*)malloc(sizeof(FileInfo));
    if (!info) return NULL;
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

int io_setenv_raw(const char* name, const char* value) {
    if (!name || !value) return 0;
#ifdef _WIN32
    // Windows uses _putenv_s
    return _putenv_s(name, value) == 0 ? 1 : 0;
#else
    // POSIX setenv (1 = overwrite existing)
    return setenv(name, value, 1) == 0 ? 1 : 0;
#endif
}

int io_unsetenv_raw(const char* name) {
    if (!name) return 0;
#ifdef _WIN32
    return _putenv_s(name, "") == 0 ? 1 : 0;
#else
    return unsetenv(name) == 0 ? 1 : 0;
#endif
}

/* Loop write to fd until all bytes are out, retrying on EINTR.
 * Returns total bytes written, or -1 on a non-EINTR error. */
static int fd_write_all(int fd, const char* data, int length) {
    if (!data) return -1;
    if (length <= 0) return 0;
    int total = 0;
    while (total < length) {
#ifdef _WIN32
        int w = AE_FD_WRITE(fd, data + total, length - total);
#else
        long w = AE_FD_WRITE(fd, data + total, length - total);
#endif
        if (w < 0) {
#ifndef _WIN32
            if (errno == EINTR) continue;
#endif
            return -1;
        }
        total += (int)w;
    }
    return total;
}

int io_stderr_write(const char* data, int length) {
    /* fflush stdio's buffer first so interleaved println / fprintf
     * output isn't reordered around the unbuffered write. */
    fflush(stderr);
    return fd_write_all(2, data, length);
}

int io_stdout_write(const char* data, int length) {
    fflush(stdout);
    return fd_write_all(1, data, length);
}

#endif // AETHER_HAS_FILESYSTEM

