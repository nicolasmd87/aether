# Aether Concurrency Experiments

**Research Goal**: Identify the optimal concurrency model for lightweight, high-performance actor systems.

## Overview

This directory contains multiple implementations and benchmarks of different concurrency models, each with performance analysis and trade-off documentation.

## Experiment Structure

Each experiment is self-contained with:
- Implementation (C source)
- Benchmark harness
- Performance results
- Analysis document

### 01 - Pthread Baseline (1:1 Threading)
**Model**: One OS thread per actor  
**Status**: ✅ Reference implementation  
**Location**: `01_pthread_baseline/`

Traditional approach using POSIX threads. Each actor runs on a dedicated OS thread with blocking message receives.

**Key Metrics**:
- Memory: 1-8MB per actor (thread stack)
- Scalability: 1,000-10,000 concurrent actors
- Throughput: ~1M messages/second
- Context switch: 1-10μs

### 02 - State Machine Actors (Async/Cooperative)
**Model**: Actors as structs, single-threaded scheduler  
**Status**: ✅ Implemented, benchmarked  
**Location**: `02_state_machine/`

Actors compiled to state machines stored in structs. Single scheduler thread iterates over active actors, calling step functions.

**Key Metrics**:
- Memory: 128 bytes per actor
- Scalability: 1,000,000+ concurrent actors
- Throughput: 125M messages/second
- Context switch: <100ns (function call)

**Trade-off**: Requires non-blocking I/O, compiler transformations.

### 03 - Work-Stealing Scheduler (M:N Threading)
**Model**: Actor queue per worker thread, steal-on-idle  
**Status**: 🚧 Planned  
**Location**: `03_work_stealing/`

Hybrid between 1:1 and state machines. Multiple OS worker threads share a pool of lightweight actor tasks. When idle, workers steal from other queues.

**Expected Metrics**:
- Memory: ~1-2KB per actor (small stack)
- Scalability: 100,000+ concurrent actors
- Throughput: 10-50M messages/second
- Multi-core utilization: High

**Similar to**: Go goroutines, Tokio runtime (Rust)

## Benchmarks

Standard benchmark suite in `benchmarks/`:
- **Ring**: N actors in a ring, pass token M times
- **Broadcast**: One sender, N receivers
- **Tree**: Binary tree of actors, aggregate results
- **Pi Calculation**: Distributed Monte Carlo (compute-heavy)
- **File I/O**: Read/write operations (I/O-heavy)

### Running All Benchmarks

```bash
# Build all experiments
cd experiments
make all

# Run comparison suite
./run_all_benchmarks.sh

# Generate report
python3 analyze_results.py > RESULTS.md
```

## Performance Comparison Table

| Model | Memory/Actor | Max Actors | Throughput | Latency | CPU Cores |
|-------|--------------|------------|------------|---------|-----------|
| Pthread (01) | 1-8 MB | 1K-10K | 1M msg/s | 1-10μs | 1 per actor |
| State Machine (02) | 128 B | 1M+ | 125M msg/s | <100ns | 1 (single thread) |
| Work-Stealing (03) | 1-2 KB | 100K+ | 10-50M msg/s | ~500ns | N (configurable) |

## Research Questions

1. **Scalability vs Complexity**: Does the 100x memory reduction justify the compiler complexity?
2. **I/O Performance**: How do non-blocking I/O wrappers affect latency?
3. **Multi-core**: Can state machines scale to multiple cores without losing efficiency?
4. **Hybrid Viability**: Can we mix blocking (pthread) and async (state machine) actors?

## Methodology

### Hardware Specifications
- **CPU**: [To be filled during benchmarking]
- **RAM**: [To be filled]
- **OS**: Windows 10 / Linux
- **Compiler**: GCC with -O2 optimization

### Measurement Protocol
1. Warm-up phase (1000 iterations, discarded)
2. Measurement phase (10,000 iterations)
3. Statistical analysis (mean, median, std dev, percentiles)
4. Memory profiling with Valgrind/Task Manager

### Reproducibility
All benchmarks are:
- Deterministic (seeded random numbers)
- Repeatable (multiple runs, variance reported)
- Self-contained (single C file per experiment)

## Documentation

**See**: `docs/erlang-go-comparison.md` for detailed technical analysis

We're not working in a vacuum. Erlang (BEAM) and Go have proven that lightweight concurrency works at massive scale. Our experiments directly compare against their approaches:

- **Erlang**: Preemptive green threads with reduction counting (~2.6KB/process)
- **Go**: M:N work-stealing scheduler with goroutines (~2KB/goroutine)  
- **Aether**: Cooperative state machines (128B/actor)

Key insight: By compiling to C instead of using a runtime, we achieve lighter weight at the cost of preemption. The comparison doc covers what we learned, what we're copying, and where we differ.

## Papers & References

Key research papers that informed these experiments:

1. **Erlang/BEAM**:
   - Armstrong, Joe. "Making reliable distributed systems in the presence of software errors." PhD Thesis, 2003.
   
2. **Go Scheduler**:
   - Cox, Russ. "Go's work-stealing scheduler." Go Blog, 2012.
   
3. **Pony Runtime**:
   - Clebsch, Sylvan, et al. "Deny capabilities for safe, fast actors." OOPSLA 2016.

4. **LMAX Disruptor**:
   - Thompson, Martin, et al. "Disruptor: High performance alternative to bounded queues." Technical Report, 2011.

5. **Lightweight Threads Survey**:
   - Behren, Rob von, et al. "Why events are a bad idea (for high-concurrency servers)." HotOS 2003.

## Contributing New Experiments

To add a new concurrency model:

1. Create directory: `experiments/XX_model_name/`
2. Implement: `implementation.c`, `benchmark.c`
3. Document: `README.md` with analysis
4. Add to: `benchmarks/run_all_benchmarks.sh`
5. Update: This overview with results

## Results Summary

**Current Winner (by metric)**:
- **Throughput**: State Machine (125M msg/s)
- **Memory**: State Machine (128B/actor)
- **Simplicity**: Pthread (no compiler changes)
- **Compatibility**: Pthread (works with any C code)
- **Multi-core**: Work-Stealing (TBD)

**Recommendation**: See `docs/ROADMAP.md` for integration plan (hybrid model).
