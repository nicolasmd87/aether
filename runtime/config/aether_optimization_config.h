// Aether Runtime Optimization Configuration
// Defines which optimizations are always-on, auto-detected, or opt-in
//
// OPTIMIZATION TIERS:
// ====================
// Tier 1 - ALWAYS ON (built-in, proven wins):
//   - Actor Pooling (1.81x speedup)
//   - Direct Send (same-core bypass)
//   - Adaptive Batching (4-64 dynamic)
//   - Message Coalescing (15x throughput)
//   - TLS Message Pools (eliminates mutex contention)
//
// Tier 2 - AUTO-DETECT (hardware-dependent):
//   - SIMD Batch Processing (requires AVX2/NEON)
//   - MWAIT Idle (requires x86 MONITOR/MWAIT)
//   - CPU Core Pinning (OS-dependent)
//
// Tier 3 - OPT-IN (user must enable via flag, have trade-offs):
//   - Lock-free Mailbox (3.8x SLOWER single-thread, 1.8x faster under heavy contention)
//   - Message Deduplication (adds overhead, changes semantics - filters duplicates)

#ifndef AETHER_OPTIMIZATION_CONFIG_H
#define AETHER_OPTIMIZATION_CONFIG_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include "../utils/aether_compiler.h"

// ============================================================================
// MEMORY PROFILES (auto-detected or user-specified via AETHER_PROFILE)
// Controls pool sizes for different workload characteristics
// ============================================================================

typedef enum {
    AETHER_PROFILE_MICRO,    // <10 actors, <1M messages: pools=256
    AETHER_PROFILE_SMALL,    // <100 actors: pools=1024
    AETHER_PROFILE_MEDIUM,   // <1000 actors: pools=4096 (default)
    AETHER_PROFILE_LARGE     // 1000+ actors: pools=16384
} AetherProfile;

// Profile pool sizes
#define AETHER_PROFILE_MICRO_MSG_POOL   256
#define AETHER_PROFILE_SMALL_MSG_POOL   1024
#define AETHER_PROFILE_MEDIUM_MSG_POOL  4096
#define AETHER_PROFILE_LARGE_MSG_POOL   16384

#define AETHER_PROFILE_MICRO_ACTOR_POOL  16
#define AETHER_PROFILE_SMALL_ACTOR_POOL  64
#define AETHER_PROFILE_MEDIUM_ACTOR_POOL 256
#define AETHER_PROFILE_LARGE_ACTOR_POOL  1024

// Get pool sizes for a profile
static inline int aether_profile_msg_pool_size(AetherProfile p) {
    switch (p) {
        case AETHER_PROFILE_MICRO:  return AETHER_PROFILE_MICRO_MSG_POOL;
        case AETHER_PROFILE_SMALL:  return AETHER_PROFILE_SMALL_MSG_POOL;
        case AETHER_PROFILE_LARGE:  return AETHER_PROFILE_LARGE_MSG_POOL;
        default:                    return AETHER_PROFILE_MEDIUM_MSG_POOL;
    }
}

static inline int aether_profile_actor_pool_size(AetherProfile p) {
    switch (p) {
        case AETHER_PROFILE_MICRO:  return AETHER_PROFILE_MICRO_ACTOR_POOL;
        case AETHER_PROFILE_SMALL:  return AETHER_PROFILE_SMALL_ACTOR_POOL;
        case AETHER_PROFILE_LARGE:  return AETHER_PROFILE_LARGE_ACTOR_POOL;
        default:                    return AETHER_PROFILE_MEDIUM_ACTOR_POOL;
    }
}

// ============================================================================
// ENVIRONMENT VARIABLE HELPERS
// ============================================================================

// Parse AETHER_PROFILE env var
static inline AetherProfile aether_profile_from_env(void) {
    const char* env = getenv("AETHER_PROFILE");
    if (!env) return AETHER_PROFILE_MEDIUM;

    if (env[0] == 'm' && env[1] == 'i') return AETHER_PROFILE_MICRO;  // "micro"
    if (env[0] == 's') return AETHER_PROFILE_SMALL;                   // "small"
    if (env[0] == 'l') return AETHER_PROFILE_LARGE;                   // "large"
    return AETHER_PROFILE_MEDIUM;                                      // "medium" or unknown
}

// Read integer from env var with default
static inline int aether_env_int(const char* name, int default_val) {
    const char* env = getenv(name);
    if (!env) return default_val;
    return atoi(env);
}

