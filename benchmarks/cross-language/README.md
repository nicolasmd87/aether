# Aether Cross-Language Benchmark Suite

Comparative benchmarking of actor/message-passing implementations across languages.

## Quick Start

```bash
./run_benchmarks.sh
```

## Benchmark Patterns

| Pattern | What It Measures |
|---------|-----------------|
| **ping_pong** | Round-trip message latency between two actors |
| **counting** | Single-actor message throughput |
| **thread_ring** | Multi-actor coordination (N actors in ring) |
| **fork_join** | Fan-out/fan-in parallelism |

## Languages & Implementations

| Language | Implementation | Notes |
|----------|---------------|-------|
| **Aether** | Native actors | Lock-free SPSC queues, computed goto dispatch |
| **Go** | Goroutines + channels | Idiomatic Go concurrency |
| **Rust** | std::sync::mpsc | Standard library channels |
| **Erlang** | Native processes | Built-in actor model |
| **Elixir** | Native processes | Built-in actor model |
| **Pony** | Native actors | Reference capabilities |
| **C** | pthreads + mutex | Baseline (no actor framework) |
| **C++** | std::mutex + cv | Baseline (no actor framework) |
| **Zig** | std.Thread | Baseline (no actor framework) |

**Note on C/C++/Zig**: These languages lack native actor support. The implementations use basic synchronization primitives, not actor frameworks like [CAF](https://github.com/actor-framework/actor-framework). Results for these languages represent baseline thread synchronization overhead, not optimized actor performance.

## Metrics

All benchmarks report:
- **ns/msg**: Nanoseconds per message (wall-clock time)
- **Throughput**: Messages per second (M msg/sec)

## Configuration

Edit `benchmark_config.json`:

```json
{
  "messages": 1000000,
  "timeout_seconds": 60
}
```

## What This Does NOT Measure

- I/O performance
- Memory allocation patterns
- GC pauses
- Real-world application workloads
- Distributed messaging

## Methodology

Based on the [Savina Actor Benchmark Suite](https://github.com/shamsimam/savina).

- All languages compiled with `-O3` or equivalent
- No specialized tuning or non-standard optimizations
- Message validation on every exchange
- Results are system-dependent; run on your hardware

## References

- [Savina Benchmark Paper](http://soft.vub.ac.be/AGERE14/papers/ageresplash2014_submission_19.pdf)
- [Savina GitHub](https://github.com/shamsimam/savina)
