# Changelog

All notable changes to Aether are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.5.0] 

### Added

- **Main Thread Actor Mode**: Single-actor programs now bypass the scheduler entirely for synchronous message processing
  - Zero-copy message passing using caller's stack pointer
  - Automatic detection when only one actor exists
  - Manual control via `AETHER_NO_INLINE` and `AETHER_INLINE` environment variables
- **Memory Profiles**: Configure pool sizes via `AETHER_PROFILE` environment variable (micro/small/medium/large)
- **New Benchmarks**: counting, thread_ring, fork_join benchmark patterns
- **wait_for_idle()**: Block until all actors have finished processing messages
- **sleep(ms)**: Pause execution for specified milliseconds
- **Cross-platform thread affinity**: Linux hard binding, macOS advisory with QoS hints, Windows SetThreadAffinityMask

### Changed

- Scheduler threads now check `main_thread_only` flag with atomic operations to prevent data races
- LSP server uses `snprintf()` instead of `strcat()` for buffer safety
- LSP document management includes proper error handling for memory allocation
- Documentation updated across architecture, scheduler, runtime optimization guides

### Fixed

- Pattern variable renaming in receive blocks
- Race condition when second actor spawns during main thread mode transition
- LSP buffer overflow vulnerability in diagnostics publishing
- LSP memory allocation error handling

### Security

- Fixed potential buffer overflow in LSP diagnostics
- Added bounds checking to all LSP string operations

## [0.4.0]

### Added

- Thread affinity support for all architectures
- Apple Silicon P-core detection for consistent performance
- C interoperability improvements
- NUMA-aware memory allocation
- Computed goto dispatch for message handlers
- Thread-local message pools

### Changed

- Updated documentation for runtime optimizations
- Improved install script with platform detection

### Fixed

- Install script fixes for various platforms

## [0.3.0]

### Added

- Multicore scheduler with work stealing
- Lock-free SPSC queues for cross-core messaging
- Adaptive batch processing
- Message coalescing

### Changed

- Scheduler redesign for partitioned per-core processing

## [0.2.0]

### Added

- Basic actor system
- Message passing primitives
- Type inference
- Pattern matching in receive blocks

## [0.1.0]

### Added

- Initial release
- Lexer, parser, type checker
- Code generation to C
- Basic runtime with single-threaded scheduler
