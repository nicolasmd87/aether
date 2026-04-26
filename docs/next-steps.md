# Next Steps

Planned features and improvements for upcoming Aether releases.

> See [CHANGELOG.md](../CHANGELOG.md) for what shipped in each release.

## Language Features

### Structured Concurrency — supervision trees + capability-scoped spawn/send

Actors exist, but when a handler panics nobody finds out. And `hide` /
`seal except` constrain variable reads but not `spawn` / `!` / `ask`.
Both gaps have the same fix shape. The design direction is documented
separately in [`docs/structured-concurrency.md`](structured-concurrency.md):
supervision trees (Erlang-style, library-level on top of a small
runtime hook for actor failure notification) plus capability-scoped
concurrency (extending the existing scope-denial primitives to cover
concurrency sites at compile time).

### Structured Error Types (`std.errors`)

Stdlib wrappers currently return error *strings*. A follow-up step is a
structured error type so callers can programmatically discriminate
between error kinds (file-not-found vs permission-denied vs OOM) without
parsing English. Likely shape: `err.kind` + `err.message` + optional
`err.cause`. Non-breaking — existing `err != ""` checks would still work
for the common "did it fail?" case.

## Stdlib Primitives

Missing primitives that keep biting real users when they try to write
tool-style Aether programs. Ordered by impact.

### P1 — `os.run` / `os.run_capture` (argv-based process execution)

`os.system` and `os.exec` both take a single command string and hand it
to `/bin/sh -c` (or `cmd.exe /c` on Windows). That works for trivial
cases and falls apart the moment an argument contains a space, a quote,
a backslash, or a shell metacharacter. Every Aether script that shells
out has to hand-roll quoting and usually gets it wrong for at least one
edge case.

The fix is a pair of argv-based APIs that bypass the shell entirely:

```aether
// Just exit code
code, err = os.run(["git", "clone", repo_url, target_dir])

// Exit code + stdout + stderr
code, stdout, stderr, err = os.run_capture(["ls", "-la", path_with_spaces])
```

**Implementation:**
- POSIX: `fork()` + `execvp()` + `waitpid()`. `run_capture` uses `pipe()` + non-blocking drain.
- Windows: `CreateProcessW()` with a properly escaped command-line buffer (`CommandLineToArgvW` rules), or the `lpApplicationName` form for the no-escape path. Capture variant uses `CreatePipe()` with inherited handles.
- `argv` accepts an Aether `list` or array of strings; no shell involved.

Keep `os.system` and `os.exec` for the shell-required cases (pipe-to-grep,
redirection, etc.) but make `os.run` the recommended default.

### P2 — `fs_glob` Windows port to `FindFirstFileW`

The current `fs_glob_raw` uses `FindFirstFileA` which is ANSI-only and
trips over paths with non-ASCII characters. The POSIX side uses `glob()`
and a recursive `dirent.h` walker. Needs:

- Port to `FindFirstFileW` (wide-char, UTF-16) with UTF-8 conversion at
  the boundary
- Recursive walk for `**` patterns, matching the POSIX behavior
- Dot-prefix filtering consistent with POSIX (hidden files excluded by
  default, matches `..` correctly)

### P3 — `aether.argv0` builtin + `os.execv` wrapper

Two small additions that unblock "re-exec self with different args"
and "know where the binary lives" patterns common in CLI tools:

- `aether.argv0()` → `string`: returns the path the current program was
  invoked with (what `argv[0]` would be in C). The runtime already
  captures this internally via `aether_args_init`; it just needs to be
  surfaced as a builtin.
- `os.execv(argv)` → doesn't return on success, returns `string` error
  on failure: replaces the current process image with another. Thin
  wrapper over POSIX `execvp()` and Windows `_execvp()`.

### P4 — `std.fs` completeness bundle

Six filesystem primitives that are present in every other language's
stdlib and currently force Aether users to shell out:

