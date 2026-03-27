# Getting Started with Aether

This guide covers installation and basic usage of the Aether programming language.

## Installation

### Quick Install (recommended)

```bash
git clone https://github.com/nicolasmd87/aether.git
cd aether
./install.sh
```

This builds the compiler, CLI tool, and standard library, then installs everything to `~/.aether`. After restarting your terminal (or running `source ~/.bashrc`, `~/.zshrc`, or `~/.bash_profile`), the `ae` command is available globally.

To install to a custom location:

```bash
./install.sh /usr/local    # System-wide install (needs sudo)
```

### Prerequisites

The installer checks for these automatically and provides platform-specific install commands if something is missing. Here's what you need:

**macOS:**
```bash
xcode-select --install
# Or with Homebrew: brew install gcc make
```

**Linux (Debian/Ubuntu/Pop!_OS/Mint):**
```bash
sudo apt-get install build-essential
```

**Linux (Fedora):**
```bash
sudo dnf install gcc make
```

**Linux (RHEL/CentOS/Rocky/AlmaLinux):**
```bash
sudo yum install gcc make
```

**Linux (Arch/Manjaro):**
```bash
sudo pacman -S base-devel
```

**Linux (openSUSE):**
```bash
sudo zypper install gcc make
```

**Linux (Alpine):**
```bash
apk add build-base
```

**Windows:**

The easiest way is to download the pre-built release binary — no MSYS2, no manual toolchain required:

