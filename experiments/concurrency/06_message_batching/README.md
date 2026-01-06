# Experiment 09: Message Batching

## Hypothesis

**Sending messages one-at-a-time has overhead**. Batch sending could:
- Reduce function call overhead
- Improve cache locality (sequential writes)
- Enable bulk memcpy operations

**Expected speedup:** 2-3× for bulk message workloads

## Current Overhead (One-at-a-Time)

```c
for (int i = 0; i < 1000; i++) {
    send_message(actor, msg);
    // Overhead per call:
    // - Function call (push/pop stack)
    // - Mailbox bound checking
    // - Individual memory write
}
```

**Cost:** 1000 function calls, 1000 bound checks

## Proposed (Batched)

```c
Message batch[1000];
for (int i = 0; i < 1000; i++) {
    batch[i] = msg;
}
send_messages_batch(actor, batch, 1000);
// Overhead:
// - 1 function call
// - 1 bound check (for entire batch)
// - memcpy() for bulk transfer
```

**Cost:** 1 function call, 1 bound check, 1 memcpy

## Test Configurations

### Test 1: Batch Size Sweep
- Batch sizes: 1, 2, 4, 8, 16, 32, 64, 128, 256
- Measure: Throughput vs batch size
- Expected: Logarithmic improvement (diminishing returns)

### Test 2: Cross-Core Batching
- Send batches across cores (tests cache effects)
- Compare to single-message cross-core
- Expected: Larger benefit due to reduced synchronization

### Test 3: Real-World Pattern
- Mixed batch sizes (Poisson distribution)
- Some actors get 1 msg, others get 100
- Measure: Average throughput improvement

## Implementation Status

✅ **Phase 1**: Single-core batching (DONE)
⏳ **Phase 2**: Multi-core batching
⏳ **Phase 3**: Auto-batching (compiler optimization)
