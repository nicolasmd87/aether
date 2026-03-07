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
    print("Hello, Aether!\n");
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

    print("Count: ")
    print(c.count)
    print("\n")
}
```

Actors are lightweight concurrent entities that communicate through asynchronous messages. Each actor has private state and a mailbox for incoming messages. Messages are defined with the `message` keyword and sent with the `!` operator. The runtime distributes actors across available CPU cores automatically.

Use `wait_for_idle()` to block until all actors have finished processing their messages. This is essential when you need to read actor state or coordinate completion.

## Module System

Import modules using the `import` statement:

```aether
import std.map
import std.log

main() {
    log.info("Starting application");

    mymap = map.new();
    defer map.free(mymap);

    key = "greeting";
    map.put(mymap, key, "hello");

    print("Map created\n");
}
```

Functions are called using **namespace-style syntax**: `namespace.function()`.

| Import | Namespace | Example |
|--------|-----------|---------|
| `import std.string` | `string` | `string.new("hello")` |
| `import std.file` | `file` | `file.exists("path")` |
| `import std.map` | `map` | `map.new()`, `map.put()` |
| `import std.list` | `list` | `list.new()`, `list.add()` |
| `import std.json` | `json` | `json.parse(str)` |

See [Module System Design](module-system-design.md) for creating your own packages.

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

See [stdlib-api.md](stdlib-api.md) for the full API reference.

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
    0 -> { print("Zero\n") }
    1 -> { print("One\n") }
    _ -> { print("Other\n") }
}
```

### List Patterns

Match on arrays (requires corresponding `_len` variable):

```aether
nums = [1, 2, 3]
nums_len = 3

match (nums) {
    [] -> { print("empty\n") }
    [x] -> { print("one element\n") }
    [h|t] -> {
        print("head: ")
        print(h)
        print("\n")
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

Access command-line arguments in your program:

```aether
main() {
    count = args_count()
    for (i = 0; i < count; i = i + 1) {
        print(args_get(i))
        print("\n")
    }
}
```

Run with arguments: `ae run myprogram.ae -- arg1 arg2`

### Environment Variables

Read configuration from environment variables:

```aether
main() {
    home = getenv("HOME")
    if (home) {
        print("Home directory: ")
        print(home)
        print("\n")
    }
}
```

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
ae version install v0.17.0  # Download and install a specific version
ae version use v0.17.0      # Switch to an installed version
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
- Run specific test category: `./build/test_runner --category=compiler`
- Check for port conflicts if network tests fail (port 8080)

### Common Pitfalls

1. Forgetting to rebuild after changes: run `make clean && make`
2. Actor structs missing the `migrate_to` field (causes struct layout mismatch)
3. On Windows, running `ae` from a directory without write permission (GCC download needs `~\.aether\`)
4. Using `state` as a variable name inside an actor body (it's reserved there — use it freely elsewhere)

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
