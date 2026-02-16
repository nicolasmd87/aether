# Aether Runtime Guide

Understanding the actor runtime, concurrency model, and message passing system.

## Performance Optimizations

The actor runtime includes several performance optimizations applied automatically. See [runtime-optimizations.md](runtime-optimizations.md) for detailed descriptions of each technique.

### Scheduler Design

**Partitioned Multicore Scheduler:**
- Static actor-to-core assignment at spawn time
- Each core processes only its local actors in the fast path
- Work stealing activated when cores become idle
- Non-blocking work stealing using try-lock with ascending core-id lock ordering
- NUMA-aware allocation when available (Linux, macOS, Windows)
- Message-driven actor migration co-locates actors with their frequent communicators

**Work Stealing:**
- Triggered after extended idle scheduler cycles
- Selects the core with the most pending work
- Steals entire actors (not individual messages) to preserve cache locality
- Uses ascending core-id lock ordering consistent with migration to prevent deadlock

**Message Delivery:**
- Same-core messages: direct mailbox or SPSC queue write (no queue overhead)
- Cross-core messages: lock-free queue with batch dequeue
- Adaptive batch processing: 64 to 1024 messages per scheduler cycle

### Active Optimizations

**Main Thread Actor Mode:**
- Single-actor programs bypass the scheduler entirely
- Messages processed synchronously in the sender's context
- Zero-copy: caller's stack pointer passed directly
- Activated automatically when only one actor exists
- Disabled by `AETHER_NO_INLINE=1` environment variable

**Message Coalescing:**
- Batch dequeue drains multiple messages in a single atomic operation
- Configurable threshold (COALESCE_THRESHOLD = 512)

**Thread-Local Message Pools:**
- Per-thread pool of 256 pre-allocated buffers (up to 256 bytes each)
- Eliminates malloc/free on the message hot path
- Falls back to malloc for oversized messages or pool exhaustion

**Direct Send:**
- Bypasses incoming queue for same-core actors
- Writes directly to actor mailbox or SPSC queue
- Guarded by `current_core_id` to prevent data races from non-scheduler threads

**Adaptive Batching:**
- Dynamically adjusts batch size based on queue depth
- Increases under sustained load, decreases when idle

**Inline Single-Int Messages:**
- Messages with one integer field bypass pool allocation entirely
- Value encoded directly in `Message.payload_int`

## Actor Model

Aether implements a lightweight actor model where actors are state machines that communicate via asynchronous messages.

### Actor Lifecycle

1. **Spawn** - Create actor instance with `spawn(ActorName())`
2. **Send** - Send messages with the `!` operator or generated send functions
3. **Step** - Scheduler calls the actor's step function to process mailbox messages
4. **Deactivate** - Actor is marked inactive when its mailbox is empty

### Actor Structure

Each actor is a C struct. The first fields must match `ActorBase` layout:

```c
typedef struct {
    int active;
    int id;
    Mailbox mailbox;
    void (*step)(void*);
    pthread_t thread;
    int auto_process;
    int assigned_core;
    int migrate_to;
    int main_thread_only;  // If set, scheduler threads skip this actor
    SPSCQueue spsc_queue;
    // User state fields follow
} ActorBase;
```

User-defined actors extend this layout with additional fields after `spsc_queue`.

## Message Passing

### Mailbox

Each actor has a ring buffer mailbox with 256 slots, sized for cache locality:

```c
#define MAILBOX_SIZE 256
#define MAILBOX_MASK (MAILBOX_SIZE - 1)

typedef struct {
    Message messages[MAILBOX_SIZE];
    int head;
    int tail;
    int count;
} Mailbox;
```

