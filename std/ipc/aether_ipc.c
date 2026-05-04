/* aether_ipc.c — child-side IPC primitives.
 *
 * Companion to std.os.run_pipe (parent side). Provides the
 * function the child calls to discover the back-channel fd that
 * the parent opened.
 *
 * Mechanism: the parent sets AETHER_IPC_FD=<n> in the child's
 * environment before exec, and dup2's the pipe's write end to
 * fd <n>. Child reads getenv("AETHER_IPC_FD"), parses to int,
 * verifies the fd is open writable. Returns fd or -1.
 *
 * The env-var-as-primary design (rather than hardcoded fd 3)
 * sidesteps fd-allocation surprises in shell intermediaries
 * (e.g. `bash -c '<driver>'` between aeb and the test driver).
 *
 * v1: child-only surface. send / send_close are Aether-side
 * wrappers around std.net.fd_write / std.net.fd_close, declared
 * in std/ipc/module.ae. */

#include "../../runtime/config/aether_optimization_config.h"

#include <stdlib.h>
#include <string.h>

#if !AETHER_HAS_FILESYSTEM
int ipc_parent_channel_raw(void) { return -1; }
#else

#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>

int ipc_parent_channel_raw(void) {
    const char* s = getenv("AETHER_IPC_FD");
    if (!s || !s[0]) return -1;
    char* end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0') return -1;
    if (v < 0 || v > 65535) return -1;
    int fd = (int)v;
    /* Verify the fd is open. fcntl(F_GETFD) returns -1 with
     * errno=EBADF if not. */
    if (fcntl(fd, F_GETFD) < 0) return -1;
    return fd;
}

#else  /* _WIN32 */

int ipc_parent_channel_raw(void) {
    /* Windows handle inheritance for fd-mapped pipes is
     * non-trivial (STARTUPINFOEX + PROC_THREAD_ATTRIBUTE_HANDLE_LIST
     * + _open_osfhandle on both sides). Even if AETHER_IPC_FD is
     * set, there's no portable way to verify the descriptor is
     * open as a pipe via the Win32 CRT layer. v1 ships POSIX-only;
     * Windows consumers fall back to file-marker patterns. */
    return -1;
}

#endif

#endif /* AETHER_HAS_FILESYSTEM */
