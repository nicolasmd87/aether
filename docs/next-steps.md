# Next Steps

Planned features and improvements for upcoming Aether releases.

> See [CHANGELOG.md](../CHANGELOG.md) for what shipped in each release.

## Language Features

### Structured Error Types (`std.errors`)

Stdlib wrappers currently return error *strings*. A follow-up step is a
structured error type so callers can programmatically discriminate
between error kinds (file-not-found vs permission-denied vs OOM) without
parsing English. Likely shape: `err.kind` + `err.message` + optional
`err.cause`. Non-breaking — existing `err != ""` checks would still work
for the common "did it fail?" case.

## Quick Wins

### Package Registry — Transitive Dependencies

`ae add` supports versioned packages (`ae add github.com/user/repo@v1.0.0`) and the module resolver finds installed packages. Next: transitive dependency resolution, lock file integrity, and `ae update`.

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

All I/O in Aether is currently blocking. `http.get()`, `file.read()`, `tcp_connect()`, and `sleep()` all block the OS thread. Since the scheduler places actors on the spawner's core by default (locality-aware placement), actors spawned from `main()` all land on core 0 — one OS thread. A blocking I/O call in one actor prevents ALL actors on that core from running.

**User impact:** An actor doing 5 HTTP requests will block all sibling actors for the entire duration. There is no way for the scheduler to preempt a handler that's blocked in a system call.

**Mitigation (shipped):**
- **Socket timeouts** — All stdlib TCP operations now set 30-second `SO_RCVTIMEO`/`SO_SNDTIMEO`. A dead peer returns an error instead of hanging forever.
- **Core placement** — `spawn(Actor(), core: N)` distributes I/O-heavy actors across cores so they run on different OS threads. Combined with `num_cores` builtin for `core: i % num_cores`.
- **HTTP server thread pool** — Bounded worker pool (8 threads) replaces unbounded thread-per-connection. Poll-based accept with timeout for graceful shutdown.
- **Platform poller** — `runtime/io/aether_poller.h` provides epoll (Linux), kqueue (macOS/BSD), and poll() (portable) backends behind a unified API.

**Next: actor-integrated HTTP ([PR #71](https://github.com/nicolasmd87/aether/pull/71))**

Ariel's PR proposes dispatching incoming HTTP connections as file descriptors directly to pre-spawned worker actors via mailbox delivery, replacing the thread pool with actor-based dispatch. This achieved +82-103% throughput improvement in benchmarks. The PR needs:
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

## Tooling

### Planned

| Feature | Status | Notes |
|---------|--------|-------|
| `ae fmt` | Not started | Source code formatter (deferred until syntax stabilizes) |
