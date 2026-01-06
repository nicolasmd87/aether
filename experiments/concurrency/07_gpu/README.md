# Experiment 06: GPU Acceleration (CUDA / OpenCL)

## Hypothesis

**For massive actor counts (100K+), GPU's thousands of cores could outperform CPU.**

- NVIDIA RTX 4090: 16,384 CUDA cores
- AMD RX 7900 XTX: 12,288 stream processors
- Each actor = one GPU thread
- Potential: **10-100× speedup** for simple actors

## GPU Architecture Benefits

| Feature | CPU | GPU |
|---------|-----|-----|
| Cores | 8-32 | 1000-16,000 |
| Parallelism | Task-level | Data-level |
| Memory bandwidth | 50 GB/s | 1000 GB/s |
| Best for | Complex logic | Simple parallel ops |

## CUDA Kernel Design

```cuda
__global__ void actor_step_kernel(
    int* counters,           // Actor state
    Message* messages,       // Input messages
    int* mailbox_heads,      // Mailbox pointers
    int actor_count
) {
    int id = blockIdx.x * blockDim.x + threadIdx.x;
    
    if (id < actor_count) {
        // Each GPU thread = one actor
        int msg_idx = mailbox_heads[id];
        Message msg = messages[msg_idx];
        
        counters[id] += msg.payload;
        mailbox_heads[id]++;
    }
}
```

## Performance Model

### CPU→GPU Transfer Cost
- PCIe 4.0: 32 GB/s
- Actor data: 168 bytes × 100K actors = 16 MB
- Transfer time: 0.5 ms
- **Problem:** For 1M messages @ 1ns/msg = 1ms, transfer dominates!

### Strategies to Amortize Transfer

1. **Keep actors on GPU permanently**
   - Only transfer messages
   - Actors never move back to CPU
   
2. **Batch many messages**
   - Transfer 10,000 messages at once
   - Process all on GPU
   - Amortizes transfer cost

3. **Persistent actors**
   - Actors live on GPU between frames
   - Game loop: Update 60× per second
   - No transfer needed

## Test Configurations

### Test 1: Small Actors (PCIe bound)
- 10K actors, 100 messages each
- Expected: CPU wins due to transfer overhead
- Transfer: 2 MB = 0.06 ms
- Compute: 1M msgs = 0.1 ms on GPU
- **CPU wins:** 0.16 ms vs 0.008 ms (CPU 20× faster)

### Test 2: Large Scale (GPU wins)
- 1M actors, 10 messages each
- Expected: GPU wins when compute > transfer
- Transfer: 170 MB = 5 ms
- Compute: 10M msgs = 1 ms on GPU
- **GPU wins:** 6 ms vs 80 ms (CPU) = 13× faster

### Test 3: Persistent GPU Actors
- Actors never leave GPU
- Only messages transferred
- Expected: Best case for GPU

## Implementation Approaches

### ✅ Option A: OpenCL (Cross-platform, GCC compatible)
**Pros:** 
- Works with NVIDIA, AMD, Intel GPUs
- Compiles with GCC (no Visual Studio needed!)
- Included in most GPU drivers
- Cross-platform (Windows, Linux, macOS)

**Cons:** 
- Slightly more verbose API than CUDA
- ~10-20% slower than CUDA on NVIDIA

**Verdict:** **BEST for Aether** - Easy to package, works everywhere

### Option B: CUDA (NVIDIA only)
**Pros:** Best performance on NVIDIA
**Cons:** Requires Visual Studio on Windows, NVIDIA-only

### Option C: Vulkan Compute (Modern)
**Pros:** Modern, cross-platform
**Cons:** Very verbose API, steep learning curve

### ❌ Option D: OpenGL Compute/Textures
**Bad idea:** Using textures as memory is a hack. Compute shaders exist for a reason.

## Expected Results

| Configuration | CPU | GPU | Winner |
|---------------|-----|-----|--------|
| 10K actors × 100 msg | 8 ms | 20 ms | CPU (transfer overhead) |
| 100K actors × 10 msg | 80 ms | 15 ms | GPU (2-5× faster) |
| 1M actors × 10 msg | 800 ms | 60 ms | GPU (13× faster) |
| Persistent GPU actors | 800 ms | 10 ms | GPU (80× faster) |

## Integration Strategy

**Runtime detection:**
```c
if (gpu_available() && actor_count > 50000) {
    use_gpu_scheduler();
} else {
    use_cpu_scheduler();
}
```

**Hybrid approach:**
- Simple actors (counters, math) → GPU
- Complex actors (I/O, branches) → CPU
- **Best of both worlds!**

## Limitations

**GPU won't help when:**
- Actor count <10K (transfer overhead dominates)
- Complex control flow (GPUs bad at branching)
- Blocking I/O (GPU can't wait)
- Irregular memory access

**GPU excels when:**
- Actor count >100K
- Simple, uniform logic
- Data-parallel operations
- Persistent state on GPU

## Implementation Files

- `gpu_bench.cu` - CUDA version (NVIDIA)
- `gpu_bench_opencl.c` - OpenCL version (cross-platform)
- `build_cuda.ps1` - Build script for CUDA
- `build_opencl.ps1` - Build script for OpenCL