// Check if env var is set (any value = true, absent = false)
static inline bool aether_env_bool(const char* name) {
    return getenv(name) != NULL;
}

// ============================================================================
// TIER 1: ALWAYS-ON OPTIMIZATIONS (no user control needed)
// These provide consistent wins with no downsides
// ============================================================================

#define AETHER_ACTOR_POOLING_ENABLED    1   // Always use pooled allocation
#define AETHER_DIRECT_SEND_ENABLED      1   // Always bypass queue for same-core
#define AETHER_ADAPTIVE_BATCH_ENABLED   1   // Always adjust batch size dynamically
#define AETHER_COALESCING_ENABLED       1   // Always coalesce messages

// ============================================================================
// TIER 2: AUTO-DETECTED OPTIMIZATIONS (hardware/OS dependent)
// Automatically enabled if hardware supports them
// ============================================================================

// SIMD Processing - auto-detected at init
typedef struct {
    bool simd_available;        // CPU supports AVX2 or NEON
    bool mwait_available;       // CPU supports MONITOR/MWAIT
    bool cpu_pinning_available; // OS supports thread affinity
    int  simd_width;            // 128 (SSE), 256 (AVX2), or 512 (AVX-512)
    int  cache_line_size;       // For alignment (usually 64)
} AetherHardwareCapabilities;

// Global hardware caps (initialized once at runtime start)
extern AetherHardwareCapabilities g_hw_caps;

// Initialize hardware detection (called by scheduler_init)
void aether_detect_hardware(void);

// Quick checks for auto-detected features
static inline bool aether_has_simd(void) { return g_hw_caps.simd_available; }
static inline bool aether_has_mwait(void) { return g_hw_caps.mwait_available; }
static inline bool aether_has_cpu_pinning(void) { return g_hw_caps.cpu_pinning_available; }
static inline int  aether_simd_width(void) { return g_hw_caps.simd_width; }

// ============================================================================
// TIER 3: OPT-IN OPTIMIZATIONS (user-controlled via flags)
// May have trade-offs, user decides based on workload
// ============================================================================

// User-controllable optimization flags
typedef enum {
    AETHER_OPT_NONE             = 0,
    
    // Tier 3 opt-in flags (have trade-offs)
    AETHER_OPT_LOCKFREE_MAILBOX = (1 << 0),  // 3.8x SLOWER single-thread, 1.8x faster multi-thread contention
    AETHER_OPT_MESSAGE_DEDUP    = (1 << 1),  // Filters duplicate messages (semantic change + overhead)
    AETHER_OPT_VERBOSE          = (1 << 3),  // Print optimization info at startup
    
    // Convenience combos
    AETHER_OPT_HIGH_CONTENTION  = AETHER_OPT_LOCKFREE_MAILBOX,  // For heavy multi-threaded workloads
    AETHER_OPT_ALL              = 0xFFFFFFFF
} AetherOptFlags;

// Current runtime configuration
typedef struct {
    // Tier 3 opt-in flags (user-controlled)
    atomic_bool use_lockfree_mailbox;
    atomic_bool use_message_dedup;
    atomic_bool verbose;

    // Inline mode (auto-detected or env override)
    // When active, single-actor programs bypass queues entirely
    atomic_bool inline_mode_active;
    atomic_bool inline_mode_forced;   // Set by AETHER_INLINE=1 env var
    atomic_bool inline_mode_disabled; // Set by AETHER_NO_INLINE=1 env var

    // Main thread actor mode: single actor, all sends from main(), no scheduler needed
    // When active: messages processed synchronously in sender's context
    atomic_bool main_thread_mode;
    void* main_actor;  // Pointer to the single actor (ActorBase*)

    // Actor tracking for auto-detection
    atomic_int actor_count;

    // Memory profile (auto-detected or env override)
    AetherProfile profile;
    int msg_pool_size;    // Actual pool size (env override or from profile)
    int actor_pool_size;  // Actual pool size (env override or from profile)

    // Statistics
    atomic_uint_fast64_t actors_pooled;
    atomic_uint_fast64_t actors_malloced;
    atomic_uint_fast64_t direct_sends;
    atomic_uint_fast64_t queue_sends;
    atomic_uint_fast64_t inline_sends;     // Messages sent via inline mode
    atomic_uint_fast64_t batches_adjusted;
    atomic_uint_fast64_t simd_batches;
    atomic_uint_fast64_t messages_deduped;
} AetherRuntimeConfig;

