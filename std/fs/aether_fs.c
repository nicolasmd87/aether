#include "aether_fs.h"
#include "../../runtime/config/aether_optimization_config.h"
#include "../../runtime/utils/aether_compiler.h"
#include "../../runtime/aether_sandbox.h"

#if !AETHER_HAS_FILESYSTEM
// Stubs when filesystem is unavailable (WASM, embedded)
File* file_open_raw(const char* p, const char* m) { (void)p; (void)m; return NULL; }
char* file_read_all_raw(File* f) { (void)f; return NULL; }
int file_write_raw(File* f, const char* d, int l) { (void)f; (void)d; (void)l; return 0; }
int file_close(File* f) { (void)f; return 0; }
int file_exists(const char* p) { (void)p; return 0; }
int file_delete_raw(const char* p) { (void)p; return 0; }
int file_size_raw(const char* p) { (void)p; return -1; }
int file_mtime(const char* p) { (void)p; return 0; }
int dir_exists(const char* p) { (void)p; return 0; }
int dir_create_raw(const char* p) { (void)p; return 0; }
int dir_delete_raw(const char* p) { (void)p; return 0; }
int fs_mkdir_p_raw(const char* p) { (void)p; return 0; }
int fs_symlink_raw(const char* t, const char* l) { (void)t; (void)l; return 0; }
char* fs_readlink_raw(const char* p) { (void)p; return NULL; }
int fs_is_symlink(const char* p) { (void)p; return 0; }
int fs_unlink_raw(const char* p) { (void)p; return 0; }
int fs_write_atomic_raw(const char* p, const char* d, int l) {
    (void)p; (void)d; (void)l; return 0;
}
int fs_rename_raw(const char* f, const char* t) { (void)f; (void)t; return 0; }
int fs_stat_raw(const char* p, int* k, int* s, int* m) {
    (void)p;
    if (k) *k = 0; if (s) *s = 0; if (m) *m = 0;
    return 0;
}
int fs_try_stat(const char* p) { (void)p; return 0; }
int fs_get_stat_kind(void)  { return 0; }
int fs_get_stat_size(void)  { return 0; }
int fs_get_stat_mtime(void) { return 0; }
char* fs_read_binary_raw(const char* p, int* n) {
    (void)p; if (n) *n = 0; return NULL;
}
int fs_try_read_binary(const char* p) { (void)p; return 0; }
const char* fs_get_read_binary(void) { return NULL; }
int fs_get_read_binary_length(void) { return 0; }
void fs_release_read_binary(void) {}
char* path_join(const char* a, const char* b) { (void)a; (void)b; return NULL; }
char* path_dirname(const char* p) { (void)p; return NULL; }
char* path_basename(const char* p) { (void)p; return NULL; }
char* path_extension(const char* p) { (void)p; return NULL; }
int path_is_absolute(const char* p) { (void)p; return 0; }
DirList* dir_list_raw(const char* p) { (void)p; return NULL; }
int dir_list_count(DirList* l) { (void)l; return 0; }
const char* dir_list_get(DirList* l, int i) { (void)l; (void)i; return NULL; }
void dir_list_free(DirList* l) { (void)l; }
DirList* fs_glob_raw(const char* p) { (void)p; return NULL; }
DirList* fs_glob_multi_raw(void* l) { (void)l; return NULL; }
#else

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#ifdef _WIN32
    #include <direct.h>
    #include <io.h>            // _unlink (for fs_unlink_raw on Windows)
    #include <process.h>       // _getpid (for fs_write_atomic_raw tmp path)
    #include <windows.h>
    #define mkdir(path, mode) _mkdir(path)
    #define rmdir _rmdir
    #define stat _stat
    #define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
    // Windows stat result has no S_ISREG / S_ISLNK macros; emulate the
    // former from the mode bits. S_ISLNK stays undefined here because
    // the lstat branch in fs_stat_raw is guarded by #ifndef _WIN32.
    #ifndef S_ISREG
        #define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
    #endif
