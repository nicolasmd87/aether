#ifndef AETHER_ACTOR_REGISTRY_H
#define AETHER_ACTOR_REGISTRY_H

#include <stddef.h>

/* std.actor — process-global name → actor_ref registry.
 *
 * Models BEAM's `erlang:register(Name, Pid)` / `erlang:whereis(Name)`:
 * a way to find an actor from any context, including from inside
 * `@c_callback` handlers where Aether-typed parameters can't flow
 * through the C void* user_data slot.
 *
 *     // At startup:
 *     a = spawn(Auditor())
 *     register(:auditor, a)            // namespaced as actor.register
 *
 *     // Anywhere later, including from C-callback handlers:
 *     a = whereis(:auditor)
 *     a ! Analyze { ... }
 *
 * The registry holds an opaque void* per name — at the C level
 * `actor_ref` lowers to `void*`, so this is exactly the right shape
 * to round-trip through the registry without coercion.
 *
 * Thread-safe via the same rwlock pattern as std.config. The
 * registry stays open for the process's lifetime; explicit
 * unregister is provided but rarely needed (actor liveness is
 * tracked elsewhere — putting a stale ref isn't a memory-safety
 * issue, just a logic error in the caller).
 *
 * Section 2 of nuther-ask-of-aether-team.md / Nico's reply. */

/* Bind `name` → `ref`. Overwrites any prior binding. NULL name or
 * NULL ref is a no-op (matches stdlib's "absent ≡ NULL" convention).
 * The name is duplicated; the actor_ref is stored as-is. */
void aether_actor_register(const char* name, void* ref);

/* Look up by name. Returns the registered actor_ref, or NULL if no
 * binding exists. Reads are concurrent (no lock contention with
 * other readers). */
void* aether_actor_whereis(const char* name);

/* Remove a binding. Returns 1 if a binding was removed, 0 if `name`
 * wasn't registered. */
int aether_actor_unregister(const char* name);

/* Membership test. 1 if `name` is bound, 0 otherwise. */
int aether_actor_is_registered(const char* name);

/* Number of currently-registered names. Mostly for tests / debug. */
int aether_actor_registry_size(void);

/* Wipe all bindings. Tests use this for isolation. */
void aether_actor_registry_clear(void);

#endif /* AETHER_ACTOR_REGISTRY_H */