The mailbox is not thread-safe. Only the owning scheduler thread (or the actor's own thread for `auto_process` actors) reads from and writes to it. Cross-core messages arrive via the lock-free incoming queue and are delivered by the scheduler.

### Message Structure

```c
typedef struct {
    int type;           // Message type ID
    int sender_id;      // Sender actor ID
    int payload_int;    // Integer payload (used by inline single-int messages)
    void* payload_ptr;  // Pointer to message data (pool or malloc allocated)
    struct {
        void* data;
        int size;
        int owned;
    } zerocopy;
} Message;
```

### Sending Messages

Messages are sent asynchronously. The generated code routes messages through the scheduler:

```c
// Same-core: direct delivery
if (current_core_id >= 0 && current_core_id == actor->assigned_core) {
    scheduler_send_local(actor, msg);
} else {
    scheduler_send_remote(actor, msg, current_core_id);
}
```

For single-int messages, the code generator emits an inline path that stores the value directly in `msg.payload_int`, bypassing pool allocation.

### Receiving Messages

The `receive` block in the actor definition becomes the step function. The code generator emits a computed goto dispatch table for message handler selection:

```c
void ActorName_step(ActorName* self) {
    Message msg;
    if (!mailbox_receive(&self->base.mailbox, &msg)) {
        self->base.active = 0;
        return;
    }
    void* _msg_data = msg.payload_ptr;
    int _msg_id = msg.type;

    static void* dispatch_table[256] = {
        [1] = &&handle_MessageA,
        [2] = &&handle_MessageB,
    };

    if (_msg_id >= 0 && _msg_id < 256 && dispatch_table[_msg_id]) {
        goto *dispatch_table[_msg_id];
    }
    return;

    handle_MessageA:
        // process message
        return;
}
```

## Single-Core Runtime

In single-core mode, actors run cooperatively. The scheduler processes actors in a loop, calling each actor's step function when its mailbox contains messages.

## Multi-Core Runtime

The multi-core scheduler uses fixed core partitioning:

- N cores = N independent scheduler threads
- Each scheduler runs in a pthread pinned to a core
- Actors assigned to cores via round-robin (`actor_id % num_cores`)
- Cross-core messages use lock-free queues with batch dequeue/enqueue

### Synchronization

The `wait_for_idle()` function blocks until all actors have finished processing their messages. This is the recommended way to synchronize the main thread with actor completion:

```aether
main() {
    ping = spawn(PingActor())
    pong = spawn(PongActor())

    // Send initial message
    ping ! Start {}

    // Wait for all messages to be processed
    wait_for_idle()

    // Actors are now idle, safe to read state
    print(ping.result)
}
```

The implementation uses per-core message counters to detect idle state with minimal overhead on the message-passing hot path. Each scheduler core tracks messages sent and processed locally, and `wait_for_idle()` sums across cores to determine when all in-flight messages have been handled.

### Initialization

```c
int main() {
    scheduler_init(4);  // Use 4 cores
    scheduler_start();

    // Create and use actors

    scheduler_stop();
    scheduler_wait();
}
```

### Core Assignment and Migration

Actors are assigned to cores at spawn time. During operation, cross-core message senders set a `migrate_to` hint on the target actor. The owning scheduler thread processes the hint and migrates the actor to co-locate it with its primary communicator. Migration is non-blocking: if the destination lock cannot be acquired, migration is deferred to the next iteration.

### Message Routing

- **Same-core messages:** Direct mailbox or SPSC queue write (verified via `current_core_id`)
- **Cross-core messages:** Enqueued to target core's lock-free incoming queue
- **Migrated actor messages:** Forwarded to the actor's current core with spin-retry

## Memory Management

Actor memory is allocated using NUMA-aware allocation (`aether_numa_alloc`) at spawn time. The `scheduler_spawn_pooled` function accepts the full derived-struct size to ensure correct allocation for user-defined actor types.

Message payloads are managed by thread-local pools. Payloads are returned to the pool on free via `aether_free_message`, which checks whether the pointer falls within the pool range before falling back to `free`.

## Memory Model

Aether uses arena-based memory management for automatic cleanup:
- **Actor memory**: NUMA-aware allocation at spawn time
- **Message payloads**: Thread-local pools with automatic return
- **Arenas**: Bulk deallocation without per-object tracking

See [Memory Management](memory-management.md) for details on arenas, pools, and allocation strategies.