#else
    #include <dirent.h>
    #include <unistd.h>
#endif

// File operations
File* file_open_raw(const char* path, const char* mode) {
    if (!path || !mode) return NULL;

    // Sandbox check: determine read vs write from mode
    if (mode[0] == 'r') {
        if (!aether_sandbox_check("fs_read", path)) return NULL;
    } else {
        if (!aether_sandbox_check("fs_write", path)) return NULL;
    }

    FILE* fp = fopen(path, mode);
    if (!fp) return NULL;

    File* file = (File*)malloc(sizeof(File));
    if (!file) { fclose(fp); return NULL; }
    file->handle = fp;
    file->is_open = 1;
    file->path = strdup(path);
    return file;
}

char* file_read_all_raw(File* file) {
    if (!file || !file->is_open) return NULL;

    FILE* fp = (FILE*)file->handle;
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    if (size < 0) return NULL;
    fseek(fp, 0, SEEK_SET);

    char* buffer = (char*)malloc(size + 1);
    if (!buffer) return NULL;
    size_t read = fread(buffer, 1, size, fp);
    buffer[read] = '\0';

    return buffer;
}

int file_write_raw(File* file, const char* data, int length) {
    if (!file || !file->is_open || !data) return 0;

    FILE* fp = (FILE*)file->handle;
    size_t written = fwrite(data, 1, (size_t)length, fp);
    return (written == (size_t)length) ? 1 : 0;
}

int file_close(File* file) {
    if (!file) return 0;

    if (file->is_open) {
        fclose((FILE*)file->handle);
        file->is_open = 0;
    }

    free((void*)file->path);
    free(file);
    return 1;
}

int file_exists(const char* path) {
    if (!path) return 0;
    if (!aether_sandbox_check("fs_read", path)) return 0;

    struct stat st;
    return (stat(path, &st) == 0 && !S_ISDIR(st.st_mode));
}

int file_delete_raw(const char* path) {
    if (!path) return 0;
    if (!aether_sandbox_check("fs_write", path)) return 0;
    return remove(path) == 0 ? 1 : 0;
}

int file_size_raw(const char* path) {
    if (!path) return 0;
    if (!aether_sandbox_check("fs_read", path)) return 0;

    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (int)st.st_size;
}

int file_mtime(const char* path) {
    if (!path) return 0;

    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (int)st.st_mtime;
}

