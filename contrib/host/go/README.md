# contrib.host.go — Go via Separate Subprocess

## Prerequisites

```bash
# Debian/Ubuntu
sudo apt-get install -y golang-go

# macOS
brew install go

# Verify
go version
```

## Build flags

Unlike the in-process hosts (Lua, Python, Perl, Ruby, Tcl, JS) the Go
host has no library to embed — the bridge shells out to the `go`
toolchain. No headers or `-l` flags are needed beyond the standard
Aether runtime:

```toml
# aether.toml
[build]
cflags = ""
link_flags = ""
```

The `go` binary must be on `$PATH` at runtime (or set `GO_BIN` to its
absolute path).

## Usage

```aether
import contrib.host.go

// Option A: run a Go source file via `go run`
go.run_script_sandboxed(perms, "/path/to/script.go")

// Option B: run a pre-built Go binary (tighter grant surface)
go.run_sandboxed(perms, "/path/to/built_binary")
```

## Containment model

Go can't share a process with another runtime, so the host runs Go
code as a **separate subprocess under LD_PRELOAD sandbox
interception** — same pattern as `contrib/host/aether`. Grants apply
to libc calls the Go binary makes (open, connect, execve, getenv)
exactly the same way they apply to any other process.

## Linux vs macOS

LD_PRELOAD is Linux's mechanism. macOS has `DYLD_INSERT_LIBRARIES`
but it's disabled for binaries under System Integrity Protection,
which rules out real enforcement for the `go` toolchain itself.
On macOS the Go subprocess runs without preload and grants are
advisory only (the in-process checker has nothing to intercept).
Use Linux CI for real sandbox enforcement verification.

## When `go run` vs pre-built binary

Mode 1 (`go.run_script_sandboxed`) shells out to `go run script.go`,
which means the `go` toolchain itself runs under the sandbox. You
need to grant it enough to operate: `fs_read` for `$GOROOT` and
`$GOPATH`, `exec` for the linker toolchain, `env` for `GOMODCACHE`
etc. That's a wide grant surface for a sandbox.

Mode 2 (`go.run_sandboxed`) skips all of that: compile with `go
build -o bin script.go` ahead of time, then run just `bin` under
tight grants. Much smaller attack surface if the intent is to
contain *the script's* effects, not the toolchain's.
