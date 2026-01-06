# Experiment 05: SIMD Vectorization (AVX2/AVX-512)

## Hypothesis

**Process multiple actors simultaneously using SIMD instructions.**

Current scalar approach processes 1 actor per CPU cycle:
```c
for (int i = 0; i < 8; i++) {
    actors[i].counter += msg.payload;  // 8 cycles
}
```

SIMD approach processes 8 actors per cycle (AVX2) or 16 (AVX-512):
```c
__m256i counters = _mm256_load_si256(&actors[0].counter);
__m256i payload = _mm256_set1_epi32(msg.payload);
__m256i result = _mm256_add_epi32(counters, payload);
_mm256_store_si256(&actors[0].counter, result);  // 1 cycle for 8 actors
```

**Expected speedup:** 4-8× (accounting for memory bandwidth limits)

## SIMD Instruction Sets

| ISA | Width | Elements | Availability |
|-----|-------|----------|--------------|
| SSE2 | 128-bit | 4 × int32 | All x86-64 |
| AVX2 | 256-bit | 8 × int32 | Intel Haswell+ (2013) |
| AVX-512 | 512-bit | 16 × int32 | Intel Skylake-X+ (2017) |

## Test Cases

### Test 1: Simple Counter (Best Case)
- Actors with single counter field
- Increment by constant
- No branches, perfect for SIMD

### Test 2: Conditional Logic
- Actors with if/else branches
- Use SIMD masks for conditional execution
- Measure branch misprediction impact

### Test 3: Mixed Actor Types
- Different actor types in same array
- Requires dynamic dispatch (vtable)
- Likely defeats SIMD benefits

### Test 4: Memory Bandwidth Test
- Very large actor arrays (>L3 cache)
- Measure SIMD speedup vs memory-bound scalar
- Expected: Lower speedup due to memory bottleneck

## Memory Layout Requirements

**Critical:** Actors must be laid out in "Structure of Arrays" (SoA) format:

**Bad (Array of Structures - AoS):**
```c
struct Actor {
    int id;
    int counter;
    int other_field;
};
Actor actors[1000];  // Fields interleaved, hard to vectorize
```

**Good (Structure of Arrays - SoA):**
```c
struct Actors {
    int ids[1000];
    int counters[1000];  // All counters contiguous, easy to vectorize
    int other_fields[1000];
};
```

## Implementation Strategy

### Phase 1: Scalar Baseline
- Measure performance of scalar code
- Establish baseline throughput

### Phase 2: AVX2 Implementation
- Vectorize inner loops
- Handle remainder elements (not multiple of 8)
- Measure speedup

### Phase 3: AVX-512 (if available)
- Use 512-bit vectors (16 elements)
- Compare to AVX2

### Phase 4: Auto-Vectorization Test
- Let compiler vectorize with `-O3 -march=native`
- Compare hand-written SIMD to compiler

## Expected Results

| Scenario | Scalar | AVX2 | AVX-512 | Speedup |
|----------|--------|------|---------|---------|
| Counter (L1 cache) | 125M msg/s | 800M msg/s | 1500M msg/s | 6-12× |
| Counter (DRAM) | 125M msg/s | 300M msg/s | 400M msg/s | 2-3× |
| Conditional | 125M msg/s | 200M msg/s | 250M msg/s | 1.6-2× |
| Mixed types | 125M msg/s | 125M msg/s | 125M msg/s | 1× (no benefit) |

## Integration Plan

**Runtime should:**
1. Detect CPU capabilities (CPUID instruction)
2. Choose best implementation:
   - AVX-512 if available
   - AVX2 if available
   - SSE2 fallback
   - Scalar fallback
3. Apply SIMD to hot loops only (actors with simple state machines)

## Limitations

**SIMD won't help when:**
- Actors have complex control flow (many branches)
- Actor types are heterogeneous (mixed in same batch)
- Memory access is irregular (pointer chasing)
- Working set exceeds cache (memory-bound)

**Best for:**
- Game entities (position updates, physics)
- Financial actors (price aggregation)
- IoT sensors (data processing)
