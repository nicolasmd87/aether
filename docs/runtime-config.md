# Runtime Configuration - Quick Reference

## What Does "Lock-Free" Mean?

**Simple Explanation:**

**With Locks (Old):**
```
Thread 1: Lock → Do Work → Unlock
Thread 2: [WAITING...] Lock → Do Work → Unlock  ← Blocked!
```
One thread blocks others. Like a single bathroom - everyone waits in line.

**Lock-Free (New):**
```
Thread 1: Do Work (atomic operations)
Thread 2: Do Work (atomic operations)  ← No blocking!
```
Multiple threads work simultaneously. Like having multiple bathrooms - no waiting.

**Benefits:**
- **No waiting** - threads never block each other
- **Faster** - 1.5-2x speedup from eliminating mutex locks
- **Scalable** - performance improves with more cores

---

## Full Integration Complete SUPPORTED

### What Was Implemented

1. **Lock-Free Message Pools** SUPPORTED
   - TLS pools have no mutex
   - Zero contention on hot path
   - Automatic per-thread allocation

2. **Lock-Free Mailboxes** SUPPORTED
   - SPSC (Single Producer Single Consumer) atomic queue
   - 64-byte cache line padding
   - Runtime switchable

3. **Runtime Configuration API** SUPPORTED
   - Control optimizations via flags
   - Auto-detect CPU features
   - Best out-of-the-box experience

---

## Usage

### Option 1: Auto-Detect (Recommended)
```c
#include "runtime/aether_runtime.h"

int main() {
    // One line - automatically enables best optimizations
    aether_runtime_init(0, AETHER_FLAG_AUTO_DETECT);
    
    // Your code here...
    
    aether_runtime_shutdown();
}
```

**Example result on modern CPU:**
- Lock-free mailboxes: SUPPORTED ENABLED
- Lock-free pools: SUPPORTED ENABLED  
- MWAIT idle: SUPPORTED ENABLED
- AVX2 SIMD: SUPPORTED ENABLED
- **Optimal performance configuration**

---

### Option 2: Manual Control
```c
// Explicit flags - full control
aether_runtime_init(8,
    AETHER_FLAG_LOCKFREE_MAILBOX |  // Use lock-free mailboxes
    AETHER_FLAG_LOCKFREE_POOLS   |  // Use lock-free TLS pools
    AETHER_FLAG_ENABLE_SIMD      |  // Use AVX2 vectorization
    AETHER_FLAG_ENABLE_MWAIT);      // Use MWAIT for idle
```

---

### Option 3: Compatibility Mode
```c
// No optimizations - works on any CPU
aether_runtime_init(4, 0);  // 4 cores, no flags
```

---

### Option 4: Debug/Verbose
```c
// Print configuration on startup
aether_runtime_init(0, 
    AETHER_FLAG_AUTO_DETECT | 
    AETHER_FLAG_VERBOSE);

// Output:
// ========================================
//   Aether Runtime Configuration
// ========================================
// CPU: <detected processor>
// Active Optimizations:
//   Lock-free mailbox: ENABLED
//   Lock-free pools:   ENABLED
//   ...
```

---

## Available Flags

| Flag | Hex | Description |
|------|-----|-------------|
| `AETHER_FLAG_AUTO_DETECT` | 0x01 | Auto-detect CPU features (recommended) |
| `AETHER_FLAG_LOCKFREE_MAILBOX` | 0x02 | Use lock-free SPSC mailboxes |
| `AETHER_FLAG_LOCKFREE_POOLS` | 0x04 | Use lock-free TLS message pools |
| `AETHER_FLAG_ENABLE_SIMD` | 0x08 | Enable AVX2 vectorization |
| `AETHER_FLAG_ENABLE_MWAIT` | 0x10 | Enable MWAIT idle strategy |
| `AETHER_FLAG_VERBOSE` | 0x20 | Print configuration on init |

**Combine flags with bitwise OR:**
```c
int flags = AETHER_FLAG_LOCKFREE_MAILBOX | 
            AETHER_FLAG_LOCKFREE_POOLS;
aether_runtime_init(8, flags);
```

---

## Performance Expectations

| Configuration | Throughput | Latency | Use Case |
|--------------|------------|---------|----------|
| **Maximum** (all optimizations) | 3.1B msg/sec (skynet) | 0.96 cycles/msg | Modern CPUs (2013+) |
| **High** (TIER 1 + TIER 2) | 418M msg/sec (ring) | 7.18 cycles/msg | AVX2-capable CPUs |
| **Moderate** (TIER 1 only) | 226M msg/sec (ping-pong) | 13.29 cycles/msg | All platforms |