| Wrapper | POSIX | Windows |
|---|---|---|
| `fs.copy(src, dst)` | `open` + `read`/`write` loop | `CopyFileW` |
| `fs.move(src, dst)` | `rename` (same filesystem), fall back to copy+delete | `MoveFileExW` with `MOVEFILE_REPLACE_EXISTING` |
| `fs.mkdir_p(path)` | recursive `mkdir(path, 0755)` | `SHCreateDirectoryExW` |
| `fs.realpath(path)` | `realpath(3)` | `GetFullPathNameW` |
| `fs.chmod(path, mode)` | `chmod(2)` | no-op or `SetFileAttributesW` for readonly |
| `fs.symlink(target, link)` | `symlink(2)` | `CreateSymbolicLinkW` with junction fallback for dirs, copy-on-failure for files (non-elevated accounts can't create file symlinks on Windows) |

All six return the usual `(value, err)` or `string` error shape
consistent with the rest of `std.fs`.

**Note on existing functions:** `path.join`, `path.normalize`,
`path.dirname`, `path.basename`, `path.is_absolute` are already
implemented and don't need to be re-done. They live in `std/fs/aether_fs.c`.

### Post-migration audit

Once P1–P4 land, walk the `tools/*.ae` files (especially anything under
`aetherBuild`) and migrate call sites from `os.system`/`os.exec` +
manual quoting to `os.run`, and from shell-based file copying to
`fs.copy`/`fs.move`. This is mechanical but high-signal: it proves the
new primitives are actually better, and it'll surface any API gaps
before external users hit them.

## Quick Wins

### Package Registry — Transitive Dependencies

`ae add` supports versioned packages (`ae add github.com/user/repo@v1.0.0`) and the module resolver finds installed packages. Next: transitive dependency resolution, lock file integrity checking, `ae update`, and a publishing command (`ae publish`).

### `or` Keyword for Error Defaults

Sugar for defaulting on error: `content = io.read_file("config.txt") or "default"`. The stdlib now uses Go-style tuple returns throughout, so this syntactic sugar can be built on top of the existing `(value, err)` convention.

## Future

Major features that require significant architectural work.

### WebAssembly Target — Phase 2

Phase 1 is complete: `ae build --target wasm` compiles Aether to WebAssembly via Emscripten. Multi-actor programs work cooperatively.

**What's remaining (Phase 2):**
- Multi-actor programs using Web Workers as scheduler threads with `postMessage`
- Emscripten-specific output (HTML template for browser)
- WASI support for non-browser environments

### Async I/O Integration

All I/O in Aether is currently blocking. `http.get()`, `file.read()`, `tcp.connect()`, and `sleep()` all block the OS thread. Since the scheduler places actors on the spawner's core by default (locality-aware placement), actors spawned from `main()` all land on core 0 — one OS thread. A blocking I/O call in one actor prevents ALL actors on that core from running.

**User impact:** An actor doing 5 HTTP requests will block all sibling actors for the entire duration. There is no way for the scheduler to preempt a handler that's blocked in a system call.

**Mitigation (shipped):**
- **Socket timeouts** — All stdlib TCP operations now set 30-second `SO_RCVTIMEO`/`SO_SNDTIMEO`. A dead peer returns an error instead of hanging forever.
- **Core placement** — `spawn(Actor(), core: N)` distributes I/O-heavy actors across cores so they run on different OS threads. Combined with `num_cores` builtin for `core: i % num_cores`.
- **HTTP server thread pool** — Bounded worker pool (8 threads) replaces unbounded thread-per-connection. Poll-based accept with timeout for graceful shutdown.
- **Platform poller** — `runtime/io/aether_poller.h` provides epoll (Linux), kqueue (macOS/BSD), and poll() (portable) backends behind a unified API.

**Next: actor-integrated HTTP ([PR #71](https://github.com/nicolasmd87/aether/pull/71))**

Ariel's PR proposes dispatching incoming HTTP connections as file descriptors directly to pre-spawned worker actors via mailbox delivery, replacing the thread pool with actor-based dispatch. Bench-measured throughput improvement vs. the thread-pool baseline was substantial; rerun benchmarks against current main before relying on historical figures. The PR needs:
- Rebase from v0.23.0 to current (v0.41.0+)
- Use the new platform poller abstraction instead of Linux-only epoll
- Integration with scheduler timeout support (added since the PR was opened)

**Future: general async I/O**
- I/O completions delivered as actor messages (send request → receive response as message)
- Scheduler awareness of I/O-blocked actors (don't count them as idle)
- Async variants of file and network operations in the stdlib
- Non-blocking `sleep` that yields to the scheduler instead of blocking the thread

### Version Management UX

`ae version list` should clearly show which versions are installed locally, which are available remotely, and which is active. Current display only marks the active version.

**What's needed:**
- `ae version list` columns: version, status (active/installed/available)
- Windows: `ae version use` should preserve the initial install in `versions/` before switching (POSIX side shipped with the macOS Gatekeeper fix).

## Host Language Bridges (`contrib/host/`)

These are cross-cutting items that touch every in-process host bridge
(Lua, Python, Perl, Ruby, Tcl, JS) plus the separate-process hosts
(Aether, Go, Java). They were deferred from the 0.72.x host cleanup
because the shape of the solution isn't obvious yet — they need an
explicit API decision, not a per-host hack.

### Capture stdout/stderr from hosted code

Today hosted code prints straight to the Aether process's stdout. That
works for demos but breaks two use cases: (a) an Aether supervisor that
wants to filter or route a sandboxed script's output, and (b) embedding
the output in a structured response (HTTP body, actor message).

**Design space**:
- Pipe-based: each `run_sandboxed()` invocation creates a pair of
  pipes, rewires the host's stdout/stderr FDs for the duration, and
  returns the captured bytes. Works uniformly across all 7 in-process
  hosts since they all emit through libc `write(1, ...)`. Thread-unsafe
  though — concurrent sandboxed calls would race on the FD swap.
- Shared-map key convention: reserve `_stdout` / `_stderr` keys in the
  per-run shared map and have each bridge's print binding also write
  to those keys. Thread-safe (map is per-token) but requires touching
  every host's print binding.
- Pass-through (status quo): don't capture, let the Aether program
  capture its own process output if it cares. Simplest but punts the
  problem onto every user.

**Decision needed**: which of the three shapes, and whether it applies
to the separate-process hosts (Go/Java/Aether) the same way or gets
mapped onto their existing `execvp` output.

### Shared-map native bindings for Perl and Ruby

`aether_map_get` / `aether_map_put` for Perl and Ruby currently work
via `eval`-injected hashes — reads pull from a tied hash, but writes
stay in the hosted language and never reach the C-side shared map.
Python, Lua, Tcl, and JS all have proper C/Tcl bindings.

**Fix shape**: Perl XS module (`AetherMap.xs`) and Ruby C extension
(`aether_map_ext.c`) that export `aether_map_get`/`aether_map_put` as
native functions calling `aether_shared_map_get_by_token` /
`aether_shared_map_put_by_token` directly.

### `string:bytes` mode for shared map

The shared map stores strings. Passing binary data (images, protobufs,
raw MIDI) currently forces the caller to base64-encode. Base64 expands
bytes by a 4:3 ratio as a matter of the encoding's arithmetic, and
adds an encode/decode step on each side of the boundary.

**Fix shape**: a sibling API `aether_shared_map_put_bytes(token, key,
buf, len)` + `aether_shared_map_get_bytes(token, key, &len) -> buf`
that doesn't null-terminate or encode. The C-side map already stores
length-prefixed values; the change is API-only on the C side. Each
bridge then needs a new binding (`aether_map_put_bytes` / `_get_bytes`)
in the language that surfaces it as bytes/blob rather than string.

## Sandbox

### Interception surface expansion

The LD_PRELOAD layer in `runtime/libaether_sandbox_preload.c`
intercepts a curated set of libc entry points. Kernel-level
alternatives to the same operations currently bypass it — see
[`docs/containment-sandbox.md`](containment-sandbox.md) →
*Interception surface* for the catalogued list (openat2,
open_by_handle_at, sendfile, copy_file_range, io_uring, readlink,
getdents64, bind/accept, UDP socket paths, execveat, clone, prctl,
memfd_create, etc.). Expanding coverage is a per-syscall exercise
combined, where needed, with seccomp-bpf for the syscalls with no
libc wrapper to hook. Defence-in-depth story: Aether covers
cooperative containment for normal-code paths; seccomp-bpf closes
adversarial kernel-level bypasses.

## HTTP server — Apache-class capability gap (#260)

Issue #260 catalogues the gap between Aether's current HTTP server
(`std/net/aether_http_server.c` — routing, middleware function-pointer
chain, multi-accept with SO_REUSEPORT, optional actor dispatch via
`MSG_HTTP_CONNECTION`) and what Apache httpd offers as a baseline.
Closing the gap is multi-week work that didn't fit the current
issue-pack PR; tracking the planned slices below so the work can land
incrementally without losing the inventory.

### Tier 0 — protocol-level fundamentals

The minimum a "production" HTTP server needs that Aether is missing
today:

- **TLS termination** — OpenSSL-based TLS context wired into the
  accept loop. Reuses the existing `AETHER_HAS_OPENSSL` guard pattern
  from `std.cryptography`. New API surface:
  `http.server_set_tls(server, cert_path, key_path) -> int`. Graceful
  "TLS unavailable" error when not built with OpenSSL. Test fixture
  uses a self-signed cert generated in test setup. Realistic estimate:
  3–4 days of focused work including the SNI / ALPN edges.
- **HTTP/1.1 keep-alive** — current server is one-request-per-socket
  even though `keep_alive_timeout` is in the struct. Wire the
  `Connection:` header parse, hold sockets open across requests,
  honor `max_requests_per_connection` and `idle_timeout`. New API:
  `http.server_set_keepalive(server, enabled, max_requests, idle_ms)`.
  Realistic estimate: 2–3 days.
- **Per-connection actor dispatch** — replace the request-per-actor
  spawn with one actor per accepted connection, draining requests on
  that connection until close. Lets long-lived connections (keep-alive,
  future SSE / WebSocket) coexist with short ones without burning a
  worker. The scheduler integration is already there
  (`MSG_HTTP_CONNECTION`, `scheduler_spawn_pooled`); the change is
  the dispatch loop inside the actor. Realistic estimate: 3–4 days
  including soak testing under parallel keep-alive sessions.

### Tier 1 — stdlib middleware (`std.http.middleware`)

A composable middleware module exposing eight wrappers as the canonical
"Apache-feature" set:

```aether
import std.http.middleware
handler = middleware.gzip(my_handler)
handler = middleware.cors(opts, handler)
handler = middleware.basic_auth(realm, verify_fn, handler)
handler = middleware.rate_limit(n, window_ms, handler)
handler = middleware.vhost(host_to_handler_map)
handler = middleware.rewrite(rules, handler)
handler = middleware.static_files(root, prefix)
handler = middleware.error_pages(code_to_handler_map, handler)
```

`gzip` builds on the existing `std.zlib` dep; `rate_limit` uses an
in-memory token bucket (per-process, single-tier — distributed rate
limiting is a separate problem). Realistic estimate: 4–5 days for all
eight including one fixture per middleware. Splittable into two PRs
of four if review fatigue is a concern (suggested split:
gzip / cors / basic_auth / rate_limit first, then vhost / rewrite /
static_files / error_pages).

### Tier 2 — modern protocols (deferred)

Out of scope for the first cut. Tracking here so the next pass starts
with the inventory:

- **HTTP/2** — multiplexed streams, server push, HPACK header
  compression. Significant new surface (~3–4k LOC of frame handling)
  unless we wrap nghttp2.
- **WebSocket** — RFC 6455 framing on top of the keep-alive socket
  pump. Easier than HTTP/2 since the framing is straightforward; the
  hard part is the per-connection message-pump actor model.
- **Server-Sent Events (SSE)** — single direction text-event-stream
  over HTTP/1.1. Simplest of the three protocols; depends only on
  Tier 0 keep-alive being shipped first.

### Tier 3 — operational features (deferred)

Production ops surface area:

- Access / error logging with structured format options (JSON,
  combined log format, custom)
- Per-route metrics (request count, latency histogram, error rate)
  exported via Prometheus / OpenTelemetry-compatible endpoints
- Graceful shutdown (drain in-flight, refuse new accepts, exit when
  drained or timeout)
- Worker lifecycle hooks for pre-fork / post-fork / shutdown handlers
- HTTP health probe surface (`/healthz`, `/readyz`) that integrates
  with the actor scheduler's quiescence detection

The umbrella issue (#260) is the canonical tracker; per-tier sub-issues
should be filed when the tier picks up scheduling.

## Type system

### Type inference propagation through `select()`

`select(linux: ..., windows: ..., macos: ...)` stores string results
correctly at the call site, but the inferred type doesn't propagate
into `println()` directly — see
[`docs/named-args-and-select.md`](named-args-and-select.md) → *Printing
string results*. Workaround today: wrap in string interpolation. Fix:
thread the selected-branch type through `select()` in the typechecker
so `println(os_string)` picks `%s` automatically.

### Polymorphism, higher-rank types, type classes

Aether inference is currently monomorphic — functions resolve to a
single concrete type per call site. Features waiting on a larger
language design pass:

- **Generic functions.** A `min(a, b)` that works for any ordered type
  without duplicating per-type definitions.
- **Higher-rank types.** Functions that accept polymorphic functions as
  arguments (e.g. a mapping combinator that takes `fn[T, U]` without
  fixing `T`/`U`).
- **Type classes / constraints.** Haskell-style `Ord`, `Eq`,
  `Show` — or Rust-style traits — to constrain generic parameters to
  operations they support.

Each of these is a language-level change; they're listed here so the
surface area is visible, not because any one is scheduled.

## Tooling

### Planned

| Feature | Status | Notes |
|---------|--------|-------|
| `ae fmt` | Not started | Source code formatter (deferred until syntax stabilizes) |
