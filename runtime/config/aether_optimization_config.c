// Aether Runtime Optimization Configuration - Implementation
// See aether_optimization_config.h for documentation

#include "aether_optimization_config.h"
#include "../utils/aether_cpu_detect.h"
#include <stdio.h>
#include <string.h>

// Global hardware capabilities (auto-detected once)
AetherHardwareCapabilities g_hw_caps = {0};

// Global runtime config with defaults
AetherRuntimeConfig g_aether_config = {
    .use_lockfree_mailbox = false,  // Opt-in (slower single-thread)
    .use_message_dedup = false,     // Opt-in (changes semantics)
    .verbose = false,
    .actors_pooled = 0,
    .actors_malloced = 0,
    .direct_sends = 0,
    .queue_sends = 0,
    .batches_adjusted = 0,
    .simd_batches = 0,
    .messages_deduped = 0
};

static int g_hw_detected = 0;

// Detect hardware capabilities (called once at startup)
void aether_detect_hardware(void) {
    if (g_hw_detected) return;
    g_hw_detected = 1;
    
    const CPUInfo* cpu = cpu_get_info();
    
    // SIMD detection
    if (cpu->avx2_supported) {
        g_hw_caps.simd_available = true;
        g_hw_caps.simd_width = 256;  // AVX2 = 256 bits
    } else if (cpu->sse42_supported) {
        g_hw_caps.simd_available = true;
        g_hw_caps.simd_width = 128;  // SSE4.2 = 128 bits
    }
#if defined(__aarch64__)
    // ARM NEON is always available on 64-bit ARM
    g_hw_caps.simd_available = true;
    g_hw_caps.simd_width = 128;  // NEON = 128 bits
#endif

    // AVX-512 check (rare but fast)
    if (cpu->avx512f_supported) {
        g_hw_caps.simd_width = 512;
    }
    
    // MWAIT detection (power-efficient idle)
    g_hw_caps.mwait_available = cpu->mwait_supported;
    
    // CPU pinning detection (OS-dependent)
    // Linux: pthread_setaffinity_np, macOS: thread_policy_set, Windows: SetThreadAffinityMask
#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
    g_hw_caps.cpu_pinning_available = true;
#else
    g_hw_caps.cpu_pinning_available = false;
#endif
    
    // Cache line size
    g_hw_caps.cache_line_size = cpu->cache_line_size > 0 ? cpu->cache_line_size : 64;
}

// Configure runtime with user flags
void aether_runtime_configure(AetherOptFlags flags) {
    // Always detect hardware first
    aether_detect_hardware();
    
    // Apply user's opt-in flags
    atomic_store(&g_aether_config.use_lockfree_mailbox, (flags & AETHER_OPT_LOCKFREE_MAILBOX) != 0);
    atomic_store(&g_aether_config.use_message_dedup, (flags & AETHER_OPT_MESSAGE_DEDUP) != 0);
    atomic_store(&g_aether_config.verbose, (flags & AETHER_OPT_VERBOSE) != 0);
    
    // Print config if verbose
    if (flags & AETHER_OPT_VERBOSE) {
        aether_print_config();
    }
}

// Print current configuration
void aether_print_config(void) {
    printf("\n=== Aether Runtime Optimization Configuration ===\n\n");
    
    printf("TIER 1 - ALWAYS ON (built-in):\n");
    printf("  [ON] Actor Pooling        - 1.81x faster allocation\n");
    printf("  [ON] Direct Send          - Same-core bypass\n");
    printf("  [ON] Adaptive Batching    - Dynamic 4-64 batch size\n");
    printf("  [ON] Message Coalescing   - 15x throughput boost\n");
    printf("  [ON] TLS Message Pools    - Eliminates mutex contention\n");
    printf("\n");
    
    printf("TIER 2 - AUTO-DETECTED (hardware-dependent):\n");
    printf("  [%s] SIMD Processing      - %d-bit vectors%s\n", 
           g_hw_caps.simd_available ? "ON" : "OFF",
           g_hw_caps.simd_width,
           g_hw_caps.simd_available ? "" : " (CPU lacks AVX2/NEON)");
    printf("  [%s] MWAIT Idle           - Sub-µs wake latency%s\n",
           g_hw_caps.mwait_available ? "ON" : "OFF",
           g_hw_caps.mwait_available ? "" : " (CPU lacks MONITOR/MWAIT)");
    printf("  [%s] CPU Pinning          - Thread affinity%s\n",
           g_hw_caps.cpu_pinning_available ? "ON" : "OFF",
           g_hw_caps.cpu_pinning_available ? "" : " (OS unsupported)");
    printf("\n");
    
    printf("TIER 3 - OPT-IN (user flags, have trade-offs):\n");
    printf("  [%s] Lock-free Mailbox   - AETHER_OPT_LOCKFREE_MAILBOX (3.8x slower single-thread)\n",
           atomic_load(&g_aether_config.use_lockfree_mailbox) ? "ON" : "off");
    printf("  [%s] Message Dedup       - AETHER_OPT_MESSAGE_DEDUP (filters duplicates + overhead)\n",
           atomic_load(&g_aether_config.use_message_dedup) ? "ON" : "off");
    printf("\n");
    
    printf("Cache line: %d bytes\n", g_hw_caps.cache_line_size);
    printf("================================================\n\n");
}
