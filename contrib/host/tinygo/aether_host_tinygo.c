// contrib.host.tinygo — in-process Go via TinyGo c-shared.
// See aether_host_tinygo.h for the API contract.
#include "aether_host_tinygo.h"

#include <stdio.h>
#include <string.h>

#include "../../../runtime/utils/aether_compiler.h"  // AETHER_TLS
#include "../../../std/dl/aether_dl.h"

#define AETHER_HOST_TINYGO_ERR_CAP 512

static AETHER_TLS char g_tg_last_err[AETHER_HOST_TINYGO_ERR_CAP];

static void tg_clear_error(void) {
    g_tg_last_err[0] = '\0';
}

static void tg_set_error(const char* fmt, const char* arg) {
    snprintf(g_tg_last_err, sizeof(g_tg_last_err), fmt, arg ? arg : "");
}

static void* tg_resolve(void* handle, const char* sym) {
    tg_clear_error();
    if (handle == NULL) {
        tg_set_error("tinygo: handle is NULL%s", "");
        return NULL;
    }
    if (sym == NULL || sym[0] == '\0') {
        tg_set_error("tinygo: symbol name is empty%s", "");
        return NULL;
    }
    void* p = aether_dl_symbol_raw(handle, sym);
    if (p == NULL) {
        // Surface the underlying dl error verbatim.
        snprintf(g_tg_last_err, sizeof(g_tg_last_err),
                 "tinygo: %s: %s", sym, aether_dl_last_error_raw());
        return NULL;
    }
    return p;
}

const char* aether_host_tinygo_last_error(void) {
    return g_tg_last_err;
}

int tinygo_call_int_void(void* handle, const char* sym) {
    void* p = tg_resolve(handle, sym);
    if (p == NULL) return 0;
    int (*fn)(void) = (int (*)(void))p;
    return fn();
}

int tinygo_call_int_int(void* handle, const char* sym, int a) {
    void* p = tg_resolve(handle, sym);
    if (p == NULL) return 0;
    int (*fn)(int) = (int (*)(int))p;
    return fn(a);
}

int tinygo_call_int_int_int(void* handle, const char* sym, int a, int b) {
    void* p = tg_resolve(handle, sym);
    if (p == NULL) return 0;
    int (*fn)(int, int) = (int (*)(int, int))p;
    return fn(a, b);
}

void tinygo_call_void_int(void* handle, const char* sym, int a) {
    void* p = tg_resolve(handle, sym);
    if (p == NULL) return;
    void (*fn)(int) = (void (*)(int))p;
    fn(a);
}

const char* tinygo_call_str_str(void* handle, const char* sym, const char* a) {
    void* p = tg_resolve(handle, sym);
    if (p == NULL) return "";
    const char* (*fn)(const char*) = (const char* (*)(const char*))p;
    return fn(a);
}