// Directory operations
int dir_exists(const char* path) {
    if (!path) return 0;

    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

int dir_create_raw(const char* path) {
    if (!path) return 0;
    return mkdir(path, 0755) == 0 ? 1 : 0;
}

int dir_delete_raw(const char* path) {
    if (!path) return 0;
    return rmdir(path) == 0 ? 1 : 0;
}

// `mkdir -p` semantics: walk through each '/' in `path`, creating each
// intermediate directory if it doesn't already exist. Treats EEXIST as
// success at every step. Returns 1 on success, 0 on failure (e.g. path
// too long, or one of the components exists but isn't a directory).
int fs_mkdir_p_raw(const char* path) {
    if (!path || !*path) return 0;
    if (!aether_sandbox_check("fs_write", path)) return 0;

    char buf[4096];
    size_t len = strlen(path);
    if (len >= sizeof(buf)) return 0;
    memcpy(buf, path, len + 1);

    // Step through each '/' in the interior of the path, creating each
    // prefix as we go. Skip a leading slash so we don't try to mkdir("").
    for (size_t i = 1; i < len; i++) {
        if (buf[i] == '/') {
            buf[i] = '\0';
            if (mkdir(buf, 0755) != 0) {
                // Tolerate already-exists. Anything else is a real failure.
                if (errno != EEXIST) return 0;
                struct stat st;
                if (stat(buf, &st) != 0 || !S_ISDIR(st.st_mode)) return 0;
            }
            buf[i] = '/';
        }
    }
    // Final component (if not already covered by a trailing slash)
    if (mkdir(buf, 0755) != 0) {
        if (errno != EEXIST) return 0;
        struct stat st;
        if (stat(buf, &st) != 0 || !S_ISDIR(st.st_mode)) return 0;
    }
    return 1;
}

#ifndef _WIN32

// Create a symbolic link at `link_path` pointing to `target`. The target
// is recorded verbatim — relative targets stay relative.
int fs_symlink_raw(const char* target, const char* link_path) {
    if (!target || !link_path) return 0;
    if (!aether_sandbox_check("fs_write", link_path)) return 0;
    return symlink(target, link_path) == 0 ? 1 : 0;
}

// Read a symbolic link. Returns the target as a heap-allocated string,
// or NULL if `path` isn't a symlink or can't be read.
char* fs_readlink_raw(const char* path) {
    if (!path) return NULL;
    if (!aether_sandbox_check("fs_read", path)) return NULL;

    char buf[4096];
    ssize_t n = readlink(path, buf, sizeof(buf) - 1);
    if (n < 0) return NULL;
    buf[n] = '\0';
    return strdup(buf);
}

// Returns 1 if `path` is a symlink (does NOT follow the link to check
// the target). Returns 0 otherwise — including when the path doesn't
// exist.
int fs_is_symlink(const char* path) {
    if (!path) return 0;
    if (!aether_sandbox_check("fs_read", path)) return 0;

    struct stat st;
    if (lstat(path, &st) != 0) return 0;
    return S_ISLNK(st.st_mode) ? 1 : 0;
}

// Remove a file or symlink. Will NOT remove a directory — use dir_delete
// for that. Returns 1 on success, 0 on failure.
int fs_unlink_raw(const char* path) {
    if (!path) return 0;
    if (!aether_sandbox_check("fs_write", path)) return 0;
    return unlink(path) == 0 ? 1 : 0;
}

#else // _WIN32

// Windows symlinks need elevation or developer mode and use a different
// API surface. For now these are stubs returning failure; a follow-up
// PR can add CreateSymbolicLinkW + a junction fallback for directories.
int fs_symlink_raw(const char* t, const char* l) { (void)t; (void)l; return 0; }
char* fs_readlink_raw(const char* p) { (void)p; return NULL; }
int fs_is_symlink(const char* p) { (void)p; return 0; }
int fs_unlink_raw(const char* path) {
    if (!path) return 0;
    if (!aether_sandbox_check("fs_write", path)) return 0;
    return _unlink(path) == 0 ? 1 : 0;
}

#endif // !_WIN32

// ---------------------------------------------------------------------
// Durable/atomic/stat helpers. Cross-platform — rely on fopen + stat +
// rename which exist on both POSIX and Windows CRT. The POSIX-specific
// fsync path is guarded by #ifndef _WIN32; Windows gets a best-effort
// fflush (no FlushFileBuffers call for v1 — good enough for the
// atomicity guarantee, since rename itself is still the durable step).
// ---------------------------------------------------------------------

#include <time.h>

int fs_write_atomic_raw(const char* path, const char* data, int length) {
    if (!path || length < 0) return 0;
    if (!aether_sandbox_check("fs_write", path)) return 0;

    // Build a tmp path <path>.tmp.<pid>.<counter>. The counter keeps
    // concurrent writers from the same PID (unlikely but cheap to
    // guard against) from stomping each other's tmp files.
    static unsigned long s_counter = 0;
    char tmp[4096];
    int plen = (int)strlen(path);
    if (plen <= 0 || plen >= (int)sizeof(tmp) - 32) return 0;
    long pid =
#ifdef _WIN32
        (long)_getpid();
#else
        (long)getpid();
#endif
    unsigned long n = ++s_counter;
    snprintf(tmp, sizeof(tmp), "%s.tmp.%ld.%lu", path, pid, n);

    FILE* fp = fopen(tmp, "wb");
    if (!fp) return 0;

    size_t written = (length > 0) ? fwrite(data, 1, (size_t)length, fp) : 0;
    int fwrite_ok = (written == (size_t)length);

#ifndef _WIN32
    // Flush + fsync before rename. Without fsync a power loss between
    // the rename and the kernel flushing the tmp's data could leave
    // the destination pointing at zero-length or garbage contents.
    int fsync_ok = 1;
    if (fwrite_ok) {
        if (fflush(fp) != 0) fsync_ok = 0;
        else if (fsync(fileno(fp)) != 0) fsync_ok = 0;
    }
#else
    int fsync_ok = fwrite_ok ? (fflush(fp) == 0) : 0;
#endif

    if (fclose(fp) != 0) fsync_ok = 0;

    if (!fwrite_ok || !fsync_ok) {
        remove(tmp);  // don't leak a half-written tmp file
        return 0;
    }

    // rename(2) is atomic on POSIX when src and dst are on the same
    // filesystem — which they always are here, since we put the tmp
    // right next to the destination. On Windows rename will fail if
    // the destination exists, so drop it first; we don't race-check
    // because concurrent writers to the same path is already UB.
#ifdef _WIN32
    remove(path);
#endif
    if (rename(tmp, path) != 0) {
        remove(tmp);
        return 0;
    }
    return 1;
}

int fs_rename_raw(const char* from, const char* to) {
    if (!from || !to) return 0;
    if (!aether_sandbox_check("fs_write", from)) return 0;
    if (!aether_sandbox_check("fs_write", to)) return 0;
    return rename(from, to) == 0 ? 1 : 0;
}

// Kind encoding for fs_stat_raw's out_kind:
//   1 = regular file, 2 = directory, 3 = symlink, 4 = other.
// A symlink is reported as kind 3 even if its target is a file or
// directory — lstat(2) never follows. Callers that want "size of
// the target" should readlink + stat the target explicitly.
#define FS_STAT_KIND_FILE    1
#define FS_STAT_KIND_DIR     2
#define FS_STAT_KIND_SYMLINK 3
#define FS_STAT_KIND_OTHER   4

int fs_stat_raw(const char* path, int* out_kind,
                int* out_size, int* out_mtime) {
    if (!path) {
        if (out_kind)  *out_kind  = 0;
        if (out_size)  *out_size  = 0;
        if (out_mtime) *out_mtime = 0;
        return 0;
    }
    if (!aether_sandbox_check("fs_read", path)) {
        if (out_kind)  *out_kind  = 0;
        if (out_size)  *out_size  = 0;
        if (out_mtime) *out_mtime = 0;
        return 0;
    }

    struct stat st;
#ifndef _WIN32
    if (lstat(path, &st) != 0) {
#else
    // Windows CRT has no lstat; stat follows symlinks, but Windows
    // symlinks already go through a different code path we stub out
    // (fs_is_symlink returns 0). Good enough for v1.
    if (stat(path, &st) != 0) {
#endif
        if (out_kind)  *out_kind  = 0;
        if (out_size)  *out_size  = 0;
        if (out_mtime) *out_mtime = 0;
        return 0;
    }

    int kind;
#ifndef _WIN32
    if (S_ISLNK(st.st_mode))       kind = FS_STAT_KIND_SYMLINK;
    else
#endif
    if (S_ISREG(st.st_mode))       kind = FS_STAT_KIND_FILE;
    else if (S_ISDIR(st.st_mode))  kind = FS_STAT_KIND_DIR;
    else                           kind = FS_STAT_KIND_OTHER;

    if (out_kind)  *out_kind  = kind;
    if (out_size)  *out_size  = (int)st.st_size;
    if (out_mtime) *out_mtime = (int)st.st_mtime;
    return 1;
}

// Thread-local cache for the split fs_try_stat / fs_get_stat_* pair.
// Storing last-stat result here lets Aether callers work without
// allocating C out-params. The trio is called sequentially by the
// Aether-side file_stat wrapper, so the cache only has to survive
// that short window on the calling thread.
#if defined(__GNUC__) || defined(__clang__)
  #define AETHER_FS_TLS __thread
#else
  #define AETHER_FS_TLS
#endif
static AETHER_FS_TLS int s_last_kind  = 0;
static AETHER_FS_TLS int s_last_size  = 0;
static AETHER_FS_TLS int s_last_mtime = 0;

int fs_try_stat(const char* path) {
    int k = 0, sz = 0, mt = 0;
    int ok = fs_stat_raw(path, &k, &sz, &mt);
    if (!ok) {
        s_last_kind = 0; s_last_size = 0; s_last_mtime = 0;
        return 0;
    }
    s_last_kind = k; s_last_size = sz; s_last_mtime = mt;
    return 1;
}

int fs_get_stat_kind(void)  { return s_last_kind;  }
int fs_get_stat_size(void)  { return s_last_size;  }
int fs_get_stat_mtime(void) { return s_last_mtime; }

char* fs_read_binary_raw(const char* path, int* out_len) {
    if (out_len) *out_len = 0;
    if (!path) return NULL;
    if (!aether_sandbox_check("fs_read", path)) return NULL;

    FILE* fp = fopen(path, "rb");
    if (!fp) return NULL;

    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return NULL; }
    long size = ftell(fp);
    if (size < 0) { fclose(fp); return NULL; }
    if (fseek(fp, 0, SEEK_SET) != 0) { fclose(fp); return NULL; }

    // Allocate size+1 so we can append a NUL past the end — handy for
    // callers who know the content is text and want to treat it as a
    // C string. The `out_len` byte count does NOT include this NUL.
    char* buf = (char*)malloc((size_t)size + 1);
    if (!buf) { fclose(fp); return NULL; }

    size_t read = (size > 0) ? fread(buf, 1, (size_t)size, fp) : 0;
    fclose(fp);
    if (read != (size_t)size) { free(buf); return NULL; }

    buf[size] = '\0';
    if (out_len) *out_len = (int)size;
    return buf;
}

// TLS cache for the split fs_try_read_binary path. The cached buffer
// is owned here; the getter hands back a borrowed pointer valid
// until the next fs_try_read_binary call or until fs_release_read_binary
// explicitly releases it. Aether callers copy the bytes out before
// issuing another read.
static AETHER_FS_TLS char* s_read_binary_buf = NULL;
static AETHER_FS_TLS int   s_read_binary_len = 0;

void fs_release_read_binary(void) {
    free(s_read_binary_buf);
    s_read_binary_buf = NULL;
    s_read_binary_len = 0;
}

int fs_try_read_binary(const char* path) {
    fs_release_read_binary();  // drop any previous read
    int len = 0;
    char* buf = fs_read_binary_raw(path, &len);
    if (!buf) return 0;
    s_read_binary_buf = buf;
    s_read_binary_len = len;
    return 1;
}

const char* fs_get_read_binary(void) { return s_read_binary_buf; }
int fs_get_read_binary_length(void)  { return s_read_binary_len; }

// Path operations
char* path_join(const char* path1, const char* path2) {
    if (!path1 || !path2) return NULL;

    size_t len1 = strlen(path1);
    size_t len2 = strlen(path2);

    // Always use '/' — it works on all platforms (Windows C stdlib accepts '/')
    // and keeps paths consistent with Aether's module system.
    char sep = '/';

    int needs_sep = (len1 > 0 && path1[len1-1] != '/' && path1[len1-1] != '\\');
    size_t total = len1 + len2 + (needs_sep ? 1 : 0);

    char* result = (char*)malloc(total + 1);
    if (!result) return NULL;
    strcpy(result, path1);
    if (needs_sep) {
        result[len1] = sep;
        strcpy(result + len1 + 1, path2);
    } else {
        strcpy(result + len1, path2);
    }

    return result;
}

char* path_dirname(const char* path) {
    if (!path) return NULL;

    const char* last_sep = strrchr(path, '/');
    const char* last_sep_win = strrchr(path, '\\');

    if (last_sep_win && (!last_sep || last_sep_win > last_sep)) {
        last_sep = last_sep_win;
    }

    if (!last_sep) {
        return strdup(".");
    }

    size_t len = last_sep - path;
    if (len == 0) len = 1;  // Root directory

    char* result = (char*)malloc(len + 1);
    if (!result) return NULL;
    memcpy(result, path, len);
    result[len] = '\0';

    return result;
}

char* path_basename(const char* path) {
    if (!path) return NULL;

    const char* last_sep = strrchr(path, '/');
    const char* last_sep_win = strrchr(path, '\\');

    if (last_sep_win && (!last_sep || last_sep_win > last_sep)) {
        last_sep = last_sep_win;
    }

    const char* base = last_sep ? last_sep + 1 : path;
    return strdup(base);
}

char* path_extension(const char* path) {
    if (!path) return NULL;

    const char* last_dot = strrchr(path, '.');
    const char* last_sep = strrchr(path, '/');
    const char* last_sep_win = strrchr(path, '\\');

    if (last_sep_win && (!last_sep || last_sep_win > last_sep)) {
        last_sep = last_sep_win;
    }

    if (!last_dot || (last_sep && last_dot < last_sep)) {
        return strdup("");
    }

    return strdup(last_dot);
}

int path_is_absolute(const char* path) {
    if (!path || path[0] == '\0') return 0;

    // Unix-style absolute: /path (works on all platforms)
    if (path[0] == '/') return 1;

    #ifdef _WIN32
    // Windows: C:\ or C:/ or \\server\share
    if ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) {
        if (path[1] && path[1] == ':' && path[2] && (path[2] == '\\' || path[2] == '/')) {
            return 1;
        }
    }
    if (path[0] == '\\' && path[1] && path[1] == '\\') return 1;
    #endif

    return 0;
}

