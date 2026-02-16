# Aether Architecture

This document provides an overview of Aether's compiler pipeline, runtime design, and key architectural decisions.

## System Overview

```
+---------------------------------------------------------+
|                     Aether Compiler                      |
|  .ae source -> Lexer -> Parser -> TypeCheck -> Optimizer |
|  -> CodeGen -> .c output                                 |
+---------------------------------------------------------+
                            |
+---------------------------------------------------------+
|                      GCC/Clang                           |
|  .c source -> Native binary                              |
+---------------------------------------------------------+
                            |
+---------------------------------------------------------+
|                    Aether Runtime                        |
|  Scheduler -> Actors -> Memory Pools -> Message Passing  |
+---------------------------------------------------------+
```

## Compiler Pipeline

### 1. Lexer (`compiler/lexer.c`)

**Purpose:** Convert source text into tokens

**Input:** Raw Aether source code (`.ae` file)

**Output:** Stream of tokens

**Process:**
1. Read source character by character
2. Identify token boundaries (whitespace, operators, keywords)
3. Classify tokens (identifier, number, string, keyword, operator)
4. Track line and column numbers for error reporting

**Example:**
```aether
x = 42 + y
```

Produces tokens:
```
IDENTIFIER("x") EQUALS NUMBER(42) PLUS IDENTIFIER("y")
```

**Key Files:**
- `compiler/lexer.c` - Implementation
- `compiler/tokens.h` - Token type definitions

### 2. Parser (`compiler/parser.c`)

**Purpose:** Build Abstract Syntax Tree (AST) from tokens

**Input:** Token stream from lexer

**Output:** AST representing program structure

**Process:**
1. Consume tokens in order
2. Apply grammar rules (recursive descent parsing)
3. Build tree structure with each node representing a language construct
4. Report syntax errors with location information

**Key Concepts:**
- **Recursive Descent:** Each grammar rule is a function
- **Precedence Climbing:** Handles operator precedence
- **Error Recovery:** Attempts to continue parsing after errors

**Key Files:**
- `compiler/parser.c` - Implementation
- `compiler/ast.h` - AST node definitions

### 3. Type Checker (`compiler/type_checker.c`)

**Purpose:** Infer and verify types for local variables

**Input:** AST from parser

**Output:** Type-annotated AST

**Process:**
1. Walk AST and assign type variables to expressions
2. Generate type constraints
3. Solve constraints using unification
4. Report type errors if constraints cannot be satisfied

**Key Algorithms:**
- **Type Inference:** Automatic type deduction for local variables
- **Unification:** Solving type equations
- **Occurs Check:** Preventing infinite types

**Key Files:**
- `compiler/type_checker.c` - Implementation
- `compiler/types.h` - Type definitions

### 4. Optimizer (`compiler/optimizer.c`)

**Purpose:** Apply optimizations to improve performance

**Input:** Type-checked AST

**Output:** Optimized AST

**Optimizations:**

1. **Constant Folding**
   ```aether
   x = 2 + 3 * 4    // Becomes: x = 14
   ```

2. **Dead Code Elimination**
   ```aether
   if (false) {      // Entire block removed
       expensive_call()
   }
   ```

3. **Tail Call Optimization**
   ```aether
   factorial(n, acc) {
       if (n == 0) return acc
       return factorial(n-1, n*acc)  // Optimized to loop
   }
   ```

**Key Files:**
- `compiler/optimizer.c` - Implementation

### 5. Code Generator (`compiler/codegen.c`)

**Purpose:** Generate C code from AST

**Input:** Optimized AST

**Output:** C source code (`.c` file)

**Process:**
1. Walk AST in depth-first order
2. Generate equivalent C code for each node
3. Emit actor runtime calls for actor constructs
4. Generate computed goto dispatch tables for message handlers
5. Detect single-int messages and emit inline fast paths

