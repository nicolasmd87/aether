// contrib.host.tinygo — in-process Go via TinyGo c-shared.
//
// Unlike contrib.host.go, which spawns a subprocess to run Go code,
// this host loads a TinyGo-built shared library
// (`tinygo build -buildmode=c-shared -o lib.so script.go`) into the
// Aether process via std.dl and invokes its exported functions
// directly — no subprocess, no IPC, no JSON marshalling.
//
// Calling-convention surface for v1: pre-defined wrapper signatures
// for the most common shapes. Adding a new shape is a one-line
// extension; libffi-style fully dynamic dispatch is intentionally
// deferred (libffi adds a system dependency that 95% of users do
// not need).
//
// Wrapper signatures shipped:
//
//   tinygo_call_int_void          int   fn(void)
//   tinygo_call_int_int           int   fn(int)
//   tinygo_call_int_int_int       int   fn(int, int)
//   tinygo_call_void_int          void  fn(int)
//   tinygo_call_str_str           str   fn(const char*)
//
// All signatures take (void* handle, const char* sym_name, ...args)
// and look up `sym_name` via `aether_dl_symbol_raw(handle, sym_name)`
// before invoking. Errors (NULL handle, missing symbol, NULL pointer
// invocation) are surfaced via `aether_host_tinygo_last_error()`.
#ifndef AETHER_HOST_TINYGO_H
#define AETHER_HOST_TINYGO_H

#include <stddef.h>

// Last-error string for the calling thread. Returns "" if no error
// is pending. Pointer ownership stays with the host module.
const char* aether_host_tinygo_last_error(void);

// int fn(void)
int tinygo_call_int_void(void* handle, const char* sym);

// int fn(int)
int tinygo_call_int_int(void* handle, const char* sym, int a);

// int fn(int, int)
int tinygo_call_int_int_int(void* handle, const char* sym, int a, int b);

// void fn(int)
void tinygo_call_void_int(void* handle, const char* sym, int a);

// const char* fn(const char*) — TinyGo //export functions returning
// `string` mangle to `const char*` in the c-shared header. Returned
// pointer ownership follows TinyGo's rule (callee-owned, valid until
// the next call or library unload).
const char* tinygo_call_str_str(void* handle, const char* sym, const char* a);

#endif