// Directory listing
DirList* dir_list_raw(const char* path) {
    if (!path) return NULL;

    DirList* list = (DirList*)malloc(sizeof(DirList));
    if (!list) return NULL;
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

    // Capacity-doubling growth for the entries array. Without it every
    // readdir step reallocated + memcpy'd the whole list, turning an
    // N-entry directory into O(N^2) work.
    int cap = 0;
    do {
        if (strcmp(find_data.cFileName, ".") != 0 &&
            strcmp(find_data.cFileName, "..") != 0) {
            if (list->count >= cap) {
                int new_cap = cap ? cap * 2 : 16;
                char** new_entries = (char**)realloc(list->entries, new_cap * sizeof(char*));
                if (!new_entries) break;
                list->entries = new_entries;
                cap = new_cap;
            }
            char* name_copy = strdup(find_data.cFileName);
            if (!name_copy) break;
            list->entries[list->count++] = name_copy;
        }
    } while (FindNextFileA(hFind, &find_data));

    FindClose(hFind);
    #else
    DIR* dir = opendir(path);
    if (!dir) return list;

    int cap = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            if (list->count >= cap) {
                int new_cap = cap ? cap * 2 : 16;
                char** new_entries = (char**)realloc(list->entries, new_cap * sizeof(char*));
                if (!new_entries) break;
                list->entries = new_entries;
                cap = new_cap;
            }
            char* name_copy = strdup(entry->d_name);
            if (!name_copy) break;
            list->entries[list->count++] = name_copy;
        }
    }

    closedir(dir);
    #endif

    return list;
}

