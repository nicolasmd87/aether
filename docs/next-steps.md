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

### ~~P1~~ — `os.run` / `os.run_capture` (argv-based process execution) — **SHIPPED**

> **Status: shipped (PR #148, exit-code tuple in #289).** See
> [Process spawn (argv-based, no shell)](stdlib-api.md#process-spawn-argv-based-no-shell)
> in the stdlib reference for current signatures and a worked example.
> Section retained below for the original rationale; rationale-as-roadmap
> rather than as a to-do.

`os.system` and `os.exec` both take a single command string and hand it
to `/bin/sh -c` (or `cmd.exe /c` on Windows). That works for trivial
cases and falls apart the moment an argument contains a space, a quote,
a backslash, or a shell metacharacter. Every Aether script that shells
out has to hand-roll quoting and usually gets it wrong for at least one
edge case.

The fix is a pair of argv-based APIs that bypass the shell entirely:

```aether
// Just exit code
code = os.run(prog, argv, env)

// Exit code + stdout + stderr
stdout, exit_code, stderr = os.run_capture(prog, argv, env)
```

**Implementation:**
- POSIX: `fork()` + `execvp()` + `waitpid()`. `run_capture` uses `pipe()` + non-blocking drain.
- Windows: currently uses POSIX-fallback shims; native `CreateProcessW()` backend tracked separately.
- `argv` is a `list<ptr>` of strings; no shell involved.

`os.system` and `os.exec` remain for the shell-required cases (pipe-to-grep,
redirection, etc.); `os.run` / `os.run_capture` are the recommended defaults.

### P2 — `fs_glob` Windows port to `FindFirstFileW`

The current `fs_glob_raw` uses `FindFirstFileA` which is ANSI-only and
trips over paths with non-ASCII characters. The POSIX side uses `glob()`
and a recursive `dirent.h` walker. Needs:

- Port to `FindFirstFileW` (wide-char, UTF-16) with UTF-8 conversion at
  the boundary
- Recursive walk for `**` patterns, matching the POSIX behavior
- Dot-prefix filtering consistent with POSIX (hidden files excluded by
  default, matches `..` correctly)

### ~~P3~~ — `aether.argv0` builtin + `os.execv` wrapper — **SHIPPED**

> **Status: shipped.** Surfaces are `os.argv0()` (and `aether_argv0()`
> for null-discriminating callers) and `os_execv(prog, argv_list)`.
> See [Argv discovery](stdlib-api.md#argv-discovery) and
> [Process replacement](stdlib-api.md#process-replacement) in the
> stdlib reference. Section retained below for the original rationale.

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

## ~~HTTP server — Apache-class umbrella~~ — **SHIPPED**

> **Status: shipped (issue #260 closed).** Tier 0 (TLS, keep-alive,
> per-connection actor dispatch), Tier 1 middleware (cors,
> basic_auth, **bearer_auth**, **session_auth**, rate_limit,
> vhost, gzip, static_files, rewrite, error_pages, **real_ip**),
> Tier 2 protocols (SSE, WebSocket per RFC 6455, and **HTTP/2 via
> libnghttp2** — h2 over TLS via ALPN, h2c upgrade per RFC 7540
> §3.2, h2 prior-knowledge over plain TCP, **GOAWAY** on graceful
> shutdown, **per-stream concurrent dispatch via a server-level
> pthread pool**), and Tier 3 operational (graceful shutdown,
> lifecycle hooks, health probes, structured access logs,
> Prometheus metrics) all delivered. See
> [http-server.md](http-server.md) for the full surface, the
> middleware compatibility table, the architecture rationale
> (pthreads vs actors, server-level vs per-connection), and the
> troubleshooting section.

Follow-up optimisations tracked separately (each its own future
issue when scheduled):

- **HTTP/2 server push (PUSH_PROMISE).** Optional per RFC 7540 §6.6,
  rarely used in practice. Add when there's a concrete consumer.
- **HPACK Huffman emit on Windows.** libnghttp2 already applies
  Huffman to response headers by default on POSIX (the wrapper
  uses `NGHTTP2_NV_FLAG_NONE`, leaving the choice to nghttp2 per
  RFC 7541 §5.2). Confirm the same path on the Windows-MinGW
  build; revisit if the wire-size profile differs.
- **HTTP/3 / QUIC.** Out of scope for #260; would be its own issue.

## VCR recorder — transparent MITM forwarder

`std.http.server.vcr` ships replay (load tape → server replays
recorded responses) and the storage primitives (`vcr.record`,
`vcr.record_full`, `vcr.flush`). What's missing is the **recording
proxy** — an in-process forwarder that:

1. Accepts inbound HTTP from a client (svn, curl, http test
   harness, or any application configured to point at it).
2. Forwards each request upstream to the real service, rewriting
   the Host header `127.0.0.1:PORT` → upstream hostname before
   sending so the upstream sees a normal request.
3. Captures the request + response as they pass through, calling
   `vcr.record_full(...)` to append the interaction.
4. Returns the upstream's response back to the original client.
5. On stop, flushes the captured interactions to a tape file via
   `vcr.flush(path)`.

**Shape (transparent MITM, not a proxy or CORS trick):** from the
client's POV the recorder *is* the upstream — same URL, same hostname
in the URL bar / config, same response shapes. The client doesn't
know it's being intercepted. Not an HTTP proxy in the
`HTTP_PROXY=...` / `CONNECT`-method sense; not a browser-side CORS
workaround. Just a normal HTTP server that happens to forward and
capture everything that flies through it.

**Why a separate piece, not a flag on `vcr.load()`:** the recorder
pulls in `std.http.client` (for outbound) on top of the existing
`std.http` server side that VCR already uses for replay. Different
config (upstream URL + scheme), different lifecycle (records on
incoming, vs replay's load-once-then-serve), and conceptually
client-of-its-own-server. Belongs alongside but distinct from the
replay dispatcher.

**Placement:** likely `contrib/vcrrecord/` rather than
`std/http/server/vcr/` — recorder is a developer testing tool with
opinionated shape (which headers to redact at record time, how to
canonicalize User-Agent / Date / UUID for re-recording stability),
which is contrib-shaped per `docs/stdlib-vs-contrib.md`. Storage
stays in std (the tape format is the stable interop format).

**Open design questions** (resolved when a real recording use case
shows up):

- Header-redaction surface: is `vcr.redact()` (already in std)
  enough, or does the recorder want a record-time hook surface
  like Java Servirtium's `InteractionManipulations`? The Java
  interface is broad (URL rewrite, single-header rewrite, body
  rewrite, allowlist/denylist) but most svn-flavoured uses boil
  down to "pin User-Agent, replace UUID and Date in stored
  response headers." Defer the broader hook surface until a second
  recording use case demands it.
- HTTPS upstream: the recorder needs `client.send_request` against
  HTTPS upstreams (svn.apache.org, GitHub APIs, etc). `std.http.client`
  has TLS support; recorder just needs to default to upstream
  scheme detection.
- Dual mode: should one VCR instance switch between record and
  replay (Java Servirtium model) or are they separate constructs
  (closer to the existing `vcr.load` shape)? Java's mode-flip is
  convenient; the cost is config complexity. Two constructs is
  simpler.

Driven by a real recording use case rather than anticipation —
right now the only consumer of recorded tapes is the svn-checkout
test, and the tape it uses was recorded by Java Servirtium and
checked into the Aether tree. When a project needs to record a
fresh tape directly via Aether (e.g. an Aether-hosted upstream
that needs canonical replay tapes generated from its current
behaviour), this is the unblocker.

## VCR — keep the C-API independently consumable

The current driver for VCR in Aether is concrete: exercising it
drives the design and hardening of `std.http.server` and
`std.http.client` simultaneously. Every VCR feature has been a
forcing function on the HTTP stack — response-headers verbatim
emission, repeated-key headers, default-header clearing, keep-alive
across many requests, custom verbs / WebDAV. That's the load-bearing
reason VCR lives inside the Aether tree right now.

**Possible future direction (no commitments):** the C externs in
`aether_vcr.c` (`vcr_load_tape`, `vcr_get_*`, `vcr_dispatch`,
`vcr_record_interaction_full`, `vcr_flush_to_tape`, plus the
diagnostic surface — `vcr_last_kind`, `vcr_last_index`,
`vcr_set_strict_headers`, `vcr_reset_cursor`,
`vcr_get_resp_headers`) form a coherent C-API. If a non-Aether
consumer ever wants to drive VCR — Java/Python/Ruby tests via a
thin FFI wrapper, the [Servirtium](https://servirtium.dev) standard's
shape — the C-API is what they'd link against. Aether would not
host the bindings; those would be ecosystem-owned.

If that future ever materialises, what's missing today:
- A clean `aether_vcr.h` as the canonical C-API contract (the
  Aether `module.ae` is that contract today).
- A versioned C ABI (right now we change it freely under "no
  backwards compat" for in-tree work).
- A non-Aether build target that produces `libvcr.so` standalone
  (separable from the rest of `libaether.a`).
- VCR may move out to its own repo at that point.

None of this is blocking. Flagged here so the rule "keep the C-API
independently consumable" guides choices we make now —
`vcr_last_kind` returns int (numeric enum, no Aether string
smuggling), the dispatcher takes plain `void* req` / `void* res`
(not Aether-typed handles), error reporting is via passive read
slots (no actor messaging). Costs nothing to keep that property,
and lowers the cost of any future split.

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
