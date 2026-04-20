// aether_host_tcl.c — Embedded Tcl Language Host Module
//
// Embeds libtcl in the Aether process. When run_sandboxed is called,
// installs the Aether sandbox checker so Tcl's libc calls (open,
// getenv, socket, exec) are intercepted and checked against the
// grant list.

#include "aether_host_tcl.h"
#include "../../../runtime/aether_sandbox.h"
#include "../../../runtime/aether_shared_map.h"

#ifdef AETHER_HAS_TCL
#include <tcl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Tcl_Interp* T = NULL;

// Bridge-owned permission stack. Each run_sandboxed call pushes its
// perms list here before installing host_tcl_checker, and pops on
// exit. Self-contained — does not poke at the compiler-emitted
// preamble's static _aether_ctx_stack (which is not cross-file
// visible anyway). Depth 64 matches the preamble's own cap.
static void* tcl_perms_stack[64];
static int   tcl_perms_depth = 0;

extern int list_size(void*);
extern void* list_get(void*, int);

static int pattern_match(const char* pat, const char* resource) {
    // Normalize IPv4-mapped IPv6 addresses so a grant for "10.0.0.1"
    // matches a TCP resource reported as "::ffff:10.0.0.1" (and
    // vice versa). Safe for non-TCP categories because "::ffff:"
    // doesn't appear in filesystem paths, env var names, or exec
    // command strings.
    if (pat && strncmp(pat, "::ffff:", 7) == 0) pat += 7;
    if (resource && strncmp(resource, "::ffff:", 7) == 0) resource += 7;
    int plen = strlen(pat);
    int rlen = strlen(resource);
    if (plen == 1 && pat[0] == '*') return 1;
    if (plen > 1 && pat[plen-1] == '*') {
        if (strncmp(pat, resource, plen-1) == 0) return 1;
    }
    if (plen > 1 && pat[0] == '*') {
        int slen = plen - 1;
        if (rlen >= slen && strcmp(resource + rlen - slen, pat + 1) == 0) return 1;
    }
    return strcmp(pat, resource) == 0;
}

static int perms_allow(void* ctx, const char* category, const char* resource) {
    if (!ctx) return 1;
    int n = list_size(ctx);
    if (n == 0) return 0;
    for (int i = 0; i < n; i += 2) {
        const char* cat = (const char*)list_get(ctx, i);
        const char* pat = (const char*)list_get(ctx, i + 1);
        if (!cat || !pat) continue;
        if (cat[0] == '*' && pat[0] == '*') return 1;
        if (strcmp(cat, category) == 0 && pattern_match(pat, resource)) return 1;
    }
    return 0;
}

static int host_tcl_checker(const char* category, const char* resource) {
    if (tcl_perms_depth <= 0) return 1;
    for (int level = 0; level < tcl_perms_depth; level++) {
        if (!perms_allow(tcl_perms_stack[level], category, resource)) return 0;
    }
    return 1;
}

int tcl_init(void) {
    if (T) return 0;
    Tcl_FindExecutable(NULL);
    T = Tcl_CreateInterp();
    if (!T) return -1;
    if (Tcl_Init(T) != TCL_OK) {
        fprintf(stderr, "[tcl] init: %s\n", Tcl_GetStringResult(T));
        Tcl_DeleteInterp(T);
        T = NULL;
        return -1;
    }
    return 0;
}

void tcl_finalize(void) {
    if (T) {
        Tcl_DeleteInterp(T);
        T = NULL;
    }
    Tcl_Finalize();
}

int tcl_run(const char* code) {
    if (!code) return -1;
    if (tcl_init() != 0) return -1;
    if (Tcl_Eval(T, code) != TCL_OK) {
        fprintf(stderr, "[tcl] %s\n", Tcl_GetStringResult(T));
        return -1;
    }
    return 0;
}

int tcl_run_sandboxed(void* perms, const char* code) {
    if (!code) return -1;
    if (tcl_init() != 0) return -1;
    if (tcl_perms_depth >= 64) return -1;

    // Push perms and install checker.
    tcl_perms_stack[tcl_perms_depth++] = perms;
    aether_sandbox_check_fn prev = _aether_sandbox_checker;
    _aether_sandbox_checker = host_tcl_checker;

    int result = 0;
    if (Tcl_Eval(T, code) != TCL_OK) {
        fprintf(stderr, "[tcl] %s\n", Tcl_GetStringResult(T));
        result = -1;
    }

    // Restore.
    _aether_sandbox_checker = prev;
    tcl_perms_depth--;

    return result;
}

// --- Shared map native bindings for Tcl ---

static uint64_t current_map_token = 0;

// aether_map_get KEY → value string or empty
static int tcl_aether_map_get(ClientData cd, Tcl_Interp* interp,
                               int objc, Tcl_Obj* const objv[]) {
    (void)cd;
    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "key");
        return TCL_ERROR;
    }
    const char* key = Tcl_GetString(objv[1]);
    const char* val = aether_shared_map_get_by_token(current_map_token, key);
    if (val) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(val, -1));
    } else {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("", 0));
    }
    return TCL_OK;
}

// aether_map_put KEY VALUE
static int tcl_aether_map_put(ClientData cd, Tcl_Interp* interp,
                               int objc, Tcl_Obj* const objv[]) {
    (void)cd;
    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "key value");
        return TCL_ERROR;
    }
    const char* key = Tcl_GetString(objv[1]);
    const char* val = Tcl_GetString(objv[2]);
    aether_shared_map_put_by_token(current_map_token, key, val);
    return TCL_OK;
}

static void register_map_bindings(Tcl_Interp* interp) {
    Tcl_CreateObjCommand(interp, "aether_map_get", tcl_aether_map_get, NULL, NULL);
    Tcl_CreateObjCommand(interp, "aether_map_put", tcl_aether_map_put, NULL, NULL);
}

int tcl_run_sandboxed_with_map(void* perms, const char* code, uint64_t map_token) {
    if (!code) return -1;
    if (tcl_init() != 0) return -1;
    if (tcl_perms_depth >= 64) return -1;
    register_map_bindings(T);

    extern void aether_shared_map_freeze_inputs_by_token(uint64_t);
    aether_shared_map_freeze_inputs_by_token(map_token);

    current_map_token = map_token;

    tcl_perms_stack[tcl_perms_depth++] = perms;
    aether_sandbox_check_fn prev = _aether_sandbox_checker;
    _aether_sandbox_checker = host_tcl_checker;

    int result = 0;
    if (Tcl_Eval(T, code) != TCL_OK) {
        fprintf(stderr, "[tcl] %s\n", Tcl_GetStringResult(T));
        result = -1;
    }

    _aether_sandbox_checker = prev;
    tcl_perms_depth--;
    current_map_token = 0;

    return result;
}

#else
#include <stdio.h>
int tcl_init(void) {
    fprintf(stderr, "error: contrib.host.tcl not available (compile with AETHER_HAS_TCL)\n");
    return -1;
}
void tcl_finalize(void) {}
int tcl_run(const char* code) { (void)code; return tcl_init(); }
int tcl_run_sandboxed(void* perms, const char* code) {
    (void)perms; (void)code; return tcl_init();
}
int tcl_run_sandboxed_with_map(void* perms, const char* code, uint64_t token) {
    (void)perms; (void)code; (void)token; return tcl_init();
}
#endif
