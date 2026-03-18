# Next Steps

Planned features and improvements for upcoming Aether releases.

## Standard Library

### `std.os` — Shell & Process Execution

Currently users must `extern system(cmd: string) -> int` to run shell commands. This should be a first-class stdlib module.

**Planned API:**

```aether
import std.os

main() {
    // Run a command, get exit code
    code = os.system("ls -la")

    // Run a command, capture stdout as string
    output = os.exec("date")
    println(output)

    // Get environment variable
    home = os.getenv("HOME")
    println(home)
}
```

**Implementation notes:**
- `os.system(cmd)` — wraps C `system()`, returns exit code
- `os.exec(cmd)` — wraps C `popen()` + read loop, returns stdout as string
- `os.getenv(name)` — wraps C `getenv()`, returns string (or empty)
- Cross-platform: all three functions exist on POSIX and Windows

**Origin:** [Issue #39](https://github.com/nicolasmd87/aether/issues/39) — user requested shell execution and terminal control (`clear`, capturing output)

## Tooling

### Planned

| Feature | Status | Notes |
|---------|--------|-------|
| `ae fmt` | Not started | Source code formatter |
| `ae check` | Not started | Type-check without compiling |
| Dead code diagnostics | Not started | Warn on unused variables/functions |

> See [CHANGELOG.md](../CHANGELOG.md) for what shipped in each release.

## Compiler Diagnostics

### Planned

| Feature | Status | Notes |
|---------|--------|-------|
| Type mismatch hints | Not started | "expected 'string', got 'ptr'" with help text |
| Unused variable warnings | Not started | Warn on declared-but-unused locals |
| Unreachable code warnings | Not started | Detect dead branches after return/exit |

**Goal:** Python 3.10-style "the compiler is teaching you" error messages. Example:

```
error[E0201]: type mismatch — expected 'string', got 'ptr'
  --> src/main.ae:12:11
   |
12 |     print(result)
   |           ^^^^^^ this is a 'ptr' (raw pointer), not a 'string'
   |
   help: use string.to_cstr(result) to convert to a printable string
```
