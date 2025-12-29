# Testing Guide for Aether

This document describes the comprehensive testing strategy for the Aether programming language.

## Test Categories

### 1. Unit Tests (240+ tests)

#### Compiler Tests
- **Lexer** (39 tests): All tokens, keywords, operators, strings, comments
- **Parser** (29 tests): All language constructs, error handling
- **Type Inference** (10 tests): Primitives, arrays, functions, structs, error detection
- **Code Generation** (15 tests): C code output verification
- **Integration** (15 tests): Full compilation pipeline

#### Runtime Tests
- **String Operations** (10 tests): concat, length, char_at, equals, substring
- **Math Operations** (21 tests): All math functions, edge cases
- **HTTP Client** (3 tests): GET, POST requests
- **TCP Networking** (7 tests): Client/server operations
- **Collections** (7 tests): ArrayList, HashMap
- **JSON** (8 tests): Parsing, stringification

#### Memory Management Tests
- **Arena Allocators** (11 tests): Creation, allocation, reset, scopes, chains
- **Memory Pools** (13 tests): Fixed-size allocation, free lists, standard pools
- **Stress Tests** (11 tests): 10,000+ allocations, cycle testing
- **Leak Detection** (10 tests): Verify no leaks in all operations
- **64-bit Support** (11 tests): Large values, pointer sizes, arithmetic

### 2. Memory Safety Tests

#### Valgrind (Leak Detection)
```bash
make test-valgrind
```

Checks for:
- Definitely lost memory
- Indirectly lost memory
- Possibly lost memory
- Still reachable memory
- Use of uninitialized values
- Invalid reads/writes

#### AddressSanitizer (Runtime Errors)
```bash
make test-asan
```

Detects:
- Heap buffer overflow
- Stack buffer overflow
- Use after free
- Use after return
- Memory leaks
- Double free

#### UndefinedBehaviorSanitizer
```bash
make CFLAGS="-fsanitize=undefined" test
```

Catches:
- Integer overflow
- Null pointer dereference
- Misaligned access
- Division by zero

### 3. Memory Profiling

#### Memory Tracking
```bash
make test-memory
```

Tracks:
- Total allocations/frees
- Current allocations
- Peak memory usage
- Bytes allocated/freed
- Allocation failures

#### Massif (Heap Profiler)
```bash
valgrind --tool=massif ./build/test_runner
ms_print massif.out
```

Profiles:
- Heap usage over time
- Peak memory consumption
- Allocation hotspots

### 4. Performance Benchmarks

```bash
make benchmark
```

Measures:
- Actor message throughput
- Multicore scheduler performance
- Arena vs malloc speed
- Pool vs malloc speed

### 5. 64-bit Architecture Tests

Verifies:
- `int64_t` and `uint64_t` support
- Large value arithmetic
- Pointer size (8 bytes on 64-bit)
- `size_t` for large allocations
- Overflow detection

## CI/CD Pipeline

### Build and Test (All Platforms)
- Ubuntu, macOS, Windows
- GCC compilation
- Full test suite execution

### Code Coverage
- Line coverage with gcovr
- Branch coverage
- Upload to Codecov

### Static Analysis
- cppcheck for warnings
- Performance analysis
- Portability checks

### Memory Check Pipeline
- Valgrind leak detection
- AddressSanitizer on Linux/macOS
- UndefinedBehaviorSanitizer
- Memory profiling with Massif
- Peak memory usage limits

### Example Verification
- Compile all examples
- Verify no compilation errors
- Test example execution

## Running Tests Locally

### Quick Test
```bash
make test
```

### Full Memory Check
```bash
make test-valgrind
make test-asan
make test-memory
```

### With Coverage
```bash
make CFLAGS="-O0 -g --coverage" test
gcovr -r . --html --html-details -o coverage.html
```

### Stress Test
```bash
# Run tests 100 times
for i in {1..100}; do make test || break; done
```

## Test Organization

