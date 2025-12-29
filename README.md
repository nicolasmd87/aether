# Aether Programming Language

A systems language combining Erlang-inspired concurrency with ML-family type inference and C performance.

## Overview

Aether brings together the best of multiple paradigms: Erlang's lightweight actor-based concurrency, ML/Haskell's automatic type inference (Hindley-Milner), and C's raw performance. Write clean, expressive code without type annotations, and the compiler infers types before generating optimized C code.

```aether
x = 42
name = "Alice"
nums = [1, 2, 3]

main() {
    print(x)
}
```

The above compiles to typed C code with no runtime overhead.

## Key Features

- **Erlang-inspired actors** - Lightweight concurrent actors with message passing
- **ML-family type inference** - Hindley-Milner style automatic type deduction
- **Clean syntax** - No explicit type declarations or verbose keywords
- **C performance** - Compiles to readable, optimized C code
- **Zero-cost abstractions** - High-level features with no runtime overhead
- **Fast memory management** - Arena allocators and memory pools
- **Defer statement** - Scope-based automatic cleanup
- **VS Code/Cursor support** - Full LSP integration with autocomplete

## Comparisons

| Feature | Aether | Erlang | Go | OCaml |
|---------|--------|--------|----|----|
| Concurrency | Actors (like Erlang) | Actors | Goroutines | None built-in |
| Type System | Static + inference | Dynamic | Static + explicit | Static + inference |
| Performance | C-level | VM-based | Fast | Fast |
| Compilation | To C | To BEAM | To native | To native/bytecode |
| Memory Model | Manual + arenas | GC | GC | GC |
| Syntax | Clean, minimal | Erlang-style | C-like | ML-style |

**Aether's sweet spot**: Erlang's concurrency model + ML's type safety + C's performance

## Current Status

**Phase 3 Complete - Actors Fully Functional**

The compiler is fully functional with working actor-based concurrency:
- Compiler builds and runs on Windows/Linux/macOS
- Full type inference for primitives, arrays, structs, and functions
- Python-style syntax (no explicit types needed)
- Code generation produces optimized C code
- Control flow structures (if/while/for) fully supported
- Actor runtime complete - spawn/send/receive working
- Message passing between actors
- Actor state management
- Multicore scheduler ready
- Arena allocators for fast memory management
- Memory pools for fixed-size allocations
- Defer statement for automatic cleanup
- LSP server with IDE integration
- VS Code/Cursor extension

**Test Coverage:** 240+ comprehensive tests covering:
- Lexer, parser, type inference, code generation
- Memory management (arenas, pools, leak detection, stress tests)
- 64-bit architecture support
- Standard library (string, math, I/O, HTTP, TCP/networking, collections, JSON)

**Memory Safety:**
- Valgrind leak detection in CI/CD
- AddressSanitizer for runtime errors
- Memory profiling and statistics tracking
- Comprehensive stress tests

**Current Development** (v0.2.0):
- Package manager (`apkg`) - Basic CLI complete
- Module system - In progress
- Expanded standard library - Logging, file system, HTTP server planned

**Next Phase:** Complete module system, add logging library, expand stdlib, implement pattern matching.

## Installation

### Quick Install

**Linux/macOS:**
```bash
curl -sSL https://raw.githubusercontent.com/yourusername/aether/main/install.sh | bash
```

Or clone and install:
```bash
git clone https://github.com/yourusername/aether.git
cd aether
chmod +x install.sh
./install.sh
```

**Windows (PowerShell):**
```powershell
git clone https://github.com/yourusername/aether.git
cd aether
.\install.ps1
```

### Requirements
- GCC (MinGW on Windows, native on Linux/macOS)
- Make (optional, can build with scripts)

### From Source

**Windows:**
```powershell
.\build_compiler.ps1
```

**Linux/Mac:**
```bash
make
make lsp  # Build LSP server
```

## Memory Management

Aether uses lightweight, predictable memory management:

- **Arena Allocators** - 10-50x faster than malloc for bulk allocations
- **Memory Pools** - O(1) alloc/free for fixed-size objects
- **Defer Statement** - Automatic scope-based cleanup
- **Zero GC Pauses** - Predictable performance
- **Leak Detection** - Valgrind + AddressSanitizer in CI/CD

See [docs/memory-management.md](docs/memory-management.md) for details.

## IDE Support

Aether includes VS Code/Cursor extension with LSP support for:
- Syntax highlighting
- Autocomplete (keywords, functions, standard library)
- Hover documentation
- Error diagnostics

### Install VS Code/Cursor Extension

**Linux/macOS:**
```bash
cd editor/vscode
./install.sh
```

**Windows:**
```powershell
cd editor\vscode
.\install.ps1
```

The extension will automatically find the `aether-lsp` server if it's in your PATH. After installation, restart your editor.

## Usage

### Quick Run
```bash
aether run examples/basic/hello_world.ae
```

### Compile to C
```bash
aether build program.ae program.c
```

### Manual Compilation
```bash
# Compile Aether to C
./build/aetherc program.ae output.c

# Compile C to executable
gcc output.c -Iruntime runtime/*.c -o program -lpthread

# Run
./program
```

## Examples

### Type Inference
```aether
x = 42          // inferred as int
pi = 3.14       // inferred as float
name = "Alice"  // inferred as string
nums = [1, 2, 3] // inferred as int[3]

main() {
    print(x)
}
```

### Functions
```aether
add(a, b) {
    return a + b
}

main() {
    result = add(10, 20)
    print(result)
}
```

### Structs
```aether
struct Point {
    x,
    y
}

main() {
    p = Point{ x: 10, y: 20 }
    print(p.x)
}
```

### Actors (fully working!)
```aether
actor Counter {
    state count = 0
    
    receive(msg) {
        if (msg.type == 1) {
            count = count + 1
        }
    }
}

main() {
    c = spawn_Counter()
    send_Counter(c, 1, 0)
    Counter_step(c)
    print("Counter incremented!")
}
```

## Known Limitations

- Pattern matching (match statements) not yet implemented
- Limited standard library (expanding)
- No package system yet
- Error messages could be more descriptive
- No IDE integration beyond basic syntax highlighting

## Project Structure

- `compiler/` - Lexer, parser, type checker, code generator
- `runtime/` - Runtime library (strings, memory, actors)
- `examples/` - Example programs and tests
- `docs/` - Language specification and guides

## Documentation

- `docs/language-reference.md` - Language specification
- `docs/type-inference-guide.md` - Type system details
- `docs/runtime-guide.md` - Runtime API

## License

MIT
