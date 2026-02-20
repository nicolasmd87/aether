// Aether Portable Thread Primitives
// Thin compatibility layer that maps the pthreads API to platform-native
// threading on all supported targets:
//
//   POSIX (Linux, macOS)  →  <pthread.h> directly (zero overhead)
//   Windows (MSVC/MinGW)  →  Win32: CRITICAL_SECTION + CONDITION_VARIABLE + FLS
//
// Usage: replace every `#include <pthread.h>` with
//        `#include "path/to/aether_thread.h"`.
//        All pthread_* call sites remain unchanged.
//
// Windows primitives chosen:
//   pthread_mutex_t  → CRITICAL_SECTION  (user-mode, no kernel transition when uncontested)
//   pthread_cond_t   → CONDITION_VARIABLE (Vista+, works natively with CRITICAL_SECTION)
//   pthread_key_t    → FLS (Fiber Local Storage) — unlike TLS, FLS supports per-key
//                       destructors called on thread exit, matching pthreads semantics
//   pthread_t        → HANDLE (from CreateThread)

#ifndef AETHER_THREAD_H
#define AETHER_THREAD_H

#ifndef _WIN32

// ============================================================
// POSIX path — delegate to the real pthreads implementation
// ============================================================
#include <pthread.h>

#else // _WIN32

// ============================================================
// Windows path — implement the pthread API with Win32 primitives
// ============================================================

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdlib.h>    // malloc, free
#include <stdint.h>    // uint64_t
#include <time.h>      // struct timespec (MSVC 2015+)
#include <errno.h>

// ETIMEDOUT / ENOMEM may not be defined on all MSVC runtimes
#ifndef ETIMEDOUT
#  define ETIMEDOUT 138
#endif
#ifndef ENOMEM
#  define ENOMEM 12
#endif

// ---- Types ---------------------------------------------------------------

typedef HANDLE              pthread_t;
typedef CRITICAL_SECTION    pthread_mutex_t;
typedef CONDITION_VARIABLE  pthread_cond_t;
typedef DWORD               pthread_key_t;   // FLS index

// Attribute types — Windows has no equivalents; these are accepted but ignored
typedef int  pthread_attr_t;
typedef int  pthread_mutexattr_t;
typedef int  pthread_condattr_t;

// ---- Threads -------------------------------------------------------------

// Bridge struct: carries the pthread-style fn+arg through CreateThread
typedef struct {
    void* (*fn)(void*);
    void*  arg;
} _AetherThreadBridge;

// Trampoline: Windows calls DWORD WINAPI fn(LPVOID), pthreads calls void* fn(void*)
static DWORD WINAPI _aether_thread_trampoline(LPVOID p) {
    _AetherThreadBridge* b = (_AetherThreadBridge*)p;
    void* (*fn)(void*) = b->fn;
    void* arg          = b->arg;
    free(b);
    fn(arg);
    return 0;
}

static inline int pthread_create(pthread_t* t, pthread_attr_t* attr,
                                  void* (*fn)(void*), void* arg) {
    (void)attr;
    _AetherThreadBridge* b = (malloc)(sizeof(_AetherThreadBridge));
    if (!b) return ENOMEM;
    b->fn  = fn;
    b->arg = arg;
    *t = CreateThread(NULL, 0, _aether_thread_trampoline, b, 0, NULL);
    if (!*t) { free(b); return (int)GetLastError(); }
    return 0;
}

static inline int pthread_join(pthread_t t, void** retval) {
    (void)retval;
    WaitForSingleObject(t, INFINITE);
    CloseHandle(t);
    return 0;
}

// ---- Mutex ---------------------------------------------------------------

static inline int pthread_mutex_init(pthread_mutex_t* m, pthread_mutexattr_t* attr) {
    (void)attr;
    InitializeCriticalSection(m);
    return 0;
}

static inline int pthread_mutex_destroy(pthread_mutex_t* m) {
    DeleteCriticalSection(m);
    return 0;
}

