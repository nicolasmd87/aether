# Aether Runtime Guide

Understanding the actor runtime, concurrency model, and message passing system.

## Actor Model

Aether implements a lightweight actor model where actors are state machines that communicate via asynchronous messages.

### Actor Lifecycle

1. **Spawn** - Create actor instance with `spawn_ActorName()`
2. **Send** - Send messages with `send_ActorName()`
3. **Step** - Process messages with `ActorName_step()`
4. **Terminate** - Actor becomes inactive when mailbox is empty

### Actor Structure

Each actor is a C struct:

```c
typedef struct Counter {
    int id;              // Unique actor ID
    int active;          // Active flag
    int assigned_core;   // Core assignment (multi-core)
    Mailbox mailbox;     // Message queue
    void (*step)(void*); // Step function pointer
    int count;           // User state variables
} Counter;
```

## Message Passing

### Mailbox

Each actor has a ring buffer mailbox (16 messages by default):

```c
typedef struct {
    Message messages[MAILBOX_SIZE];
    int head;
    int tail;
    int count;
} Mailbox;
```

### Message Structure

```c
typedef struct {
    int type;           // Message type
    int sender_id;      // Sender actor ID
    int payload_int;    // Integer payload
    void* payload_ptr;  // Pointer payload
} Message;
```

### Sending Messages

Messages are sent asynchronously. The mailbox enqueues the message immediately.

```aether
send_Counter(c, 1, 42);
```

This generates:

```c
void send_Counter(Counter* actor, int type, int payload) {
    Message msg = {type, 0, payload, NULL};
    if (actor->assigned_core == current_core_id) {
        scheduler_send_local((ActorBase*)actor, msg);
    } else {
        scheduler_send_remote((ActorBase*)actor, msg, current_core_id);
    }
}
```

### Receiving Messages

The `receive` block in your actor definition becomes the step function:

```aether
receive(msg) {
    if (msg.type == 1) {
        count = count + 1;
    }
}
```

This generates:

```c
void Counter_step(Counter* self) {
    Message msg;
    if (!mailbox_receive(&self->mailbox, &msg)) {
        self->active = 0;
        return;
    }
    // Generated code from receive block
    if (msg.type == 1) {
        self->count = self->count + 1;
    }
}
```

## Single-Core Runtime

In single-core mode, actors run cooperatively. You manually call step functions:

```aether
main() {
    Counter c = spawn_Counter();
    send_Counter(c, 1, 0);
    Counter_step(c);  // Process one message
}
```

### Performance

Single-core performance:
- **166.7 M msg/sec** throughput
- **264 bytes** per actor
- **Zero-copy** message passing
- **No locking** overhead

## Multi-Core Runtime

The multi-core scheduler uses fixed core partitioning:

- N cores = N independent schedulers
- Each scheduler runs in a pthread
- Actors assigned to cores via hash (actor_id % num_cores)
- Cross-core messages use lock-free queues

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

### Core Assignment

Actors are assigned to cores at spawn time:

```c
int core = actor->id % num_cores;
```

This ensures deterministic placement and good load balancing.

### Message Routing

- **Local messages** (same core): Direct mailbox enqueue
- **Remote messages** (different core): Lock-free queue to target core

### Performance

Multi-core performance scales linearly for local messages:
- 4 cores ≈ 4x single-core throughput
- Cross-core messages have ~100ns overhead

## Memory Management

Actors use manual memory management (malloc/free). The compiler generates allocation in spawn functions, but cleanup is manual:

```c
Counter* c = spawn_Counter();
// ... use actor ...
free(c);  // Manual cleanup
```

## Best Practices

1. **Batch Processing** - Process multiple messages per step when possible
2. **Local Communication** - Prefer actors on same core for hot paths
3. **Message Types** - Use enum constants for message types
4. **State Access** - Access state via `self->` in generated code

## Limitations

- No automatic garbage collection
- No actor supervision trees (yet)
- No pattern matching in receive blocks (yet)
- Fixed mailbox size (16 messages)