1. Download `aether-*-windows-x86_64.zip` from [GitHub Releases](https://github.com/nicolasmd87/aether/releases)
2. Extract to any folder — e.g. `C:\aether`
3. Add `C:\aether\bin` to your PATH (System Settings → Environment Variables → Path)
4. **Restart your terminal** (so PATH takes effect)
5. Open any terminal (PowerShell or CMD):

```powershell
ae version
ae init hello
cd hello
ae run
```

**GCC is downloaded automatically on the first `ae run`** (~80 MB, one-time). No MSYS2, no separate installer needed.

> **Windows Defender tip:** The first build may trigger a scan. Adding `C:\aether` to Windows Security exclusions speeds things up.

> **Building from source / contributors:** Install [MSYS2](https://www.msys2.org/), open "MSYS2 MinGW 64-bit", then `pacman -S mingw-w64-x86_64-gcc make git` and `make ae`.

### Development Build (without installing)

If you prefer to build without installing to your system:

```bash
make ae
./build/ae version
```

This builds the `ae` CLI tool in the `build/` directory. You'll need to use `./build/ae` instead of just `ae`.

### Editor Setup

For syntax highlighting and a better development experience:

**VS Code / Cursor:**
```bash
cd editor/vscode
./install.sh
```

Features included:
- Syntax highlighting with TextMate grammar
- Custom "Aether Erlang" dark theme optimized for Aether code
- `.ae` file icons
- Basic language configuration (comments, brackets, etc.)

After installation, open any `.ae` file and VS Code will automatically apply syntax highlighting. Select the "Aether Erlang" theme via `Preferences > Color Theme` for the best experience.

## Your First Aether Program

**Option A: Create a project (recommended)**

```bash
ae init myproject
cd myproject
ae run
```

This creates a project with `aether.toml`, `src/main.ae`, and `tests/`.

**Option B: Run a single file**

Create `hello.ae`:

```aether
main() {
    println("Hello, Aether!")
}
```

```bash
ae run hello.ae
```

**Build an executable:**

```bash
ae build hello.ae -o hello
./hello
```

**Build for WebAssembly:**

```bash
ae build --target wasm hello.ae
node hello.js
```

Requires [Emscripten](https://emscripten.org/) (`emcc` on PATH).

**Type-check without compiling:**

```bash
ae check hello.ae
```

## Interactive REPL

Experiment with Aether interactively:

```bash
ae repl
```

```
Aether 0.27.0 REPL
Type expressions to evaluate. Enter on empty line to run.
Commands: :help :reset :show :quit

ae> x = 5

ae> y = 3

ae> println(x + y)

8
ae> :show
  x = 5
  y = 3
ae> :quit
```

Assignments and constants persist across evaluations. Multi-line blocks (if/while/for) auto-continue until braces close. Use `:reset` to clear the session, `:show` to see accumulated code.

## Actor-Based Programming

Aether is built around the actor model. Here is a simple counter example:

```aether
message Ping {}

actor Counter {
    state count = 0

    receive {
        Ping() -> {
            count = count + 1
        }
    }
}

main() {
    c = spawn(Counter())
    c ! Ping {}
    c ! Ping {}
    c ! Ping {}

    // Wait for all messages to be processed
    wait_for_idle()

    println("Count: ${c.count}")
}
```

Actors are lightweight concurrent entities that communicate through asynchronous messages. Each actor has private state and a mailbox for incoming messages. Messages are defined with the `message` keyword and sent with the `!` operator. The runtime distributes actors across available CPU cores automatically.

Use `wait_for_idle()` to block until all actors have finished processing their messages. This is essential when you need to read actor state or coordinate completion.

## Module System

Import modules using the `import` statement:

```aether
import std.map

main() {
    mymap = map.new()
    defer map.free(mymap)

    map.put(mymap, "greeting", "hello")

    println("Map created")
}
```

Functions are called using **namespace-style syntax**: `namespace.function()`.

| Import | Namespace | Example |
|--------|-----------|---------|
| `import std.string` | `string` | `string.new("hello")`, `string.length(s)` |
| `import std.file` | `file` | `file.open("f", "r")`, `file.exists("f")` |
| `import std.dir` | `dir` | `dir.create("d")`, `dir.exists("d")` |
| `import std.path` | `path` | `path.join("a", "b")`, `path.basename("a/b")` |
| `import std.list` | `list` | `list.new()`, `list.add()`, `list.get()` |
| `import std.map` | `map` | `map.new()`, `map.put()`, `map.get()` |
| `import std.json` | `json` | `json.parse(str)`, `json.create_object()` |
| `import std.math` | `math` | `math.abs_int(x)`, `math.sqrt(x)` |
| `import std.http` | `http` | `http.get(url)`, `http.server_create(port)` |
| `import std.tcp` | `tcp` | `tcp.connect(host, port)`, `tcp.send()` |
| `import std.log` | `log` | `log.init("app")`, `log.write()` |
| `import std.io` | `io` | `io.read_file("f")`, `io.getenv("HOME")` |
| `import std.os` | `os` | `os.exec("ls")`, `os.system("cmd")` |

### Creating Your Own Modules

You can write reusable modules in pure Aether — no C required. Place your module in `lib/<name>/module.ae`:

**lib/mymath/module.ae:**
```aether
export const PI = 3

export double_it(x) {
    return multiply(x, 2)
}

export add(a, b) {
    return a + b
}

// Private helper — not accessible from outside
multiply(a, b) {
    return a * b
}
```

**src/main.ae:**
```aether
import mymath

main() {
    println(mymath.double_it(5))    // 10
    println(mymath.add(3, 4))       // 7
    println(mymath.PI)              // 3
    // mymath.multiply(2, 3)        // Error: not exported
}
```

Use `export` to control which functions and constants are part of your module's public API. Non-exported symbols are private — they can be used internally by exported functions but are not accessible to importers. If a module has no `export` declarations, all symbols are public (backwards compatible).

Modules support functions, constants, intra-module calls (functions calling other functions in the same module), and export visibility. See [Module System Design](module-system-design.md#pure-aether-modules) for full details.

## Standard Library

Aether includes a standard library with the following modules:

| Module | Description |
|--------|-------------|
| `std.string` | String operations |
| `std.file` | File operations |
| `std.dir` | Directory operations |
| `std.path` | Path utilities |
| `std.list` | Dynamic array (ArrayList) |
| `std.map` | Hash map (HashMap) |
| `std.json` | JSON parsing and creation |
| `std.http` | HTTP client and server |
| `std.tcp` | TCP sockets |
| `std.log` | Structured logging |
| `std.math` | Math functions |
| `std.io` | Console I/O, environment variables |
| `std.os` | Shell execution, command output capture |

See [stdlib-api.md](stdlib-api.md) for the full API reference.

> **Strings just work.** When you read a file or get data from stdlib functions, the result is a
> regular string — you can `print()` it, use it in `"${interpolation}"`, or pass it in messages
> directly. No conversion needed.

### Memory Management

Some stdlib functions return heap-allocated strings (`io.getenv()`, `io.read_file()`, `os.exec()`, `os.getenv()`, `file.read_all()`, `json.stringify()`, `json.get_string()`, etc.). Use `defer free()` to release them:

```aether
import std.io

main() {
    home = io.getenv("HOME")
    defer free(home)
    println(home)

    content = io.read_file("data.txt")
    defer free(content)
    println(content)
}
```

`free()` is a language builtin — no import needed. `defer` ensures the memory is released when the enclosing scope exits, even if the function returns early.

For `std.string` managed strings (`string.new()`, `string.to_upper()`, etc.), use `defer string.release(s)` instead.

## Pattern Matching

Aether features Erlang-inspired pattern matching, one of its most powerful features.

### Function Pattern Matching

Define functions with multiple clauses that match on argument values:

```aether
// Match on literal values
factorial(0) -> 1;
factorial(n) when n > 0 -> n * factorial(n - 1);

// Fibonacci with multiple base cases
fib(0) -> 0;
fib(1) -> 1;
fib(n) when n > 1 -> fib(n - 1) + fib(n - 2);

// Guards for conditional matching
classify(x) when x < 0 -> "negative";
classify(x) when x == 0 -> "zero";
classify(x) when x > 0 -> "positive";

// Multi-parameter pattern matching
gcd(a, 0) -> a;
gcd(a, b) when b > 0 -> gcd(b, a - (a / b) * b);
```

This style replaces verbose if/else chains with declarative, readable code.

### Match Statements

Use `match` for value dispatch:

```aether
match (value) {
    0 -> { println("Zero") }
    1 -> { println("One") }
    _ -> { println("Other") }
}
```

### List Patterns

Match on arrays (requires corresponding `_len` variable):

```aether
nums = [1, 2, 3]
nums_len = 3

match (nums) {
    [] -> { println("empty") }
    [x] -> { println("one element") }
    [h|t] -> {
        println("head: ${h}")
    }
}
```

## Project Configuration

Projects use `aether.toml` for configuration. Created automatically by `ae init`:

```toml
[package]
name = "myproject"
version = "0.1.0"

[[bin]]
name = "myproject"
path = "src/main.ae"

[dependencies]

[build]
target = "native"
# link_flags = "-lsqlite3 -lcurl"  # Link external C libraries
```

### Linking C Libraries

To link external C libraries, add `link_flags` to the `[build]` section:

```toml
[build]
link_flags = "-lsqlite3 -lcurl -lssl"
```

This allows Aether programs to use libraries like SQLite, libcurl, OpenSSL, etc.

### Command-Line Arguments

Access command-line arguments using the runtime's argument functions:

```aether
extern aether_args_count() -> int
extern aether_args_get(index: int) -> string

main() {
    count = aether_args_count()
    for (i = 0; i < count; i = i + 1) {
        println(aether_args_get(i))
    }
}
```

To pass arguments, build and run the binary directly:

```bash
ae build myprogram.ae -o myprogram
./myprogram arg1 arg2
```

### Environment Variables

Read configuration from environment variables using `std.os`:

```aether
import std.os

main() {
    home = os.getenv("HOME")
    defer free(home)
    if home != 0 {
        println("Home directory: ${home}")
    }
}
```

The builtin `getenv()` also works without an import for quick scripts (returns a malloc'd string — use `defer free()`).

## Next Steps

- Read the [Tutorial](tutorial.md) for a guided introduction
- Learn about [C Interoperability](c-interop.md) to use C libraries
- See [C Embedding Guide](c-embedding.md) to embed Aether actors in C applications
- Explore [Standard Library Documentation](stdlib-reference.md)
- See [Architecture](architecture.md) for compiler and runtime internals
- See [Runtime Optimizations](runtime-optimizations.md) for performance details
- Use `ae version list` to see available releases and `ae version install <v>` to switch versions

## Version Management

Switch between Aether releases without reinstalling:

```bash
ae version              # Show current version
ae version list         # List all available releases (marks installed/active)
ae version install v0.25.0  # Download and install a specific version
ae version use v0.25.0      # Switch to an installed version
```

Versions are stored in `~/.aether/versions/`. The active version is symlinked to `~/.aether/current` (Linux/macOS) or copied to `~/.aether/bin/` (Windows).

## Troubleshooting

### Build Failures

**"Aether compiler not found" (Windows)**
- Make sure you **restarted your terminal** after adding `C:\aether\bin` to PATH
- Verify: `where ae` should show `C:\aether\bin\ae.exe`
- Verify: `where aetherc` should show `C:\aether\bin\aetherc.exe`
- If both exist but it still fails, set `AETHER_HOME`: `set AETHER_HOME=C:\aether`

**"gcc: command not found" (Windows)**
- This should not happen with the pre-built binary — `ae` auto-downloads GCC (~80 MB) on first run
- If you built from source via MSYS2, open the "MSYS2 MinGW 64-bit" shell, not plain PowerShell
- Verify: `gcc --version` — should show MinGW-w64 GCC

**"pthread.h: No such file or directory"**
- Linux: `sudo apt-get install libpthread-stubs0-dev`
- Windows: The Aether runtime uses Win32 threads natively — no pthread library needed

**Test failures**
- Run `make test` for unit tests, `make test-ae` for integration tests
- Check for port conflicts if network tests fail (port 8080)

### Common Pitfalls

1. Forgetting to rebuild after changes: run `make clean && make`
2. On Windows, running `ae` from a directory without write permission (GCC download needs `~\.aether\`)
3. Using `state` as a variable name inside an actor body (it's reserved there — use it freely elsewhere)

### WebAssembly and Embedded Targets

Aether supports cross-compilation to platforms without pthreads or POSIX APIs via the `PLATFORM` Makefile variable:

```bash
# Build for WebAssembly (requires Emscripten SDK)
make stdlib PLATFORM=wasm
# Or force cooperative mode on native for testing:
make stdlib EXTRA_CFLAGS="-DAETHER_NO_THREADING"

# Build for embedded ARM (syntax-check only — requires arm-none-eabi-gcc)
make ci-embedded

# Docker-based cross-compilation (no local toolchain needed):
make docker-ci-wasm        # Emscripten + Node.js execution
make docker-ci-embedded    # ARM Cortex-M4 syntax-check
```

On threadless platforms, the cooperative scheduler (`aether_scheduler_coop.c`) replaces the multi-core scheduler. All actors run on a single thread via `aether_scheduler_poll()`. Multi-actor programs work correctly — messages are processed cooperatively during `wait_for_idle()`.

Stdlib modules that depend on filesystem or networking return errors gracefully (NULL, 0, -1). Console I/O (`print`, `println`) always works.

### Platform-Specific Notes

**macOS:**
- May need `xcode-select --install` for command line tools
- Homebrew GCC recommended: `brew install gcc`
- Apple Silicon (M1/M2/M3): Runtime auto-detects P-cores for consistent performance
- Thread affinity is advisory on macOS; occasional benchmark variance is normal

**Linux:**
- Kernel 4.14+ recommended for full NUMA support
- AddressSanitizer may require `gcc-multilib` on some distributions
- Full thread affinity support for deterministic performance

**Windows:**
- Pre-built binaries work in any terminal (PowerShell, CMD, Windows Terminal)
- GCC is auto-downloaded on first `ae run` — no MSYS2 or manual setup required
- The runtime uses Win32 threads natively — no pthreads library required
- Full thread affinity support via `SetThreadAffinityMask`
- P-core detection via `GetSystemCpuSetInformation` (Windows 10 1903+)
- **Building from source:** Use MSYS2 MinGW 64-bit shell with `make ae`