static inline int pthread_mutex_lock(pthread_mutex_t* m) {
    EnterCriticalSection(m);
    return 0;
}

static inline int pthread_mutex_trylock(pthread_mutex_t* m) {
    return TryEnterCriticalSection(m) ? 0 : EBUSY;
}

static inline int pthread_mutex_unlock(pthread_mutex_t* m) {
    LeaveCriticalSection(m);
    return 0;
}

// ---- Condition Variable --------------------------------------------------

static inline int pthread_cond_init(pthread_cond_t* c, pthread_condattr_t* attr) {
    (void)attr;
    InitializeConditionVariable(c);
    return 0;
}

static inline int pthread_cond_destroy(pthread_cond_t* c) {
    (void)c;   // CONDITION_VARIABLE needs no explicit cleanup on Windows
    return 0;
}

static inline int pthread_cond_signal(pthread_cond_t* c) {
    WakeConditionVariable(c);
    return 0;
}

static inline int pthread_cond_broadcast(pthread_cond_t* c) {
    WakeAllConditionVariable(c);
    return 0;
}

static inline int pthread_cond_wait(pthread_cond_t* c, pthread_mutex_t* m) {
    SleepConditionVariableCS(c, m, INFINITE);
    return 0;
}

// pthread_cond_timedwait: abstime is absolute CLOCK_REALTIME (seconds + nanoseconds
// since the Unix epoch 1970-01-01). Windows SleepConditionVariableCS takes a
// relative timeout in milliseconds. We convert by:
//   1. Read current wall clock via GetSystemTimeAsFileTime (100ns ticks since 1601-01-01)
//   2. Subtract the Windows-to-Unix epoch offset (116444736000000000 * 100ns)
//   3. Compute relative ms = abstime_ms - now_ms (clamped to 0)
static inline int pthread_cond_timedwait(pthread_cond_t* c, pthread_mutex_t* m,
                                          const struct timespec* abstime) {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER now_100ns;
    now_100ns.LowPart  = ft.dwLowDateTime;
    now_100ns.HighPart = ft.dwHighDateTime;
    // Convert Windows epoch (1601) → Unix epoch (1970): subtract 116444736000000000 * 100ns
    uint64_t now_ms  = now_100ns.QuadPart / 10000ULL - 11644473600000ULL;
    uint64_t abs_ms  = (uint64_t)abstime->tv_sec  * 1000ULL
                     + (uint64_t)abstime->tv_nsec  / 1000000ULL;
    DWORD timeout_ms = (abs_ms > now_ms) ? (DWORD)(abs_ms - now_ms) : 0;
    BOOL ok = SleepConditionVariableCS(c, m, timeout_ms);
    return ok ? 0 : ETIMEDOUT;
}

// ---- Thread-Local Storage (via Fiber Local Storage) ----------------------
// FLS is used instead of TLS because FlsAlloc() accepts a per-key destructor
// called on thread exit — exactly matching pthread_key_create() semantics.
// TlsAlloc() has no destructor support.

static inline int pthread_key_create(pthread_key_t* key, void (*destructor)(void*)) {
    *key = FlsAlloc((PFLS_CALLBACK_FUNCTION)destructor);
    return (*key == FLS_OUT_OF_INDEXES) ? ENOMEM : 0;
}

static inline int pthread_key_delete(pthread_key_t key) {
    return FlsFree(key) ? 0 : (int)GetLastError();
}

static inline int pthread_setspecific(pthread_key_t key, const void* value) {
    return FlsSetValue(key, (PVOID)(uintptr_t)value) ? 0 : (int)GetLastError();
}

static inline void* pthread_getspecific(pthread_key_t key) {
    return FlsGetValue(key);
}

// ---- sched_yield compat --------------------------------------------------
// On POSIX, sched_yield() lives in <sched.h>.
// On Windows the equivalent is SwitchToThread() from <windows.h>.
static inline int sched_yield(void) {
    SwitchToThread();
    return 0;
}

#endif // _WIN32

#endif // AETHER_THREAD_H
