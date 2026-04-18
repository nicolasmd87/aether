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

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* AETHER_HOST_H */