```
tests/
├── test_harness.h              # Test framework
├── test_harness.c              # Test runner with setjmp/longjmp
├── test_lexer_comprehensive.c  # Lexer tests
├── test_parser_comprehensive.c # Parser tests
├── test_type_inference_comprehensive.c
├── test_codegen_output.c
├── test_compiler_integration.c
├── test_runtime_strings.c
├── test_runtime_math.c
├── test_runtime_http.c
├── test_runtime_net.c
├── test_runtime_collections.c
├── test_runtime_json.c
├── test_memory_arena.c         # Arena allocator tests
├── test_memory_pool.c          # Memory pool tests
├── test_memory_stress.c        # Stress tests
├── test_memory_leaks.c         # Leak detection tests
├── test_64bit.c                # 64-bit architecture tests
└── test_defer.c                # Defer statement tests
```

## Memory Leak Prevention

### Best Practices

1. **Always pair create/destroy**
   ```c
   Arena* arena = arena_create(1024);
   // ... use arena ...
   arena_destroy(arena);  // MUST call
   ```

2. **Use defer in Aether code**
   ```aether
   func process() {
       resource = acquire()
       defer release(resource)
       // Automatic cleanup
   }
   ```

3. **Test with Valgrind**
   ```bash
   valgrind --leak-check=full ./your_program
   ```

4. **Enable memory tracking in development**
   ```c
   #define AETHER_MEMORY_TRACKING
   ```

### Common Leak Patterns

❌ **Missing destroy**
```c
Arena* arena = arena_create(1024);
arena_alloc(arena, 100);
// LEAK: arena_destroy() not called
```

✅ **Proper cleanup**
```c
Arena* arena = arena_create(1024);
arena_alloc(arena, 100);
arena_destroy(arena);  // ✓
```

❌ **Pool not freed**
```c
MemoryPool* pool = pool_create(64, 10);
void* ptr = pool_alloc(pool);
// LEAK: pool_free() and pool_destroy() not called
```

✅ **Proper pool usage**
```c
MemoryPool* pool = pool_create(64, 10);
void* ptr = pool_alloc(pool);
pool_free(pool, ptr);
pool_destroy(pool);  // ✓
```

## Continuous Monitoring

### GitHub Actions

Every commit triggers:
1. Build on 3 platforms
2. Full test suite (240+ tests)
3. Valgrind leak check
4. AddressSanitizer
5. Memory profiling
6. Code coverage
7. Static analysis

### Failure Modes

Tests fail if:
- Any test assertion fails
- Valgrind detects leaks
- ASAN detects errors
- Peak memory > 100MB
- Code coverage drops
- Static analysis warnings

## Performance Targets

- **Arena allocation**: <10ns per allocation
- **Pool allocation**: <20ns per allocation
- **Memory overhead**: <1% of allocated memory
- **Peak memory**: <100MB for test suite
- **Test execution**: <5 seconds total

## Adding New Tests

1. Create test file: `tests/test_yourfeature.c`
2. Include test harness: `#include "test_harness.h"`
3. Write tests using `TEST()` macro
4. Use assertions: `ASSERT_EQ`, `ASSERT_NOT_NULL`, etc.
5. Tests auto-discovered by Makefile
6. Run: `make test`

Example:
```c
#include "test_harness.h"

TEST(my_feature_works) {
    int result = my_function(42);
    ASSERT_EQ(42, result);
}

TEST(my_feature_handles_errors) {
    int result = my_function(-1);
    ASSERT_EQ(0, result);
}
```

## Debugging Failed Tests

### Valgrind Detailed Output
```bash
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose ./build/test_runner
```

### ASAN with Symbolization
```bash
export ASAN_OPTIONS=symbolize=1:halt_on_error=0
make test-asan
```

### GDB Debugging
```bash
gdb ./build/test_runner
(gdb) run
(gdb) bt  # backtrace on crash
```

### Memory Stats
```bash
make test-memory
# Check output for leak warnings
```

## Test Coverage Goals

- **Line coverage**: >90%
- **Branch coverage**: >85%
- **Function coverage**: >95%
- **Memory leak detection**: 100% (zero leaks)
- **Platform coverage**: Windows, Linux, macOS

Current status: ✓ All goals met

