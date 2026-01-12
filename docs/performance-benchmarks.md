# Aether Performance Benchmarks

**Last Updated:** January 2026
**Test Platform:** Apple M1 Pro, 8 cores, Darwin
**Workload:** Actor message passing benchmarks

## Current Performance Metrics

### Cross-Language Benchmark Results

Comprehensive benchmarks comparing Aether against established actor systems and concurrent runtimes.

| Pattern | Aether | Go | Rust | C++ | Erlang | Java |
|---------|--------|----|----|-----|--------|------|
| **Ping-Pong** (2 actors) | 226M msg/sec | 4.0M | 4.3M | 15M | 8.5M | 0.27M |
| **Ring** (100 actors) | 418M msg/sec | 131M | 95M | 78M | 42M | - |
| **Skynet** (1111 actors) | 3.1B msg/sec | 88K actors/sec | 73K | 59K | 50K | 31K |

**Latency** (cycles per message):
- Ping-Pong: 13.29 cycles/msg
- Ring: 7.18 cycles/msg
- Skynet: 0.96 cycles/msg (sub-nanosecond)

**Memory Usage**:
- Ping-Pong: 2.1 MB
- C pthread baseline: 0.96 MB

See [benchmarks/cross-language/methodology.md](../benchmarks/cross-language/methodology.md) for detailed methodology.

## Benchmark Patterns

### Ping-Pong
Two actors exchange messages. Tests basic message passing latency and throughput.

**Workload:**
- 2 actors
- 10 million messages
- Measures: throughput, latency, memory

**Results:**
- Aether: 226M msg/sec, 13.29 cycles/msg
- vs Go: 56x faster
- vs Java: 837x faster

### Ring
100 actors in a ring topology pass messages. Tests routing efficiency and multi-actor coordination.

**Workload:**
- 100 actors in ring
- 100K rounds (10M total messages)
- Measures: throughput under coordination

**Results:**
- Aether: 418M msg/sec, 7.18 cycles/msg
- vs Go: 3.2x faster
- vs C++: 5.4x faster

### Skynet
Hierarchical actor tree with 1111 actors. Tests scaling and actor creation overhead.

**Workload:**
- 1111 actors (tree of 10 nodes, each spawning 10 children)
- Tests: actor creation speed, tree messaging

**Results:**
- Aether: 1.25M actors/sec, 0.89ms total time
- vs Go: 14x faster actor creation
- vs Rust: 17x faster
- vs Erlang: 25x faster

## Optimization Techniques

### Lock-Free SPSC Queues
Single-producer, single-consumer queues for same-core messaging.

**Implementation:** `runtime/actors/aether_spsc_queue.h`

**Performance:** 2-3x improvement over mutex-based queues for same-core messaging.

### Message Coalescing
Batch processing of messages to amortize atomic operations.

**Implementation:** `runtime/scheduler/multicore_scheduler.c`

**Configuration:** `COALESCE_THRESHOLD = 512` messages

**Performance:** 15x throughput improvement at high message rates.

### Sender-Side Batching
Thread-local send buffers accumulate messages before flushing.

**Implementation:** `runtime/actors/aether_send_buffer.h`

**Batch Size:** 256 messages optimal

**Performance:** 1.78x speedup (batch_256 vs single sends).

### Actor Pooling
Pre-allocated actor pool to eliminate allocation overhead.

**Implementation:** `runtime/actors/aether_actor_pool.h`

**Pool Size:** 256 actors per type

**Performance:** 1.81x speedup.

### Zero-Copy Message Passing
Transfer ownership of large payloads without copying.

**Implementation:** `runtime/actors/aether_zerocopy.h`

**Applicability:** Messages > 256 bytes

**Performance:** 4.8x improvement for large messages.

### SIMD Batch Processing
AVX2 vectorization for batch operations.

**Implementation:** `runtime/actors/aether_simd_batch.h`

**Requirements:** AVX2-capable CPU

**Performance:** 1.5x improvement for compute-heavy handlers.

## Methodology

All benchmarks use:
- High-precision timing (RDTSC on x86_64, clock_gettime on ARM)
- Multiple runs with warmup
- Isolated processes
- Same hardware for all languages
- Best practices for each language

Each language implementation uses idiomatic patterns:
- Go: Goroutines with buffered channels
- Rust: Tokio async with mpsc channels
- C++: std::thread with queues
- Erlang: BEAM VM processes
- Java: ArrayBlockingQueue
- Aether: Lock-free SPSC queues with batching

## Performance Characteristics

**Strengths:**
- Extremely low latency (sub-nanosecond per message in skynet)
- High throughput (226M-3.1B msg/sec depending on pattern)
- Low memory overhead (2.1 MB for ping-pong)
- Scales well with actor count

**Considerations:**
- Single-node only (no distribution)
- Manual memory management
- Requires C compilation toolchain

## Running Benchmarks

```bash
cd benchmarks/cross-language

# Quick benchmark (1 run per language)
./quick_bench.sh

# Statistical analysis (5 runs + warmup)
bash run_statistical_bench.sh

# Start web UI
make benchmark-ui
# Open http://localhost:8080
```

## Hardware Specifications

All benchmarks run on:
- CPU: Apple M1 Pro (3.2GHz, 8 cores)
- OS: macOS (Darwin)
- Memory: Dedicated process memory
- Compiler: Clang with -O3 optimization

Results will vary on different hardware but relative performance should remain consistent.

## References

- [Cross-Language Benchmarks](../benchmarks/cross-language/)
- [Benchmark Methodology](../benchmarks/cross-language/methodology.md)
- [Runtime Optimizations](runtime-optimizations.md)
- [Memory Management](memory-management.md)
