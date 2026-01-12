# Aether Memory Management

Aether uses a lightweight, predictable memory management system designed for performance and safety without the overhead of traditional garbage collection.

## Philosophy

Instead of heavy runtime GC (mark-sweep, generational), Aether combines:

1. **Arena Allocators** - Fast bulk allocation/deallocation
2. **Memory Pools** - Fixed-size object pools
3. **Reference Counting** - For shared strings
4. **Compile-time Analysis** - Future: lifetime tracking

## Arena Allocators

### What are Arenas?

Arenas (also called bump allocators or region allocators) allocate memory from a contiguous block by simply incrementing a pointer. All allocations are freed at once when the arena is destroyed.

### Benefits

- **O(1) allocation** - Just bump a pointer
- **O(1) deallocation** - Free entire arena at once
- **Zero fragmentation** - Linear memory layout
- **Cache-friendly** - Sequential access patterns
- **No GC pauses** - Predictable performance

### Usage

```c
#include "runtime/aether_arena.h"

// Create arena with 1MB
Arena* arena = arena_create(1024 * 1024);

// Allocate objects
int* numbers = arena_alloc(arena, 100 * sizeof(int));
char* buffer = arena_alloc(arena, 1024);

// Use objects...

// Free everything at once
arena_destroy(arena);
```

### Scoped Arenas

```c
Arena* arena = arena_create(4096);

ArenaScope scope = arena_begin(arena);

// Allocate temporary data
void* temp = arena_alloc(arena, 1000);
// Use temp...

// Restore arena to previous state
arena_end(scope);
```

### Automatic Chain Growth

If an allocation exceeds arena capacity, a new arena is automatically chained:

```c
Arena* arena = arena_create(1024);

// This works - creates new arena in chain
void* large = arena_alloc(arena, 1024 * 1024);
```

## Memory Pools

### What are Memory Pools?

Pre-allocated pools for fixed-size objects with O(1) alloc/free via free lists.

### Benefits

- **O(1) allocation and deallocation**
- **No fragmentation**
- **Predictable performance**
- **Ideal for common sizes**

### Usage

```c
#include "runtime/aether_pool.h"

// Create pool for 64-byte objects, 100 capacity
MemoryPool* pool = pool_create(64, 100);

// Allocate
void* obj = pool_alloc(pool);

// Free (returns to pool)
pool_free(pool, obj);

// Destroy pool
pool_destroy(pool);
```

### Standard Pools

Pre-configured pools for common sizes (8, 16, 32, 64, 128, 256 bytes):

```c
StandardPools* pools = standard_pools_create();

// Automatically selects appropriate pool
void* obj = standard_pools_alloc(pools, 50);  // Uses 64-byte pool

standard_pools_free(pools, obj, 50);
standard_pools_destroy(pools);
```

## Memory Tracking

Enable memory statistics tracking:

```c
#define AETHER_MEMORY_TRACKING
#include "runtime/aether_memory_stats.h"

memory_stats_init();

// ... allocations ...

memory_stats_print();
```

Output:
```
========== Memory Statistics ==========
Allocations:
  Total:   1000
  Frees:   1000
  Current: 0
  Peak:    50
  Failures: 0

Bytes:
  Allocated: 102400 (0.10 MB)
  Freed:     102400 (0.10 MB)
  Current:   0 (0.00 MB)
  Peak:      51200 (0.05 MB)

Leak Detection:
  SUPPORTED No memory leaks detected
=======================================
```

## Use Cases

### Actor Messages

Actors use arenas for message processing:

```c
// Per-actor mailbox arena
Arena* mailbox_arena = arena_create(64 * 1024);

// Process messages
while (has_messages()) {
    ArenaScope scope = arena_begin(mailbox_arena);
    
    Message* msg = receive_message();
    process_message(msg, mailbox_arena);
    
    // Free all message allocations
    arena_end(scope);
}
```

### Compilation

Compiler uses arenas for temporary AST nodes:

```c
Arena* parse_arena = arena_create(1024 * 1024);

// Parse creates many temporary nodes
ASTNode* ast = parse(source, parse_arena);

// Generate code
generate_code(ast);

// Free all parse data at once
arena_destroy(parse_arena);
```

### String Operations

Temporary string operations use arenas:

```c
Arena* temp_arena = arena_create(4096);

char* result = arena_alloc(temp_arena, 1024);
strcpy(result, str1);
strcat(result, str2);

// Use result...

arena_destroy(temp_arena);
```

## Testing for Memory Leaks

### Valgrind

```bash
make test-valgrind
```

Runs full test suite with Valgrind leak detection.

### AddressSanitizer

```bash
make test-asan
```

Detects memory leaks, use-after-free, buffer overflows.

### Memory Tracking

```bash
make test-memory
```

Runs tests with built-in memory statistics tracking.

## 64-bit Support

Aether fully supports 64-bit architectures:

- `int64_t` and `uint64_t` types
- Large memory allocations (>4GB)
- 64-bit pointers
- Tested on x86_64 and ARM64

## Performance

Benchmarks show:

- **Arena allocation**: 10-50x faster than malloc
- **Pool allocation**: 5-20x faster than malloc
- **Zero GC pauses**: Predictable latency
- **Low overhead**: <1% memory overhead

## Best Practices

1. **Use arenas for temporary data** - Compilation, message processing, string operations
2. **Use pools for fixed-size objects** - Common data structures
3. **Profile memory usage** - Use memory tracking in development
4. **Test for leaks** - Run Valgrind/ASAN in CI/CD

## CI/CD Integration

Memory checks run automatically on every commit:

- **Valgrind** - Full leak detection
- **AddressSanitizer** - Runtime error detection
- **Memory profiling** - Peak usage tracking
- **64-bit tests** - Architecture verification

See `.github/workflows/memory-check.yml` for details.

