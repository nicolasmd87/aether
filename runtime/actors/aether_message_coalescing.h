// Message Coalescing System
// Combines multiple small messages into batches to reduce queue overhead.
// Amortises the per-enqueue atomic operations across a batch — target
// workloads are small-message, high-rate producer-consumer paths.

#ifndef AETHER_MESSAGE_COALESCING_H
#define AETHER_MESSAGE_COALESCING_H

#include <stdint.h>
#include <stdatomic.h>
#include <string.h>

#define COALESCE_BUFFER_SIZE 16
#define COALESCE_THRESHOLD 8  // Start coalescing when this many messages pending

// Coalesced message batch
typedef struct {
    void* messages[COALESCE_BUFFER_SIZE];
    uint16_t sizes[COALESCE_BUFFER_SIZE];
    uint16_t count;
    uint16_t capacity;
} CoalescedBatch;

// Per-actor coalescing buffer (thread-local)
typedef struct {
    CoalescedBatch pending;
    atomic_uint_fast64_t last_flush_time;
    uint32_t high_watermark;
    uint32_t flush_count;
} CoalescingBuffer;

// Initialize coalescing buffer
static inline void coalescing_buffer_init(CoalescingBuffer* buf) {
    buf->pending.count = 0;
    buf->pending.capacity = COALESCE_BUFFER_SIZE;
    atomic_store(&buf->last_flush_time, 0);
    buf->high_watermark = 0;
    buf->flush_count = 0;
}

// Add message to coalescing buffer
// Returns 1 if buffer should be flushed, 0 otherwise
static inline int coalescing_buffer_add(
    CoalescingBuffer* buf,
    void* message,
    uint16_t size
) {
    if (buf->pending.count >= buf->pending.capacity) {
        return 1;  // Buffer full, must flush
    }
    
    buf->pending.messages[buf->pending.count] = message;
    buf->pending.sizes[buf->pending.count] = size;
    buf->pending.count++;
    
    if (buf->pending.count > buf->high_watermark) {
        buf->high_watermark = buf->pending.count;
    }
    
    // Flush when threshold reached
    return buf->pending.count >= COALESCE_THRESHOLD;
}

// Flush coalesced batch (send all messages)
static inline void coalescing_buffer_flush(
    CoalescingBuffer* buf,
    void (*send_fn)(void*, uint16_t)
) {
    for (uint16_t i = 0; i < buf->pending.count; i++) {
        send_fn(buf->pending.messages[i], buf->pending.sizes[i]);
    }
    
    buf->flush_count++;
    buf->pending.count = 0;
}

// Check if buffer should be flushed due to timeout
// flush_interval_ns: nanoseconds since last flush before forcing flush
static inline int coalescing_buffer_should_flush_timeout(
    CoalescingBuffer* buf,
    uint64_t current_time_ns,
    uint64_t flush_interval_ns
) {
    if (buf->pending.count == 0) {
        return 0;
    }
    
    uint64_t last_flush = atomic_load(&buf->last_flush_time);
    return (current_time_ns - last_flush) >= flush_interval_ns;
}

// Get coalescing statistics
typedef struct {
    uint32_t messages_coalesced;
    uint32_t flush_count;
    uint32_t high_watermark;
    float avg_batch_size;
} CoalescingStats;

static inline CoalescingStats coalescing_buffer_stats(const CoalescingBuffer* buf) {
    CoalescingStats stats;
    stats.flush_count = buf->flush_count;
    stats.high_watermark = buf->high_watermark;
    stats.messages_coalesced = buf->flush_count * buf->high_watermark;
    stats.avg_batch_size = buf->flush_count > 0 
        ? (float)stats.messages_coalesced / buf->flush_count 
        : 0.0f;
    return stats;
}

// Adaptive coalescing decision
// Returns 1 if should coalesce, 0 if should send immediately
static inline int coalescing_should_enable(
    uint32_t messages_pending,
    uint32_t messages_per_second
) {
    // Enable coalescing under high load (>10k msg/sec or >5 msgs pending)
    return messages_pending >= 5 || messages_per_second > 10000;
}

#endif // AETHER_MESSAGE_COALESCING_H
