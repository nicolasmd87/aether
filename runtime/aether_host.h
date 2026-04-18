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
    AetherGoBinding     go;
} AetherManifest;

/* Manifest builders — called from std/host/module.ae via externs.
 *
 * These names are aggressive-looking (notify, event, input, namespace,
 * java, ...) and could collide with user code in the embedding host's C
 * compilation unit. The collision risk is low because:
 *   - The host typically dlopens the library rather than statically
 *     linking it, so namespace pollution is per-library not per-process.
 *   - Hosts that statically link can wrap the library in a translation
 *     unit that doesn't import these names directly.
 * If collisions become a problem, a future version can prefix everything
 * with `aether_` and have std.host's externs use the prefixed names. */
void namespace(const char* name);
void namespace_end(void);
void input(const char* name, const char* type_signature);
void event(const char* name, const char* carries_type);
void bindings(void);                                              /* visual no-op */
void java(const char* package_name, const char* class_name);
void python(const char* module_name);
void go(const char* package_name);

/* Read the captured manifest. Returns a borrowed pointer to the
 * process-global state — DO NOT free. Returns NULL if no manifest
 * has been declared in this process. */
AetherManifest* manifest_get(void);

/* Drop the captured manifest (zeroes input/event arrays, NULLs the
 * binding strings). Used by tests and the pipeline. Strings stored in
 * the manifest are borrowed pointers; clearing them just resets our
 * own bookkeeping — we don't free what we don't own. */
void manifest_clear(void);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* AETHER_HOST_H */
