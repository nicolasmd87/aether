/*
 * aether_host.h — Host-callback primitives for Aether scripts compiled
 * with `aetherc --emit=lib` and embedded in a host application.
 *
 * The pattern is the EAI / Hohpe "claim check": Aether scripts emit
 * thin notifications carrying only an event name and an int64 ID; the
 * host receives those via registered handlers and calls back into the
 * script through normal typed downcalls if it wants the detail.
 *
 * Threading: single-threaded, synchronous. notify() invokes the
 * registered handler (if any) on whatever thread is currently running
 * Aether code; the handler runs to completion before notify() returns.
 * Multi-threaded hosts must serialize their access to a given Aether
 * library handle externally.
 *
 * Wiring: the host links against the runtime, calls
 * aether_event_register() before invoking any aether_<name>() entry
 * point, and the script's notify() calls end up in the registered
 * handler. The v2 namespace generator wraps this in per-namespace
 * shims so the host SDK feels namespace-scoped.
 */

#ifndef AETHER_HOST_H
#define AETHER_HOST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Handler signature. The id is whatever the script passed to
 * notify(). Return value (if any) is ignored by the runtime. */
typedef void (*aether_event_handler_t)(int64_t id);

/* Register a handler for the named event. Replaces any prior handler
 * for the same name. Returns 0 on success, -1 if the registry is
 * full (compile-time cap, see AETHER_HOST_MAX_EVENTS in the .c file). */
int aether_event_register(const char* event_name, aether_event_handler_t handler);

/* Remove a handler for the named event. Returns 0 if removed, -1 if
 * no such handler was registered. */
int aether_event_unregister(const char* event_name);

/* Drop all registered handlers. Useful between test cases or when a
 * host shuts down a session and starts a fresh one. */
void aether_event_clear(void);

/* The notify() function called from generated Aether code. Returns 1
 * if a handler was found and invoked, 0 if no listener was registered
 * for that event name. NULL event_name returns 0. */
int notify(const char* event_name, int64_t id);

/* ---------------------------------------------------------------------
 * Manifest registry — populated when a manifest.ae script runs under
 * the namespace compile pipeline. The std.host module exposes builder
 * functions (namespace(), input(), event(), bindings { java(...) })
 * that call the namespace_*, input, event, binding_* externs declared
 * here. The pipeline reads the captured manifest via aether_manifest()
 * after the script returns.
 *
 * The registry is process-global static state, intentionally simple:
 * one manifest per process at a time. The pipeline calls
 * aether_manifest_clear() before running each manifest.ae.
 * --------------------------------------------------------------------- */

typedef struct AetherInputDecl {
    const char* name;
    const char* type_signature;
} AetherInputDecl;

typedef struct AetherEventDecl {
    const char* name;
    const char* carries_type;
} AetherEventDecl;

typedef struct AetherJavaBinding {
    const char* package_name;   /* may be NULL if no java { } declared */
    const char* class_name;
} AetherJavaBinding;

typedef struct AetherPythonBinding {
    const char* module_name;    /* may be NULL */
} AetherPythonBinding;

typedef struct AetherRubyBinding {
    const char* module_name;    /* may be NULL */
} AetherRubyBinding;

typedef struct AetherGoBinding {
    const char* package_name;   /* may be NULL */
} AetherGoBinding;

#ifndef AETHER_MANIFEST_MAX_INPUTS
#define AETHER_MANIFEST_MAX_INPUTS 64
#endif
#ifndef AETHER_MANIFEST_MAX_EVENTS
#define AETHER_MANIFEST_MAX_EVENTS 64
#endif

typedef struct AetherManifest {
    const char* namespace_name;     /* set by namespace_begin */
    int input_count;
    AetherInputDecl inputs[AETHER_MANIFEST_MAX_INPUTS];
    int event_count;
    AetherEventDecl events[AETHER_MANIFEST_MAX_EVENTS];
    AetherJavaBinding   java;
    AetherPythonBinding python;
    AetherRubyBinding   ruby;
    AetherGoBinding     go;
} AetherManifest;

/* Manifest builders — called from std/host/module.ae via externs.
 *
 * Each takes `void* _ctx` as its first parameter to satisfy the
 * codegen's auto-_ctx-injection rule for builder functions inside a
 * trailing block. The _ctx is unused — the manifest registry is
 * process-global state, written in declaration order. This shape lets
 * authors write:
 *
 *     abi() {
 *         describe("trading") {
 *             input("port", "int")
 *             bindings() { java("com.example", "Trading") }
 *         }
 *     }
 *
 * matching Aether's existing builder DSL idiom (see contrib/tinyweb,
 * examples/calculator-tui).
 *
 * These names (describe, event, input, java, ...) are short and could
 * collide with user code in the embedding host's C compilation unit.
 * The collision risk is low because hosts typically dlopen the library
 * rather than statically link it. If a host needs to statically link
 * and these names collide, wrap the library .o behind a translation
 * unit that doesn't import these names directly. */
void describe(void* _ctx, const char* name);
void input(void* _ctx, const char* name, const char* type_signature);
void event(void* _ctx, const char* name, const char* carries_type);
void bindings(void* _ctx);
void java(void* _ctx, const char* package_name, const char* class_name);
void python(void* _ctx, const char* module_name);
void ruby(void* _ctx, const char* module_name);
void go(void* _ctx, const char* package_name);

/* Read the captured manifest. Returns a borrowed pointer to the
 * process-global state — DO NOT free. Returns NULL if no manifest
 * has been declared in this process. */
AetherManifest* manifest_get(void);

/* Drop the captured manifest (zeroes input/event arrays, NULLs the
 * binding strings). Used by tests and the pipeline. Strings stored in
 * the manifest are borrowed pointers; clearing them just resets our
 * own bookkeeping — we don't free what we don't own. */
void manifest_clear(void);

/* ---------------------------------------------------------------------
 * Discovery
 *
 * AetherNamespaceManifest is the layout `aether_describe()` returns
 * from a namespace .so built by `ae build --namespace`. It's the same
 * shape as AetherManifest above — both describe the same data — but
 * AetherNamespaceManifest is statically embedded in each namespace .so
 * at compile time, while AetherManifest is the runtime mutable
 * registry the std.host builders write into.
 *
 * Hosts call aether_describe() to introspect the loaded library:
 *   - Verify the namespace name matches what they expect at startup
 *   - Walk inputs to know what set<X>() methods to expose
 *   - Walk events to know what on<Y>() listeners to wire up
 *   - Read the bindings struct to confirm the SDK they're using
 *     was generated for the same namespace
 *
 * The struct layout MUST stay binary-compatible with the static-init
 * the aetherc --emit-namespace-describe stub generates (see
 * compiler/aetherc.c::emit_describe_c). If you reorder fields, update
 * both sides at once.
 * --------------------------------------------------------------------- */

typedef struct AetherNamespaceManifest {
    const char* namespace_name;
    int input_count;
    AetherInputDecl inputs[AETHER_MANIFEST_MAX_INPUTS];
    int event_count;
    AetherEventDecl events[AETHER_MANIFEST_MAX_EVENTS];
    AetherJavaBinding   java;
    AetherPythonBinding python;
    AetherRubyBinding   ruby;
    AetherGoBinding     go;
} AetherNamespaceManifest;

/* The discovery entry point. Defined by the auto-generated stub
 * inside each namespace .so; declared here so hosts can include this
 * header and call it via dlsym. Returns a borrowed pointer to a
 * static — DO NOT free. */
const AetherNamespaceManifest* aether_describe(void);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* AETHER_HOST_H */
