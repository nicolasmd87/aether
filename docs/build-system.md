# Build System

## Overview

Aether uses a multi-tier build system with different optimization profiles for development, testing, and release builds.

## Build Tiers

All builds go through the Makefile, which handles the full source list across subdirectories (`compiler/parser/`, `compiler/analysis/`, `compiler/codegen/`, `runtime/scheduler/`, `runtime/memory/`, etc.).

### Development Build

```bash
make compiler CFLAGS="-O0 -g -Icompiler -Iruntime -Iruntime/actors -Iruntime/scheduler -Wall -Wextra -Wno-unused-parameter"
```

**Purpose**: Fast compilation with debug symbols for active development.

### Testing Build

```bash
make compiler    # Uses -O2 by default
```

**Purpose**: Moderate optimization for CI and testing.

### Release Build

```bash
make compiler CFLAGS="-O3 -march=native -flto -Icompiler -Iruntime -Iruntime/actors -Iruntime/scheduler -Wall -Wextra -Wno-unused-parameter"
```

**Purpose**: Full optimization for production use and benchmarking.

**Flags:**
- `-O3`: Aggressive inlining, loop unrolling, auto-vectorization
- `-march=native`: CPU-specific instruction selection
- `-flto`: Link-time optimization for cross-translation-unit inlining

### Profile-Guided Optimization

```bash
# Stage 1: Instrument
gcc -O3 -march=native -fprofile-generate -o aetherc_pgo ...

# Stage 2: Profile (run representative workload)
./aetherc_pgo <typical_usage>

# Stage 3: Optimize with profile data
gcc -O3 -march=native -fprofile-use -o aetherc ...
```

PGO uses runtime profiling data to improve branch prediction, function inlining decisions, and code layout. It is used by major projects including Chrome, Firefox, CPython, and LLVM for their release builds.

## Incremental Compilation

**Dependency Tracking:**
```makefile
CFLAGS += -MMD -MP
-include $(DEPS)

build/%.o: %.c
    $(CC) $(CFLAGS) -c $< -o $@
```

The `-MMD` flag generates `.d` dependency files listing all headers included by each source file. Make uses these to rebuild only modified files and their dependents.

## Parallel Compilation

```bash
make -j8    # 8 parallel jobs
```

Limited by dependency ordering: some files must build before others.

## Build Recommendations

| Use Case | Flags | Notes |
|----------|-------|-------|
| Development | `-O0 -g` | Fast iteration, debug symbols |
| Testing/CI | `-O2` | Balanced optimization |
| Release | `-O3 -march=native -flto` | Full optimization |
| Profiling | PGO pipeline | Based on representative workload |

## References

- GCC Optimization Options: https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html
- LLVM PGO Guide: https://llvm.org/docs/HowToBuildWithPGO.html
