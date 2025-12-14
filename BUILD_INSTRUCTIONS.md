# Aether Build Instructions

## Prerequisites

- GCC (GNU Compiler Collection) or compatible C compiler
- GNU Make
- pthread library (usually included with GCC)
- Standard C library

## Building the Compiler

### 1. Navigate to Source Directory

```bash
cd src
```

### 2. Build the New Compiler

```bash
make
```

This will create `aetherc_new`, the new Aether compiler with full lexer, parser, type checker, and code generator.

### 3. Build Runtime Library (Optional)

```bash
make runtime
```

This creates `libaetheruntime.a` for linking with generated C code.

## Building Example Programs

### 1. Compile Aether Source

```bash
./aetherc_new ../examples/hello_actors.ae ../build/hello_actors.c
```

### 2. Compile Generated C Code

```bash
gcc ../build/hello_actors.c ../runtime/*.c -o hello_actors -lpthread
```

### 3. Run the Program

```bash
./hello_actors
```

## Makefile Targets

- `make` - Build the compiler
- `make runtime` - Build runtime library
- `make test` - Compile test program
- `make clean` - Clean build artifacts
- `make install` - Install compiler (optional)

## Troubleshooting

### Compilation Errors

If you get compilation errors:

1. Check GCC version: `gcc --version`
2. Ensure pthread is available: `gcc -lpthread --version`
3. Check file permissions: `ls -la src/`

### Runtime Errors

If programs don't run:

1. Check pthread library: `ldd your_program`
2. Verify runtime files exist: `ls -la runtime/`
3. Check for missing dependencies

### Memory Issues

If you get memory errors:

1. Check available memory: `free -h`
2. Reduce actor count in examples
3. Check for memory leaks with valgrind

## Development Build

For development with debug symbols:

```bash
make CFLAGS="-g -O0 -Wall -Wextra"
```

## Cross-Platform Build

### Windows (MinGW)

```bash
make CC=gcc CFLAGS="-O2 -Wall -Wextra -std=c99"
```

### macOS

```bash
make CC=clang CFLAGS="-O2 -Wall -Wextra -std=c99"
```

### Linux

```bash
make CFLAGS="-O2 -Wall -Wextra -std=c99"
```

## Performance Build

For maximum performance:

```bash
make CFLAGS="-O3 -march=native -Wall -Wextra"
```

## Testing

Run the test suite:

```bash
make test
```

This compiles and runs a basic test program to verify the compiler works correctly.
