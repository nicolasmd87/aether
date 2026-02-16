# Aether Performance Benchmarks

## Overview

This document describes the benchmark methodology and available performance tests for the Aether runtime. Benchmarks measure message-passing throughput, latency, and scalability characteristics.

## Benchmark Patterns

### Ping-Pong

Two actors exchange messages in sequence. Tests cross-actor message-passing latency and throughput.

**Workload:**
- 2 actors exchanging messages
- Configurable message count via `BENCHMARK_MESSAGES` environment variable
- Measures round-trip throughput

**Location:** `benchmarks/cross-language/aether/ping_pong.ae`

### Counting

Single actor receives increment messages from the main thread. Tests main thread to actor message throughput.

**Workload:**
- 1 actor
- Main thread sends all messages
- Measures unidirectional throughput

**Notes:**
- Activates Main Thread Actor Mode (synchronous processing, no scheduler overhead)
- Represents best-case single-actor performance

**Location:** `benchmarks/cross-language/aether/counting.ae`

### Thread Ring

N actors arranged in a ring, each forwarding messages to the next. Tests multi-actor coordination and scheduler efficiency.

**Workload:**
- N actors (configurable)
- Token passed around the ring
- Measures ring completion time

**Location:** `benchmarks/cross-language/aether/thread_ring.ae`

### Fork-Join

Master spawns N workers, distributes work, collects results. Tests fan-out/fan-in patterns.

**Workload:**
- 1 master actor, N worker actors
- Master sends tasks, workers reply with results
- Measures parallel dispatch and aggregation

**Location:** `benchmarks/cross-language/aether/fork_join.ae`

## Cross-Language Benchmarks

The `benchmarks/cross-language/` directory contains equivalent implementations across multiple languages for comparative analysis:

- **Aether**: Lock-free SPSC queues, batch dequeue, thread-local message pools
- **Go**: Goroutines with buffered channels
- **Rust**: Tokio async runtime with mpsc channels
- **C++**: std::thread with concurrent queues
- **Erlang**: BEAM VM processes with mailboxes
- **Java**: Thread pools with blocking queues

Results vary based on hardware architecture, OS scheduler, compiler optimization level, and runtime design choices (VM vs native, GC vs manual memory management). Report your specific test environment when sharing results.

See [benchmarks/cross-language/README.md](../benchmarks/cross-language/README.md) for detailed methodology.

## Active Optimization Techniques

### Lock-Free Queues
Single-producer, single-consumer ring buffers for cross-core messaging. Cache-line aligned with power-of-2 sizing for fast modulo.

**Implementation:** `runtime/scheduler/lockfree_queue.h`

### Message Coalescing
Batch dequeue drains multiple messages in a single atomic operation, reducing per-message atomic overhead.

**Implementation:** `runtime/scheduler/multicore_scheduler.c`

### Thread-Local Message Pools
Per-thread pre-allocated buffers eliminate malloc/free on the hot path. Falls back to malloc for oversized messages.

**Implementation:** `runtime/actors/aether_send_message.c`

### Adaptive Batching
Dynamic batch size adjustment based on queue utilization. Range: 64 to 1024 messages.

**Implementation:** `runtime/actors/aether_adaptive_batch.h`

### Inline Single-Int Messages
Messages with exactly one integer field bypass pool allocation. The value is stored directly in `Message.payload_int`.

**Implementation:** `compiler/backend/codegen.c`

### Computed Goto Dispatch
Message handlers use a dispatch table with GCC computed goto for direct label jumps.

**Implementation:** `compiler/backend/codegen.c` (generated code)

## Methodology

### Measurement Approach

- High-precision timing (`clock_gettime` with `CLOCK_MONOTONIC`)
- Multiple runs with warmup periods
- Isolated processes to minimize interference
- Compiler flags: `-O3 -march=native -flto`
- Correctness validation before measurement

### Hardware Specifications

Document your test environment when reporting results:
- CPU model, core count, and clock speed
- Operating system and version
- Compiler and version
- Memory configuration

### Statistical Validity

- Multiple iterations to account for variance
- Median values to avoid outlier bias
- Both cold-start and warm-cache scenarios considered

## Running Benchmarks

### Cross-Language Suite

```bash
cd benchmarks/cross-language
./run_benchmarks.sh
```

### Aether Only

```bash
cd benchmarks/cross-language/aether
make
./ping_pong
```

## Interpreting Results

### Key Metrics

- **Throughput**: Messages processed per second
- **Latency**: Time from send to receive (per round-trip)

### Considerations

- Single-node architecture (no distributed messaging)
- Arena-based memory management (no tracing GC pauses)
- Platform-specific optimizations (PAUSE, YIELD, NUMA)
- Cross-language comparisons involve fundamentally different runtime models

## References

- [Cross-Language Benchmarks](../benchmarks/cross-language/)
- [Runtime Optimizations](runtime-optimizations.md)
- [Scheduler Architecture](scheduler-quick-reference.md)
