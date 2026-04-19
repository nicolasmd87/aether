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
