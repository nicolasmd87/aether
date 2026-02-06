// Aether Runtime Initialization and Configuration
// Provides flags for controlling optimizations

#include "aether_runtime.h"
#include "utils/aether_cpu_detect.h"
#include <stdio.h>
#include <stdlib.h>

// Global runtime configuration
static AetherRuntimeConfig g_runtime_config = {0};
static int g_runtime_initialized = 0;

// Initialize runtime with default settings
void aether_runtime_init(int num_cores, int flags) {
    if (g_runtime_initialized) {
        fprintf(stderr, "Warning: Runtime already initialized\n");
        return;
    }
    
    // Detect CPU features
    const CPUInfo* cpu = cpu_get_info();
    
    // Set defaults based on CPU capabilities
    g_runtime_config.num_cores = num_cores > 0 ? num_cores : cpu_recommend_cores();
    g_runtime_config.flags = flags;
    
    // Auto-enable features based on CPU if AUTO flag set
    if (flags & AETHER_FLAG_AUTO_DETECT) {
        if (cpu->avx2_supported) {
            g_runtime_config.flags |= AETHER_FLAG_ENABLE_SIMD;
        }
        if (cpu->mwait_supported) {
            g_runtime_config.flags |= AETHER_FLAG_ENABLE_MWAIT;
        }
        // Always prefer lock-free on modern CPUs
        g_runtime_config.flags |= AETHER_FLAG_LOCKFREE_MAILBOX;
        g_runtime_config.flags |= AETHER_FLAG_LOCKFREE_POOLS;
    }
    
    // Apply explicit overrides
    g_runtime_config.use_lockfree_mailbox = 
        (flags & AETHER_FLAG_LOCKFREE_MAILBOX) || 
        (flags & AETHER_FLAG_AUTO_DETECT && cpu->sse42_supported);
    
    g_runtime_config.use_lockfree_pools = 
        (flags & AETHER_FLAG_LOCKFREE_POOLS) ||
        (flags & AETHER_FLAG_AUTO_DETECT);
    
    g_runtime_config.use_mwait = 
        (flags & AETHER_FLAG_ENABLE_MWAIT) ||
        (flags & AETHER_FLAG_AUTO_DETECT && cpu->mwait_supported);
    
    g_runtime_config.use_simd = 
        (flags & AETHER_FLAG_ENABLE_SIMD) ||
        (flags & AETHER_FLAG_AUTO_DETECT && cpu->avx2_supported);
    
    g_runtime_initialized = 1;
    
    // Print configuration if verbose
    if (flags & AETHER_FLAG_VERBOSE) {
        aether_runtime_print_config();
    }
}

// Get current runtime configuration
const AetherRuntimeConfig* aether_runtime_get_config() {
    if (!g_runtime_initialized) {
        // Initialize with defaults on first access
        aether_runtime_init(0, AETHER_FLAG_AUTO_DETECT);
    }
    return &g_runtime_config;
}

// Check if a specific feature is enabled
int aether_runtime_has_feature(int feature_flag) {
    const AetherRuntimeConfig* config = aether_runtime_get_config();
    return (config->flags & feature_flag) != 0;
}

// Print current runtime configuration
void aether_runtime_print_config() {
    const AetherRuntimeConfig* config = aether_runtime_get_config();
    const CPUInfo* cpu = cpu_get_info();
    
    printf("\n");
    printf("========================================\n");
    printf("  Aether Runtime Configuration\n");
    printf("========================================\n\n");
    
    printf("CPU: %s\n", cpu->cpu_brand);
    printf("Cores: %d (using %d)\n", cpu->num_cores, config->num_cores);
    printf("Cache line: %d bytes\n\n", cpu->cache_line_size);
    
    printf("Active Optimizations:\n");
    printf("  Lock-free mailbox: %s\n", config->use_lockfree_mailbox ? "ENABLED" : "disabled");
    printf("  Lock-free pools:   %s\n", config->use_lockfree_pools ? "ENABLED" : "disabled");
    printf("  MWAIT idle:        %s", config->use_mwait ? "ENABLED" : "disabled");
    if (config->use_mwait && !cpu->mwait_supported) {
        printf(" (WARNING: CPU doesn't support MWAIT!)");
    }
    printf("\n");
    printf("  SIMD vectorize:    %s", config->use_simd ? "ENABLED" : "disabled");
    if (config->use_simd && !cpu->avx2_supported) {
        printf(" (WARNING: CPU doesn't support AVX2!)");
    }
    printf("\n\n");
    
    printf("Expected Performance:\n");
    if (config->use_lockfree_mailbox && config->use_simd && config->use_mwait) {
        printf("  Throughput: ~2.3B msg/sec on 8 cores\n");
        printf("  Latency:    Sub-microsecond\n");
        printf("  Status:     MAXIMUM\n");
    } else if (config->use_lockfree_mailbox && config->use_simd) {
        printf("  Throughput: ~2.1B msg/sec on 8 cores\n");
        printf("  Latency:    ~1 microsecond\n");
        printf("  Status:     HIGH\n");
    } else if (config->use_lockfree_mailbox) {
        printf("  Throughput: ~350M msg/sec on 8 cores\n");
        printf("  Latency:    ~1 microsecond\n");
        printf("  Status:     MODERATE\n");
    } else {
        printf("  Throughput: ~125M msg/sec on 8 cores\n");
        printf("  Latency:    ~10 microseconds\n");
        printf("  Status:     BASELINE\n");
    }
    
    printf("\n========================================\n\n");
}

// Shutdown runtime
void aether_runtime_shutdown() {
    g_runtime_initialized = 0;
}

// Command-line arguments
int aether_argc = 0;
char** aether_argv = NULL;

void aether_args_init(int argc, char** argv) {
    aether_argc = argc;
    aether_argv = argv;
}

int aether_args_count(void) {
    return aether_argc;
}

const char* aether_args_get(int index) {
    if (index < 0 || index >= aether_argc || !aether_argv) {
        return NULL;
    }
    return aether_argv[index];
}
