# Getting Started with Aether

This guide covers installation and basic usage of the Aether programming language.

## Installation

### Quick Install (recommended)

```bash
git clone https://github.com/nicolasmd87/aether.git
cd aether
./install.sh
```

This builds the compiler, CLI tool, and standard library, then installs everything to `~/.aether`. After restarting your terminal (or running `source ~/.zshrc`), the `ae` command is available globally.

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
- Install [MinGW-w64](https://www.mingw-w64.org/) (GCC 11.0+)
- Add MinGW `bin` directory to your PATH
- MSVC is not supported — Aether requires GCC
- Use `mingw32-make ae` instead of the install script

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
import std.collections.HashMap
import std.log as Log

main() {
    Log.info("Starting application")

    map = HashMap.new()
    map.insert("key", "value")

    print(map.get("key"))
}
```

## Standard Library

Aether includes a standard library with the following modules:

### Collections
- **HashMap**: Hash map with Robin Hood hashing
- **Set**: Set operations (union, intersection, difference)
- **Vector**: Dynamic array with amortized O(1) append
- **PriorityQueue**: Binary heap for priority-based scheduling

### Utilities
- **log**: Structured logging with levels
- **fs**: File system operations
- **net**: Networking utilities

See [stdlib-reference.md](stdlib-reference.md) for the full API reference.

## Pattern Matching

Aether supports pattern matching in match statements:

```aether
match (value) {
    0 -> { print("Zero\n") }
    1 -> { print("One\n") }
    _ -> { print("Other\n") }
}
```

List patterns work with arrays (requires corresponding `_len` variable):

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

## Troubleshooting

### Build Failures

**"gcc: command not found" (Windows)**
- Ensure MinGW bin directory is in PATH
- Verify: `gcc --version`

**"pthread.h: No such file or directory"**
- Linux: `sudo apt-get install libpthread-stubs0-dev`
- MinGW includes pthread by default

**Test failures**
- Run specific test category: `./build/test_runner --category=compiler`
- Check for port conflicts if network tests fail (port 8080)

### Common Pitfalls

1. Forgetting to rebuild after changes: run `make clean && make`
2. Using MSVC on Windows: Aether requires GCC (MinGW)
3. Missing `-lpthread` flag when compiling manually
4. Actor structs missing the `migrate_to` field (causes struct layout mismatch)

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
- Use PowerShell, not CMD
- Forward slashes in paths work: `./build/ae`
- Full thread affinity support via SetThreadAffinityMask