int dir_list_count(DirList* list) {
    return list ? list->count : 0;
}

const char* dir_list_get(DirList* list, int index) {
    if (!list || index < 0 || index >= list->count) return NULL;
    return list->entries[index];
}

void dir_list_free(DirList* list) {
    if (!list) return;

    for (int i = 0; i < list->count; i++) {
        free(list->entries[i]);
    }
    free(list->entries);
    free(list);
}

// --- Glob: pattern matching for file discovery ---

#ifndef _WIN32
#include <glob.h>
#include <fnmatch.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

// Helper: add a path to a DirList
static void dirlist_add(DirList* list, const char* path) {
    char** new_entries = (char**)realloc(list->entries, (list->count + 1) * sizeof(char*));
    if (!new_entries) return;
    list->entries = new_entries;
    list->entries[list->count] = strdup(path);
    list->count++;
}

#ifdef _WIN32
// Simple glob-style pattern match for Windows (replaces fnmatch).
// Supports '*' (any sequence) and '?' (any single char).
// When the pattern does NOT start with '.', a leading dot in the name
// is not matched by '*' — mirroring FNM_PERIOD / POSIX semantics.
static int win_fnmatch(const char* pattern, const char* name) {
    const char* p = pattern;
    const char* n = name;
    const char* star_p = NULL;
    const char* star_n = NULL;

    // FNM_PERIOD semantics: if pattern doesn't start with '.',
    // a leading dot in name must not be matched by '*' or '?'.
    if (n[0] == '.' && p[0] != '.') return 0;

    while (*n) {
        if (*p == '*') {
            star_p = ++p;
            star_n = n;
            continue;
        }
        if (*p == '?' || *p == *n) {
            p++;
            n++;
            continue;
        }
        if (star_p) {
            p = star_p;
            n = ++star_n;
            continue;
        }
        return 0;
    }
    while (*p == '*') p++;
    return *p == '\0';
}