See [benchmarks/cross-language](../benchmarks/cross-language/) for detailed measurements.

---

## Examples

### Example 1: Simple Program
```c
#include "runtime/aether_runtime.h"
#include "runtime/actors/aether_actor.h"

void my_actor_process(Actor* self, void* msg, int size) {
    // Process message
}

int main() {
    // Initialize with auto-detect
    aether_runtime_init(0, AETHER_FLAG_AUTO_DETECT);
    
    // Create actor (automatically uses configured optimizations)
    Actor* actor = aether_actor_create(my_actor_process);
    aether_actor_start(actor);
    
    // Send messages...
    
    aether_actor_stop(actor);
    aether_actor_destroy(actor);
    aether_runtime_shutdown();
    return 0;
}
```

### Example 2: Query Configuration
```c
const AetherRuntimeConfig* config = aether_runtime_get_config();

if (config->use_lockfree_mailbox) {
    printf("Using lock-free mailboxes!\n");
}

if (config->use_simd) {
    printf("AVX2 SIMD enabled\n");
}
```

### Example 3: Feature Check
```c
if (aether_runtime_has_feature(AETHER_FLAG_ENABLE_MWAIT)) {
    printf("MWAIT is enabled\n");
}
```

---

## Testing

### Check CPU Features
```bash
./build/cpu_info.exe
```

### Run Configuration Example
```bash
./build/runtime_example.exe
```

---

## How It Works Internally

### Mailbox Selection
```c
// In aether_actor_create()
Actor* actor = aether_actor_create(process_fn);

// Runtime automatically selects mailbox type:
if (runtime_config->use_lockfree_mailbox) {
    // Use atomic SPSC queue (no mutex)
    lockfree_mailbox_init(&actor->mailbox.lockfree);
} else {
    // Use simple ring buffer
    mailbox_init(&actor->mailbox.simple);
}
```

### Message Send
```c
// In aether_send_message()
if (actor->use_lockfree) {
    lockfree_mailbox_send(&actor->mailbox.lockfree, msg);  // Atomic
} else {
    mailbox_send(&actor->mailbox.simple, msg);  // Simple
}
```

### Message Pool Allocation
```c
// In message_pool_alloc()
if (pool->is_thread_local) {
    // LOCK-FREE PATH (no mutex)
    buffer = pool->buffers[pool->head];
    pool->head = (pool->head + 1) % MESSAGE_POOL_SIZE;
    return buffer;
} else {
    // Shared pool (with mutex)
    pthread_mutex_lock(&pool->lock);
    // ...
}
```

---

## Best Practices

1. **Always use `AETHER_FLAG_AUTO_DETECT`** unless you have a specific reason not to
2. **Use `AETHER_FLAG_VERBOSE`** during development to see what's enabled
3. **Don't recompile** for different CPUs - auto-detect handles it
4. **Test on target hardware** to verify optimizations are available
5. **Profile first** - measure actual performance gains

---

## Cross-Platform Support

| Platform | Lock-Free | MWAIT | SIMD | Status |
|----------|-----------|-------|------|--------|
| **Intel x86** (2013+) | SUPPORTED | SUPPORTED | AVX2 | Full support |
| **AMD Ryzen** | SUPPORTED | SUPPORTED | AVX2 | Full support |
| **Old x86** (pre-2013) | SUPPORTED | NO | SSE4.2 | Partial |
| **ARM** | SUPPORTED | NO (uses WFE) | NEON | Fallbacks active |
| **Other** | SUPPORTED | NO | NO | Baseline |

Runtime automatically uses best available optimizations for each platform.

---

## Troubleshooting

### Issue: "Optimization not enabled"
**Solution:** Use `AETHER_FLAG_VERBOSE` to see why:
```c
aether_runtime_init(0, AETHER_FLAG_AUTO_DETECT | AETHER_FLAG_VERBOSE);
```

### Issue: "Performance not as expected"
**Check:**
1. CPU actually supports features (`./build/cpu_info.exe`)
2. Flags are set correctly
3. Actors created after `aether_runtime_init()`

### Issue: "Works on development machine, not on server"
**Reason:** Different CPU capabilities
**Solution:** Use `AETHER_FLAG_AUTO_DETECT` - it adapts automatically