extern AetherRuntimeConfig g_aether_config;

// Initialize with user flags
void aether_runtime_configure(AetherOptFlags flags);

// Print current configuration
void aether_print_config(void);

// ============================================================================
// IMPLEMENTATION (inline for performance)
// ============================================================================

static inline void aether_enable_opt(AetherOptFlags flag) {
    if (flag & AETHER_OPT_LOCKFREE_MAILBOX) atomic_store(&g_aether_config.use_lockfree_mailbox, true);
    if (flag & AETHER_OPT_MESSAGE_DEDUP) atomic_store(&g_aether_config.use_message_dedup, true);
    if (flag & AETHER_OPT_VERBOSE) atomic_store(&g_aether_config.verbose, true);
}

static inline void aether_disable_opt(AetherOptFlags flag) {
    if (flag & AETHER_OPT_LOCKFREE_MAILBOX) atomic_store(&g_aether_config.use_lockfree_mailbox, false);
    if (flag & AETHER_OPT_MESSAGE_DEDUP) atomic_store(&g_aether_config.use_message_dedup, false);
    if (flag & AETHER_OPT_VERBOSE) atomic_store(&g_aether_config.verbose, false);
}

static inline bool aether_has_opt(AetherOptFlags flag) {
    if (flag & AETHER_OPT_LOCKFREE_MAILBOX && !atomic_load(&g_aether_config.use_lockfree_mailbox)) return false;
    if (flag & AETHER_OPT_MESSAGE_DEDUP && !atomic_load(&g_aether_config.use_message_dedup)) return false;
    return true;
}

// Record statistics (enabled in debug builds)
#ifdef AETHER_STATS
#define AETHER_STAT_INC(field) atomic_fetch_add(&g_aether_config.field, 1)
#else
#define AETHER_STAT_INC(field) ((void)0)
#endif

// ============================================================================
// INLINE MODE DETECTION (for single-actor sequential optimization)
// ============================================================================

// Check if inline mode is currently active (single branch, predicted false)
static inline bool aether_inline_mode_active(void) {
    return unlikely(atomic_load_explicit(&g_aether_config.inline_mode_active, memory_order_relaxed));
}

// Check if main thread actor mode is active (synchronous processing)
// This is the fastest path: no scheduler, no queues, direct function calls.
// NOTE: No __builtin_expect here — in single-actor programs this is always true
// (hinting it as unlikely would put the fast inline path in cold instruction cache).
// The CPU branch predictor learns the correct bias quickly for both cases.
static inline bool aether_main_thread_mode_active(void) {
    return atomic_load_explicit(&g_aether_config.main_thread_mode, memory_order_relaxed);
}

// Enable main thread mode for an actor (call after spawn, before any sends)
static inline void aether_enable_main_thread_mode(void* actor) {
    atomic_store_explicit(&g_aether_config.main_thread_mode, true, memory_order_relaxed);
    g_aether_config.main_actor = actor;
}

// Called at spawn time to update actor count and inline mode state
static inline void aether_on_actor_spawn(void) {
    int count = atomic_fetch_add_explicit(&g_aether_config.actor_count, 1, memory_order_relaxed);

    // Check env overrides
    if (atomic_load_explicit(&g_aether_config.inline_mode_disabled, memory_order_relaxed)) {
        return; // User forced inline off
    }

    if (atomic_load_explicit(&g_aether_config.inline_mode_forced, memory_order_relaxed)) {
        atomic_store_explicit(&g_aether_config.inline_mode_active, true, memory_order_relaxed);
        return; // User forced inline on regardless of actor count
    }

    // Auto-detect: inline mode only for single actor (count was 0 before increment)
    bool single_actor = (count == 0);
    atomic_store_explicit(&g_aether_config.inline_mode_active, single_actor, memory_order_relaxed);

    // MAIN THREAD MODE: Disable when second actor is spawned
    // This ensures multi-actor programs use scheduler threads (not synchronous processing)
    if (!single_actor) {
        atomic_store_explicit(&g_aether_config.main_thread_mode, false, memory_order_relaxed);
        g_aether_config.main_actor = NULL;
    }
}

// Called when an actor terminates
static inline void aether_on_actor_terminate(void) {
    atomic_fetch_sub_explicit(&g_aether_config.actor_count, 1, memory_order_relaxed);
}

// Initialize profile and inline mode from env vars
// Called once at runtime init
void aether_init_from_env(void);

#endif // AETHER_OPTIMIZATION_CONFIG_H