**Key Features:**
- Computed goto dispatch for message handlers
- Inline single-int message encoding (bypasses pool allocation)
- `scheduler_send_local` / `scheduler_send_remote` routing based on `current_core_id`
- NUMA-aware actor allocation via `scheduler_spawn_pooled` with derived-struct size

**Key Files:**
- `compiler/codegen.c` - Implementation
- `compiler/codegen.h` - API

## Runtime Architecture

### Actor System

**Design:** Lightweight process model with per-core scheduling

**Key Components:**

1. **Actor Structure** (`runtime/scheduler/multicore_scheduler.h`)
   ```c
   typedef struct {
       int active;
       int id;
       Mailbox mailbox;
       void (*step)(void*);
       pthread_t thread;
       int auto_process;
       int assigned_core;
       int migrate_to;        // Affinity hint: core to migrate to (-1 = none)
       int main_thread_only;  // If set, scheduler threads skip this actor
       SPSCQueue spsc_queue;  // Lock-free same-core messaging
   } ActorBase;
   ```

2. **Message Structure** (`runtime/actors/actor_state_machine.h`)
   ```c
   typedef struct {
       int type;
       int sender_id;
       int payload_int;
       void* payload_ptr;
       struct {
           void* data;
           int size;
           int owned;
       } zerocopy;
   } Message;
   ```

3. **Message Queue** (`runtime/scheduler/lockfree_queue.h`)
   - Lock-free SPSC ring buffer with cache-line aligned head/tail
   - Batch enqueue and dequeue to reduce atomic operations
   - Power-of-2 sizing with bitwise AND masking

4. **Message Dispatch** (generated code)
   - Computed goto dispatch table for direct label jumps
   - Message ID read from `msg.type` (not pointer dereference)

### Scheduler

**Design:** Partitioned multicore scheduler with work stealing and message-driven migration

**Architecture:**
```
+---------------------------------------------------------+
|              Partitioned Scheduler                       |
|  +------------+------------+------------+------------+  |
|  |  Core 0    |  Core 1    |  Core 2    |  Core 3    |  |
|  |  [A1, A2]  |  [A3, A4]  |  [A5, A6]  |  [A7]      |  |
|  |  (local)   |  (local)   |  (local)   |  (idle)    |  |
|  |            |            |  <- steal --------+      |  |
|  +------------+------------+------------+------------+  |
+---------------------------------------------------------+
```

**Partitioned Scheduling:**
1. Actors statically assigned to cores at spawn
2. Each core processes only its local actors in the fast path
3. Same-core messages delivered directly to mailbox (no queue overhead)
4. Cross-core messages enqueued to target core's lock-free incoming queue

**Message-Driven Migration:**
1. Cross-core sender sets `migrate_to` hint on target actor
2. Owning scheduler processes actor first, then checks migration hint
3. Actor relocated to sender's core using ascending core-id lock ordering
4. Migrated-actor messages forwarded with spin-retry to prevent drops

**Work Stealing (idle cores):**
1. Core becomes idle after extended empty cycles
2. Scans all cores to find busiest (most pending work)
3. Non-blocking try-lock attempt using ascending core-id ordering
4. Steals one actor if victim has more than 4 actors
5. Stolen actor reassigned to idle core

**NUMA-Aware Allocation:**
1. Detects NUMA topology at scheduler initialization
2. Allocates actor structures on the NUMA node local to the assigned core
3. Falls back to standard malloc on single-node systems
4. Platform support: Linux (libnuma), Windows (VirtualAllocExNuma), macOS (fallback)

**Key Files:**
- `runtime/scheduler/multicore_scheduler.c` - Implementation
- `runtime/scheduler/multicore_scheduler.h` - API and data structures
- `runtime/scheduler/lockfree_queue.h` - Cross-core message queue
- `runtime/aether_numa.c` - NUMA detection and allocation

### Memory Management