// Recursive walk for ** patterns (Windows).
static void walk_recursive(const char* dir, const char* suffix_pattern, DirList* result) {
    char search[MAX_PATH];
    snprintf(search, sizeof(search), "%s\\*", dir);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(search, &fd);
    if (h == INVALID_HANDLE_VALUE) return;

    do {
        // Skip '.' and '..'
        if (fd.cFileName[0] == '.' &&
            (fd.cFileName[1] == '\0' ||
             (fd.cFileName[1] == '.' && fd.cFileName[2] == '\0'))) {
            continue;
        }

        char fullpath[4096];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // Skip dot-prefixed directories (.git, .aeb, .vscode, …)
            if (fd.cFileName[0] == '.') continue;
            walk_recursive(fullpath, suffix_pattern, result);
        } else {
            if (win_fnmatch(suffix_pattern, fd.cFileName)) {
                dirlist_add(result, fullpath);
            }
        }
    } while (FindNextFileA(h, &fd));

    FindClose(h);
}
#else
// Recursive walk for ** patterns (POSIX).
// Skips '.' and '..', and skips dot-prefixed directories (e.g. .git, .aeb)
// from recursion — but matches dot-prefixed FILES against the suffix pattern.
// Without this, patterns like "**/.build.ae" or "**/.*.ae" would never find
// dot-prefixed config files.
static void walk_recursive(const char* dir, const char* suffix_pattern, DirList* result) {
    DIR* d = opendir(dir);
    if (!d) return;

    struct dirent* entry;
    while ((entry = readdir(d)) != NULL) {
        // Always skip '.' and '..'
        if (entry->d_name[0] == '.' &&
            (entry->d_name[1] == '\0' ||
             (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
            continue;
        }

        char fullpath[4096];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, entry->d_name);

        struct stat st;
        if (stat(fullpath, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            // Skip dot-prefixed directories (.git, .aeb, .vscode, …) from
            // recursion. Most build/config systems keep these as opaque
            // metadata, and the existing aeb scan excluded them too.
            if (entry->d_name[0] == '.') continue;
            walk_recursive(fullpath, suffix_pattern, result);
        } else {
            // Match suffix pattern against the file name. Use FNM_PERIOD
            // to require explicit leading-dot matching for dot-prefixed
            // files — that matches POSIX shell-glob expectations and
            // means a pattern like ".*.ae" picks up ".build.ae" while
            // "*.ae" still doesn't.
            if (fnmatch(suffix_pattern, entry->d_name, FNM_PERIOD) == 0) {
                dirlist_add(result, fullpath);
            }
        }
    }
    closedir(d);
}
#endif

DirList* fs_glob_raw(const char* pattern) {
    if (!pattern) return NULL;

    DirList* result = (DirList*)malloc(sizeof(DirList));
    if (!result) return NULL;
    result->entries = NULL;
    result->count = 0;

#ifdef _WIN32
    // Check for ** (recursive glob)
    const char* dstar = strstr(pattern, "/**/");
    if (dstar) {
        char dir[4096];
        int dirlen = (int)(dstar - pattern);
        if (dirlen == 0) {
            strcpy(dir, ".");
        } else {
            strncpy(dir, pattern, dirlen);
            dir[dirlen] = '\0';
        }
        const char* suffix = dstar + 4;  // skip "/**/"

        // Match files directly in the base directory
        char search[MAX_PATH];
        snprintf(search, sizeof(search), "%s\\*", dir);
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(search, &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    if (win_fnmatch(suffix, fd.cFileName)) {
                        char fullpath[4096];
                        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, fd.cFileName);
                        dirlist_add(result, fullpath);
                    }
                }
            } while (FindNextFileA(h, &fd));
            FindClose(h);
        }

        // Recursive walk
        walk_recursive(dir, suffix, result);
    } else {
        // Simple glob (no **)
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(pattern, &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    dirlist_add(result, fd.cFileName);
                }
            } while (FindNextFileA(h, &fd));
            FindClose(h);
        }
    }
