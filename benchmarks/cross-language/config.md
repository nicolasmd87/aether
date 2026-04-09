# Benchmark Configuration Guide

## System Requirements

The benchmark suite automatically detects available compilers and tools. Benchmarks that can't run are skipped.

### Minimum Requirements
- **Python 3** - For JSON config parsing
- **GCC or Clang** - For C/C++ benchmarks
- **GNU sed** or **BSD sed** - For config application

### Optional Language Support
- **Java 17+** - For Scala/Java benchmarks (auto-detected in standard locations)
- **Go** - For Go benchmark
- **Rust** - For Rust benchmark
- **Zig** - For Zig benchmark
- **Erlang/Elixir** - For BEAM benchmarks
- **Pony** - For Pony benchmark
- **sbt** - For Scala compilation

The script automatically searches common installation paths on both macOS and Linux:
- macOS: `/opt/homebrew`, `/Library/Java/JavaVirtualMachines`
- Linux: `/usr/lib/jvm`, system PATH
- Custom: `$JAVA_HOME` environment variable

## Quick Start

All benchmark parameters are configured in a single JSON file: **`benchmark_config.json`**

```json
{
  "messages": 1000000,
  "timeout_seconds": 60
}
```

Just edit the values and run `make benchmark` - the configuration is applied automatically.

## Configuration Options

### messages
Number of messages to exchange (default: 1,000,000)

- **Higher values**: More accurate results, longer runtime
- **Lower values**: Faster benchmarks, suitable for low-spec systems

### timeout_seconds
Maximum time to wait for each benchmark (default: 60)

## Presets

The config file includes presets for common scenarios:

### Full
```json
"messages": 10000000
```
Full benchmark - 10M messages. Takes 5-30 seconds per language.

### Medium (Default)
```json
"messages": 1000000
```
Default benchmark - 1M messages. Good balance of accuracy and speed.

### Low Spec
```json
"messages": 100000
```
Minimal load - 100K messages. For systems with <4GB RAM or slower CPUs.

### Stress Test
```json
"messages": 100000000
```
Heavy load - 100M messages. May take several minutes per language.

## System Recommendations

| System RAM | CPU Cores | Recommended Messages | Expected Runtime |
|-----------|-----------|---------------------|------------------|
| <4 GB     | 1-2       | 100,000            | ~5 minutes total |
| 4-8 GB    | 2-4       | 1,000,000 (default) | ~10 minutes total |
| 8-16 GB   | 4-8       | 10,000,000         | ~15 minutes total |
| >16 GB    | 8+        | 100,000,000        | ~30+ minutes total |

## Usage Examples

### Use a preset
Edit `benchmark_config.json` to use a preset value:
```json
{
  "messages": 1000000,
  "timeout_seconds": 60
}
```

### Custom configuration
Set any value you want:
```json
{
  "messages": 5000000,
  "timeout_seconds": 120
}
```

### Run benchmarks
```bash
cd benchmarks/cross-language
make benchmark
```

The script automatically:
1. Reads `benchmark_config.json`
2. Updates all language implementations
3. Compiles and runs benchmarks
4. Generates results

## Installing Missing Dependencies

### macOS
```bash
# Java 17+
brew install openjdk@17

# Other languages
brew install go rust zig erlang elixir ponyc sbt
```

### Ubuntu/Debian
```bash
# Java 17+
sudo apt install openjdk-17-jdk

# Other languages
sudo apt install golang rustc erlang elixir
```

### Arch Linux
```bash
# Java 17+
sudo pacman -S jdk17-openjdk

# Other languages
sudo pacman -S go rust zig erlang elixir
```

## Troubleshooting

### Benchmark times out
- Reduce `messages` (try 1,000,000 or 100,000)
- Increase `timeout_seconds` (try 120 or 300)

### Out of memory errors
- Reduce `messages` significantly (try 100,000)
- Close other applications
- Some languages (Java, Scala) use more memory

### Build failures
- Install missing compilers (see main README)
- Check system has required dependencies
- Builds are skipped automatically if tools unavailable

## Architecture Notes

The configuration system works by:
1. `run_benchmarks.ae` reads `BENCHMARK_MESSAGES` from environment
2. Each language is compiled with highest optimization flags
3. The Aether runner parses output and writes JSON results
4. Results are normalized and compared fairly

All languages use the exact same message count for fair comparison.

## Manual Configuration (Not Recommended)

If you need to configure languages individually, you can edit the source files directly:

- **Aether**: `aether/ping_pong.ae` - `total_messages = N`
- **C**: `c/ping_pong.c` - `#define MESSAGES N`
- **C++**: `cpp/ping_pong.cpp` - `constexpr size_t MESSAGES = N`
- **Go**: `go/ping_pong.go` - `const messages = N`
- **Rust**: `rust/src/main.rs` - `const MESSAGES: usize = N`
- **Java**: `java/PingPong.java` - `private static final int MESSAGES = N`
- **Zig**: `zig/ping_pong.zig` - `const messages = N`
- **Elixir**: `elixir/ping_pong.exs` - `@messages N`
- **Erlang**: `erlang/ping_pong.erl` - `-define(MESSAGES, N).`
- **Pony**: `pony/ping_pong.pony` - `if _count < N then`
- **Scala**: `scala/ping_pong.scala` - `val maxCount = NL`

However, this is error-prone and not recommended. Use `benchmark_config.json` instead.