**Message Payloads:**
- Thread-local pool of 256 pre-allocated buffers (256 bytes each)
- Pool acquisition and release use plain loads/stores (no atomics for TLS)
- Falls back to malloc for oversized messages or pool exhaustion
- `aether_free_message` returns buffers to the pool or calls `free`

**Actor Allocation:**
- `scheduler_spawn_pooled` allocates via `aether_numa_alloc` with the full derived-struct size
- NUMA-aware placement on the local node of the assigned core
- Falls back to standard allocation on non-NUMA systems

### Performance Optimizations

The runtime implements these optimization strategies in the active code paths:

1. **Main Thread Actor Mode** - Single-actor programs bypass scheduler entirely
2. **Thread-Local Message Pools** - Eliminate malloc/free overhead
3. **Batch Dequeue** - Reduce atomic operations from N to 1 per batch
4. **Adaptive Batching** - Scale batch size with load (64 to 1024)
5. **Direct Mailbox Delivery** - Same-core messages bypass the queue
6. **Computed Goto Dispatch** - Direct label jumps for message handlers
7. **Inline Single-Int Messages** - Bypass pool allocation for common messages
8. **Message-Driven Migration** - Co-locate communicating actors
9. **Optimized Spinlock** - PAUSE/YIELD hints during spin-wait
10. **Cache Line Alignment** - Prevent false sharing on shared structures
11. **Relaxed Atomic Ordering** - Avoid barriers on non-critical counters

See [runtime-optimizations.md](runtime-optimizations.md) for implementation details.

## Module System

**Design:** Static module resolution with circular import detection

**Resolution Process:**
1. Parse import statement: `import std.collections.HashMap`
2. Search paths: `std/collections/HashMap.ae`, `./collections/HashMap.ae`
3. Load and parse module if not already loaded
4. Build dependency graph
5. Detect circular imports using DFS with visited tracking
6. Generate C includes for resolved modules

**Key Files:**
- `compiler/aether_module.c` - Module resolution
- `compiler/aether_module.h` - Module API

## Design Decisions

### Why Compile to C?

**Advantages:**
- Leverage mature C compilers (GCC, Clang)
- Access to optimizations (inlining, loop unrolling, auto-vectorization)
- Portable across platforms
- Direct FFI with C libraries

**Disadvantages:**
- Two-stage compilation adds latency
- Debugging requires looking at generated C
- Some optimizations harder to implement at the source level

### Why Work Stealing?

**Advantages:**
- Automatic load balancing for uneven workloads
- Cache-friendly for local work
- Non-blocking: failed steals do not block progress

**Alternatives Considered:**
- Round-robin: poor load balancing
- Per-core task queue: no load balancing
- Global queue: high contention

### Why Lock-Free Queues?

**Advantages:**
- No mutex contention in the message-passing hot path
- Scales with core count
- Batch operations amortize atomic overhead

**Disadvantages:**
- Complexity (memory ordering, ABA considerations)
- Requires careful attention to cache line alignment

## Trade-Offs

### Compilation Speed vs Runtime Performance
- **Choice:** Favor runtime performance
- **Implication:** Two-stage compilation with aggressive optimization flags
- **Mitigation:** Incremental builds, parallel compilation

### Memory Safety vs Performance
- **Choice:** Arena-based memory management (bulk deallocation, no tracing GC)
- **Implication:** Deterministic cleanup, no GC pauses
- **Mitigation:** Valgrind, AddressSanitizer for leak detection during development

### Portability vs Optimization
- **Choice:** Portable C11 code with platform-specific branches
- **Implication:** Platform-specific intrinsics guarded by `#if` directives
- **Mitigation:** Fallback paths for all platform-specific code

## References

- Type Inference: Pierce & Turner (2000)
- Michael-Scott Queue: Michael & Scott (1996)
- Work Stealing: Blumofe & Leiserson (1999)
- Actor Model: Hewitt, Bishop, Steiger (1973)
- Arena Allocation: Hanson (1990)
