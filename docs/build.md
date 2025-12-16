# Build Instructions

## Prerequisites

Windows:
- GCC (MinGW-w64 or similar)
- Git

Linux/Mac:
- GCC or Clang
- Make
- pthread library

## Quick Build

```bash
# Build compiler
gcc -o build/aetherc.exe compiler/*.c -Icompiler -O2

# Compile an Aether program
build/aetherc examples/test_actor_working.ae output.c

# Compile and run generated C code
gcc output.c runtime/multicore_scheduler.c -Iruntime -pthread -o program
./program
```

## Makefile Targets

```bash
make              # Build compiler
make test         # Run test suite
make benchmark    # Run performance benchmarks
make examples     # Compile example programs
make clean        # Remove build artifacts
```

## Multi-Core Programs

Multi-core actor programs require the scheduler:

```bash
# Compile
build/aetherc your_program.ae output.c

# Link with scheduler
gcc output.c runtime/multicore_scheduler.c -Iruntime -pthread -O2 -o program

# Run
./program
```

## Single-Core Programs

For single-core only:

```bash
# Compile
build/aetherc your_program.ae output.c

# Link without scheduler (manual actor management)
gcc output.c -Iruntime -o program
./program
```

## Testing

```bash
# Compiler tests
gcc tests/test_*.c compiler/*.c -Icompiler -o build/test_runner.exe
build/test_runner.exe

# Actor compilation tests
build/aetherc examples/test_actor_working.ae build/test.c
gcc -c build/test.c -Iruntime

# Performance benchmarks
gcc examples/ring_benchmark_manual.c -Iruntime -O2 -o bench
./bench
```

## Optimization Flags

Recommended for production:
```bash
gcc -O3 -march=native -flto ...
```

For debugging:
```bash
gcc -g -O0 -fsanitize=address ...
```
