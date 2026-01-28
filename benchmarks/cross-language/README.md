# Aether Cross-Language Benchmark Suite

Comparative benchmarking of actor model implementations across multiple languages.

## Quick Start

```bash
cd benchmarks/cross-language
./run_benchmarks.sh
```

To view results in an interactive dashboard:
```bash
make benchmark-ui
# Open http://localhost:8080
```

## What This Benchmarks

This suite compares baseline actor implementations using a ping-pong message passing test.

**Languages tested:**
- Aether
- C (pthreads)
- C++
- Go
- Rust
- Java
- Zig
- Erlang
- Elixir
- Pony
- Scala (Akka)

**Test characteristics:**
- Ping-pong pattern with full round-trip validation
- Configurable message count (default: 10,000,000)
- All languages use standard optimizations (-O3 equivalent)
- No specialized tuning or non-standard optimizations
- Validates message integrity on every exchange

## What This Measures

- Actor message passing latency
- Basic scheduler overhead
- Message validation performance

## What This Does Not Measure

- I/O performance
- Concurrent workload scaling beyond message passing
- Real-world application performance
- Production-ready system behavior
- Memory allocation patterns
- GC performance

## Configuration

Edit `benchmark_config.json` to adjust parameters:

```json
{
  "messages": 10000000,
  "timeout_seconds": 60
}
```

See `config.md` for detailed configuration options.

## Scripts

- `run_benchmarks.sh` - Main entry point. Runs all benchmarks and generates results.
- `build_server.sh` - Builds the visualization HTTP server (standalone utility).
- `measure_memory.sh` - Cross-platform memory measurement helper (internal use).

## Requirements

**Minimum:**
- Python 3
- GCC or Clang
- GNU sed or BSD sed

**Optional (for additional languages):**
- Java 17+
- Go
- Rust
- Zig
- Erlang/Elixir
- Pony
- sbt (for Scala)

The script will prompt to install missing dependencies.

## Important Notes

- Results are highly system-dependent
- Benchmarks measure baseline implementations only
- Not representative of all workload types
- Intended for comparative analysis on your specific hardware
- Performance varies based on CPU, OS, memory, and system load
- Run on your own system to evaluate performance characteristics

## Stopping the Server

```bash
pkill -f "visualize/server"
```
