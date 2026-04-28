// std.dl - cross-platform dynamic library loader.
//
// Wraps dlopen/dlsym/dlclose (POSIX) and LoadLibraryA/GetProcAddress/
// FreeLibrary (Windows) behind a uniform C API. The Aether-side
// std/dl/module.ae layers a Go-style (handle, err) ergonomics over
// these primitives.
//
// Sandbox interaction: on POSIX, libaether_sandbox_preload.c intercepts
// real dlopen and consults the per-process grant table — std.dl users
// inherit those grants transparently. Windows has no sandbox preload
// today; LoadLibraryA runs unrestricted.
#ifndef AETHER_DL_H
#define AETHER_DL_H

// Open a shared library. Returns NULL on failure; check
// aether_dl_last_error_raw() for the reason.
//
// `path` is platform-native: ".so" on Linux/BSD, ".dylib" on macOS,
// ".dll" on Windows. Callers are expected to pick the right suffix
// (the std/dl Aether wrapper does not auto-mangle).
void* aether_dl_open_raw(const char* path);

// Look up a named symbol in a previously opened handle. Returns NULL
// on failure (consult aether_dl_last_error_raw()).
//
// On POSIX symbols are returned verbatim; on Windows they are looked
// up via GetProcAddress (no name decoration adjustment). C-shared
// callers (e.g. tinygo build -buildmode=c-shared) export plain names.
void* aether_dl_symbol_raw(void* handle, const char* name);

// Close a previously opened handle. Returns 1 on success, 0 on
// failure. Closing a NULL handle is a no-op that returns 1.
int aether_dl_close_raw(void* handle);

// Return the last-error string for the calling thread. The returned
// pointer is owned by std.dl and remains valid until the next std.dl
// call on this thread. Returns "" if there is no pending error.
const char* aether_dl_last_error_raw(void);

#endif
