# Per-process configuration in Aether

Many CLI tools and servers need a per-process key/value: parse it once at startup (from `--flag X` args, an env var, or a config file), then read it from many call sites for the rest of the process's life. Examples:

- `--superuser-token X` parsed at CLI entry, checked on every HTTP request handler.
- `--log-level debug` set once, consulted by every log call.
- `--data-dir /var/lib/myapp` baked in at startup, used by every filesystem op.
- API keys, tenant IDs, current-user identity stashed by an auth middleware.

In C this is a `static char *g_thing = NULL;` plus `set_thing(s)` / `get_thing(void)` helpers. Aether **does not allow mutable assignment to module-level identifiers** (only `const` works there, which can't be updated by a setter), so the same shape doesn't transcribe directly. The canonical pattern is the **actor-as-singleton**: spawn one actor at startup, set values via messages, read via messages.

This page is the worked example. It's a doc, not a stdlib feature — there's nothing to import beyond the language primitives.

## The pattern

```aether
import std.string

// Messages that drive the config actor.
message SetUser   { name: string }
message SetToken  { value: string }
// Reads route through a reply-bearing message: include a reply
// actor_ref so the config actor can send the value back.
message GetUser   { to: actor_ref }
message GetToken  { to: actor_ref }
message UserReply { value: string }
message TokenReply { value: string }

// The singleton. State-only — no business logic, just a key/value
// holder. Spawn exactly one of these.
actor ConfigActor {
    state user  = ""
    state token = ""

    receive {
        SetUser(name)   -> { user = name }
        SetToken(value) -> { token = value }
        GetUser(to)     -> { to ! UserReply  { value: user  } }
        GetToken(to)    -> { to ! TokenReply { value: token } }
    }
}

main() {
    cfg = spawn(ConfigActor())

    // Write-once at startup.
    cfg ! SetUser  { name: "alice" }
    cfg ! SetToken { value: "secret-xyz" }

    // … later, in a request handler that holds an `actor_ref` to its
    // own reply target …
    // cfg ! GetUser { to: self_ref }
    // receive { UserReply(value) -> { … use value … } }
}
```

The reply pattern (caller sends a `Get*` message containing its own actor_ref, then `receive`s the matching `*Reply`) is the same ergonomics Erlang's `gen_server:call` provides under the hood. It's verbose for one-shot reads; if you only need writes, drop the `Get*` / `*Reply` half and design your readers to take the value from a message they were sent at startup.

## Variants

**Write-once, read-many config** (the most common shape — log level, data dir, auth token):

The shape above is what you want. The actor's mailbox serializes writes with reads, so concurrent setters and getters can't interleave incorrectly. If you only set at startup and never again, the cost is one message round-trip per read.

**Hot-path reads where actor message overhead is too much**:

Measure first. A same-core actor message under Aether's lock-free SPSC scheduler is ~80 ns on commodity hardware (see [runtime-optimizations.md](runtime-optimizations.md)). For a per-HTTP-request auth-token check this is in the noise compared to the cost of the request itself.

If you've measured and the round-trip really is too expensive (e.g. an inner loop reading the value 10⁹ times), the alternatives in order of preference:

1. **Pass the config value through your call chain.** Plumb it as a parameter from the place that has the singleton's value to the place that needs it. This is the "explicit dependency" approach and is usually the right answer.

2. **Cache the value in a local actor's state.** A worker actor that handles requests can ask the config actor once on startup and store the value in its own `state token = "…"`. Each subsequent handler invocation reads the local state — no round-trip. The trade-off is staleness if the config can change after the worker initialized.

3. **Use a `const` if the value is genuinely immutable across the process's life.** `const TOKEN = "secret-xyz"` lowers to a `#define` and reads are zero-cost. Only works when the value is known at compile time (so not for CLI-parsed config).

**Multi-tenant / per-request state**:

Per-process config is the wrong shape for things that vary per request (current user, tenant ID, request ID). For those, use thread-local state (TLS) or pass the value explicitly through your handler chain — the actor singleton would serialize all handlers behind one mailbox, which defeats parallelism.

## Why not module-level `var`?

Aether deliberately rejects `var` at module scope. The reasons:

- **Memory model.** Module-level mutable state with concurrent access requires explicit synchronisation (locks, atomics) for correctness; the actor model lets the runtime handle that for you. A bare `static char *g_user;` in a multi-threaded server is a TOCTOU bug waiting to bite.
- **Testability.** Process-global state is hostile to tests — you can't run two test cases in parallel with different configurations without process isolation. Actor state can be reset by re-spawning.
- **One escape hatch is enough.** `const` covers the compile-time-known case; the actor pattern covers everything else.

If you find yourself wanting `var` at module scope, the question to ask is: does this state need to be visible across actor boundaries? If yes, model it as an actor. If no, it should live in the actor that owns the work.

## Worked example: replacing a C global-state shim

The svn-aether port has this in `subversion/ae/ra/shim.c`:

```c
static char *g_client_user = NULL;

void svnae_ra_set_user(const char *user) {
    free(g_client_user);
    g_client_user = user && *user ? strdup(user) : NULL;
}

const char *aether_ra_get_user(void) {
    return g_client_user ? g_client_user : "";
}
```

The Aether replacement is the actor pattern above. Set on CLI parse:

```aether
cfg ! SetUser { name: parsed_user_arg }
```

Read in the request handler — slightly more involved than a function call because the handler needs an actor_ref to receive the reply. The natural place to plumb that is through whatever spawns the handler:

```aether
// At handler-spawn time:
worker = spawn(RequestHandler())
cfg ! GetUser { to: worker }   // worker stashes the reply when it arrives

// In the worker's receive block:
receive {
    UserReply(value) -> { current_user = value }
    HttpRequest(req) -> { handle_request(req, current_user) }
}
```

This is more code than the C version, but it's also testable, thread-safe by construction, and amenable to "swap the config actor for a mock" in tests.

## See also

- [actor-concurrency.md](actor-concurrency.md) — runtime details on the scheduler, mailboxes, and message delivery
- [runtime-optimizations.md](runtime-optimizations.md) — message-passing performance numbers
- `tests/integration/test_http_client_v2.ae` — uses an actor singleton (`SrvActor`) for an in-process HTTP server's lifecycle
