# Aether Language

A modern programming language designed for performance and concurrency, featuring actor-based concurrency and compile-time type safety.

## Features

- **Actor-Based Concurrency**: Modern actor model for safe concurrent programming
- **Type Safety**: Compile-time type checking with actor-specific types
- **Performance**: Zero-overhead abstractions with C transpilation
- **Clean Syntax**: Intuitive, readable language design
- **Cross-Platform**: Runs on Windows and Linux

## Quick Start

### Building the Compiler

```bash
cd src
make all
```

Or manually:
```bash
cd src
gcc -O2 -o aetherc aetherc.c lexer.c ast.c parser.c typechecker.c codegen.c ../runtime/actor.c ../runtime/scheduler.c ../runtime/memory.c -lpthread
```

### Your First Aether Program

Create a file `hello.ae`:
```aether
main() {
    print("Hello, Aether World!\n");
    
    int x = 42;
    if (x > 40) {
        print("x is greater than 40!\n");
    }
}
```

### Compile and Run

```bash
# Compile Aether to C
./aetherc hello.ae hello.c

# Compile C to executable
gcc hello.c ../runtime/*.c -o hello -lpthread

# Run the program
./hello
```

## Language Features

### Basic Types
- `int` - Integer numbers
- `float` - Floating point numbers
- `bool` - Boolean values
- `string` - String literals

### Control Flow
- `if/else` statements
- `for` loops
- `while` loops
- `switch` statements

### Functions
```aether
func add(a: int, b: int) -> int {
    return a + b;
}
```

### Actor System (Future)
```aether
actor Counter {
    state int count = 0;
    
    receive(msg) {
        match msg {
            Increment => { count += 1; }
            GetValue(sender) => { send(sender, count); }
        }
    }
}
```

## Project Structure

```
aether/
├── src/                    # Compiler source
│   ├── aetherc.c          # Main compiler
│   ├── lexer.c            # Tokenization
│   ├── parser.c           # AST construction
│   ├── ast.c              # AST nodes
│   ├── typechecker.c      # Type system
│   ├── codegen.c          # C code generation
│   └── Makefile           # Build system
├── runtime/               # Actor runtime
│   ├── actor.c            # Actor lifecycle
│   ├── scheduler.c        # Thread scheduler
│   ├── memory.c           # Memory management
│   └── aether_runtime.h   # Runtime API
├── examples/              # Example programs
│   ├── hello_world.ae     # Simple hello world
│   ├── main_example.ae    # Comprehensive example
│   └── hello_actors.ae    # Actor examples
├── stdlib/                # Standard library
├── docs/                  # Documentation
└── README.md              # This file
```

## Examples

### Hello World
```aether
main() {
    print("Hello, Aether!\n");
}
```

### Variables and Control Flow
```aether
main() {
    int x = 42;
    int y = 8;
    
    if (x > y) {
        print("x (%d) is greater than y (%d)\n", x, y);
    }
    
    for (int i = 0; i < 5; i++) {
        print("Loop iteration: %d\n", i);
    }
}
```

### Functions
```aether
func factorial(n: int) -> int {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

main() {
    int result = factorial(5);
    print("5! = %d\n", result);
}
```

## Building from Source

### Prerequisites
- GCC or compatible C compiler
- Make (optional, for automated builds)
- pthread library

### Windows (MinGW)
```bash
cd src
gcc -O2 -o aetherc.exe aetherc.c lexer.c ast.c parser.c typechecker.c codegen.c ../runtime/actor.c ../runtime/scheduler.c ../runtime/memory.c -lpthread
```

### Linux
```bash
cd src
make all
```

## Development

### Compiler Architecture
1. **Lexer**: Tokenizes Aether source code
2. **Parser**: Builds Abstract Syntax Tree (AST)
3. **Type Checker**: Performs type checking and validation
4. **Code Generator**: Translates AST to C code
5. **Runtime**: Actor system with threading and memory management

### Contributing
1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## Documentation

- [Language Specification](docs/LANGUAGE_SPEC.md)
- [Getting Started Guide](docs/GETTING_STARTED.md)
- [Build Instructions](BUILD_INSTRUCTIONS.md)

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Status

**Current Version**: Proof of Concept (POC)
- ✅ Complete compiler pipeline
- ✅ Basic language features
- ✅ Actor runtime system
- ✅ Type checking
- ✅ C code generation
- 🔄 Advanced actor syntax (in development)
- 🔄 LLVM backend (planned)

## Roadmap

- [ ] Complete actor syntax implementation
- [ ] Advanced type system features
- [ ] LLVM backend for optimization
- [ ] Package management system
- [ ] IDE support and tooling
- [ ] Standard library expansion

---

**Aether Language** - Modern concurrency, clean syntax, high performance.