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

## Documentation Gaps

### Built-in functions not documented on `main`

The language reference on the development branch documents `clock_ns()`, `sleep()`, and other builtins under **Built-in Functions → Timing / Concurrency**, but this section is incomplete on `main`. Merge pending.

| Built-in | Status | Notes |
|----------|--------|-------|
| `print(value)` | Documented | |
| `println(value)` | Documented | |
| `clock_ns()` | Not on `main` | Returns nanoseconds as `long` |
| `sleep(ms)` | Not on `main` | Millisecond pause |
| `spawn(Actor())` | Not on `main` | Actor creation |
| `wait_for_idle()` | Not on `main` | Wait for actors to finish |
| `getenv(name)` | Not on `main` | Environment variable lookup |
| `atoi(s)` | Not on `main` | String to int (builtin, no extern needed) |
| `exit(code)` | Not on `main` | Terminate program with exit code |

## Tooling

### Planned

| Feature | Status | Notes |
|---------|--------|-------|
| `ae fmt` | Not started | Source code formatter |
| `ae check` | Not started | Type-check without compiling |
| Dead code diagnostics | Not started | Warn on unused variables/functions |

### Recently Completed (0.17.0)

| Feature | Notes |
|---------|-------|
| `aetherc --dump-ast` | Print parsed AST for debugging |
| `ae run -g` debug symbols | Dev builds include debug info for gdb/lldb |
| `NO_COLOR` / `isatty()` support | Clean error output in pipes and CI |
| `exit()` builtin | Terminate program with exit code |
| String `==`/`!=` with `strcmp` | Content comparison, not pointer comparison |
| `match` on strings with `strcmp` | String match arms work correctly |
| `defer` correct return types | No more truncation of ptr/long/float returns |
| Token overflow error | Clear error when source exceeds MAX_TOKENS |
