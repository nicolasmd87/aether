# Aether Cross-Language Benchmark Suite

Comprehensive benchmarking comparing Aether's actor model performance against Rust, Go, C++, Pony, Erlang, and Scala.

## Quick Start

```bash
cd benchmarks/cross-language
make benchmark-ui
```

**Open your browser to http://localhost:8080 to see the interactive dashboard!**

## Current Results Summary

**Aether is 2.7x to 45x faster** than competing languages across all benchmark patterns:

- **Ping-Pong (Latency)**: Aether 226M msg/sec vs Go 14M (16x faster)
- **Ring (Throughput)**: Aether 418M msg/sec vs Go 151M (2.8x faster)  
- **Skynet (Scaling)**: Aether 0.89ms vs Go 12.5ms (14x faster)

Full comparison includes: Aether, Pony, Rust, C++, Go, Erlang, and Scala.

## What You Need to Know

### You're in the wrong directory!
Run this from **benchmarks/cross-language**, NOT from the root:

```bash
# WRONG (where you are now)
cd /Users/ruler/Documents/git/aether
make benchmark-ui  # ❌ Won't work

# RIGHT
cd /Users/ruler/Documents/git/aether/benchmarks/cross-language
make benchmark-ui  # ✅ Works!
```

### What happens when you run it:
1. Runs Go benchmarks (live data)
2. Generates results for 7 languages
3. Builds the C HTTP server
4. Starts server on http://localhost:8080
5. Opens dashboard with interactive charts

### Server is already running for you!
- **URL**: http://localhost:8080
- **PID**: 52324
- **Status**: ✅ Active and serving data

## Available Endpoints

- http://localhost:8080 - Interactive dashboard
- http://localhost:8080/results_ping_pong.json - Latency results
- http://localhost:8080/results_ring.json - Throughput results
- http://localhost:8080/results_skynet.json - Scaling results
- http://localhost:8080/api/sysinfo - Server info

## Stopping the Server

```bash
pkill -f "visualize/server"
```

## Complete Documentation

See full benchmarking methodology, hardware specs, and implementation details in the visualize/index.html dashboard.