#else
    // Check for ** (recursive glob)
    const char* dstar = strstr(pattern, "/**/");
    if (dstar) {
        // Split: prefix is the directory, suffix is the file pattern
        // e.g., "src/**/*.c" → dir="src", suffix="*.c"
        char dir[4096];
        int dirlen = (int)(dstar - pattern);
        if (dirlen == 0) {
            strcpy(dir, ".");
        } else {
            strncpy(dir, pattern, dirlen);
            dir[dirlen] = '\0';
        }
        const char* suffix = dstar + 4;  // skip "/**/"

        // Also match files directly in the base directory
        char direct[8192];
        snprintf(direct, sizeof(direct), "%s/%s", dir, suffix);
        glob_t g;
        if (glob(direct, 0, NULL, &g) == 0) {
            for (size_t i = 0; i < g.gl_pathc; i++) {
                dirlist_add(result, g.gl_pathv[i]);
            }
            globfree(&g);
        }

        // Recursive walk
        walk_recursive(dir, suffix, result);
    } else {
        // Simple glob (no **)
        glob_t g;
        if (glob(pattern, 0, NULL, &g) == 0) {
            for (size_t i = 0; i < g.gl_pathc; i++) {
                dirlist_add(result, g.gl_pathv[i]);
            }
            globfree(&g);
        }
    }
#endif

    return result;
}

// Multi-pattern glob: takes a list of patterns, returns merged DirList.
// The list is an ArrayList (from std.list) containing string pointers.
extern int list_size(void*);
extern void* list_get_raw(void*, int);

DirList* fs_glob_multi_raw(void* pattern_list) {
    if (!pattern_list) return NULL;

    DirList* result = (DirList*)malloc(sizeof(DirList));
    if (!result) return NULL;
    result->entries = NULL;
    result->count = 0;

    int n = list_size(pattern_list);
    for (int i = 0; i < n; i++) {
        const char* pattern = (const char*)list_get_raw(pattern_list, i);
        if (!pattern) continue;

        DirList* partial = fs_glob_raw(pattern);
        if (!partial) continue;

        for (int j = 0; j < partial->count; j++) {
            dirlist_add(result, partial->entries[j]);
        }
        dir_list_free(partial);
    }

    return result;
}

#endif // AETHER_HAS_FILESYSTEM

