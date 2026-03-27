# Build System

## Overview

Aether uses a multi-tier build system with different optimization profiles for development, testing, and release builds.

## Project Configuration (`aether.toml`)

Every Aether project has an `aether.toml` at its root. `ae run` and `ae build` read it automatically.

### Minimal project

```toml
[package]
name = "myapp"
version = "0.1.0"

[[bin]]
name = "myapp"
path = "src/main.ae"
```

### Full configuration reference

```toml
[package]
name = "myapp"
version = "1.0.0"
description = "What this program does"

[build]
# Extra C compiler flags applied during `ae build` (release builds only).
# `ae run` uses -O0 for fast iteration regardless of this setting.
cflags = "-O3 -march=native"

# Platform-specific linker flags (e.g. for third-party C libraries).
# macOS/Linux: link_flags = "-lraylib"
# Windows:     link_flags = "-Ldeps/raylib/lib -lraylib -lopengl32 -lgdi32 -lwinmm"
link_flags = ""

[[bin]]
name = "myapp"
path = "src/main.ae"

# Extra C source files compiled alongside the Aether output.
# Useful for C FFI helpers, renderer backends, or any C code your program needs.
# Merged additively with any --extra flags passed on the command line.
extra_sources = ["src/ffi_helpers.c", "src/renderer.c"]
```

### `extra_sources` vs `--extra`

Both add C files to the build — they are additive when both are present.

| | `extra_sources` in `aether.toml` | `--extra file.c` CLI flag |
|---|---|---|
| **Scope** | Always included for this binary | Per-invocation |
| **Good for** | C helpers your program always needs | Renderer backends, platform variants |
| **Works with** | `ae build` and `ae run` | `ae build` and `ae run` |

---

## Build Cache

`ae` caches compiled binaries in `~/.aether/cache/` using an FNV64 hash of source content, compiler mtime, runtime library mtime, and build flags.

**Typical timings:**
- Cache hit: ~8 ms (exec cached binary directly)
- Cache miss: ~300 ms gcc + ~25 ms aetherc
- First macOS run: 1–3 s extra (OS Gatekeeper check, one-time per binary)

```bash
ae cache          # Show cache location and entry count
ae cache clear    # Delete all cached builds
```

Cache entries are invalidated automatically when source or compiler changes.

---

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

## Cross-Compilation (PLATFORM variable)

The `PLATFORM` Makefile variable selects the scheduler backend and sets platform-specific flags:

```bash
# Native (default) — multi-core scheduler, pthreads
make stdlib PLATFORM=native

# WebAssembly — cooperative scheduler, no pthreads/fs/net
make stdlib PLATFORM=wasm    # CC=emcc, -DAETHER_NO_THREADING/FILESYSTEM/NETWORKING

# Or use the ae CLI directly:
ae build --target wasm hello.ae    # Produces hello.js + hello.wasm
node hello.js                       # Run with Node.js

# Embedded — cooperative scheduler, no pthreads/fs/net/getenv
make stdlib PLATFORM=embedded    # -DAETHER_NO_THREADING/FILESYSTEM/NETWORKING/GETENV

# Override individual features on native
make stdlib EXTRA_CFLAGS="-DAETHER_NO_THREADING"    # Auto-selects cooperative scheduler
make stdlib EXTRA_CFLAGS="-DAETHER_NO_FILESYSTEM -DAETHER_NO_NETWORKING"
```

The Makefile auto-detects `AETHER_NO_THREADING` in `EXTRA_CFLAGS` and switches to the cooperative scheduler automatically. It also omits `-pthread` from linker flags.

### Docker-Based Cross-Compilation

For cross-compilation without installing toolchains locally:

```bash
make docker-ci-wasm        # Emscripten SDK → compile + run with Node.js
make docker-ci-embedded    # arm-none-eabi-gcc → syntax-check
make ci-portability        # All: native coop + WASM + embedded
```

Docker images: `docker/Dockerfile.wasm` (Emscripten), `docker/Dockerfile.embedded` (ARM Cortex-M4).

## Build Recommendations

| Use Case | Flags | Notes |
|----------|-------|-------|
| Development | `-O0 -g` | Fast iteration, debug symbols |
| Testing/CI | `-O2` | Balanced optimization |
| Release | `-O3 -march=native -flto` | Full optimization |
| Profiling | PGO pipeline | Based on representative workload |
| WASM | `PLATFORM=wasm` | Cooperative scheduler, Emscripten |
| Embedded | `PLATFORM=embedded` | Cooperative scheduler, no OS |

## References

- GCC Optimization Options: https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html
- LLVM PGO Guide: https://llvm.org/docs/HowToBuildWithPGO.html
