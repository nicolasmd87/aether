# Embedding Aether Actors in C Applications

This guide explains how to embed Aether actors in your existing C applications. There are two approaches: compiling Aether code to C and linking it, or using the runtime API directly.

## Overview

Aether compiles to C, which means integration with C codebases is straightforward:

1. **Compile-and-Link**: Write actors in Aether, compile to C with `aetherc`, include the generated code
2. **Direct Runtime API**: Use Aether's runtime API to create actors entirely in C

## Approach 1: Compile Aether to C

### Step 1: Write Your Aether Actor

**counter.ae:**
```aether
message Increment {
    amount: int
}

message GetValue {}

message Reset {}

actor Counter {
    state count = 0

    receive {
        Increment(amount) -> {
            count = count + amount
        }
        GetValue() -> {
            print(count)
        }
        Reset() -> {
            count = 0
        }
    }
}

main() {
    counter = spawn(Counter())
    counter ! Increment { amount: 10 }
    counter ! GetValue {}
}
```

### Step 2: Compile to C

```bash
aetherc counter.ae counter.c
```

This generates `counter.c` containing:
- Message struct definitions
- Actor struct with state
- Message handler dispatch table
- `spawn_Counter()` function

### Step 3: Include in Your C Project

**main.c:**
```c
#include <stdio.h>
#include "runtime/aether_runtime.h"
#include "runtime/scheduler/multicore_scheduler.h"

// Forward declarations from generated code
typedef struct Counter Counter;
Counter* spawn_Counter(void);

// Message IDs (from generated code)
#define MSG_Increment 1
#define MSG_GetValue 2
#define MSG_Reset 3

int main(int argc, char** argv) {
    // Initialize Aether runtime
    aether_runtime_init(0, AETHER_FLAG_AUTO_DETECT);
    scheduler_init(4);  // 4 cores
    scheduler_start();

    // Spawn the actor
    Counter* counter = spawn_Counter();

    // Send messages using the Message struct
    Message msg = {0};
    msg.type = MSG_Increment;
    msg.payload_int = 10;  // amount
    scheduler_send_remote((ActorBase*)counter, msg, -1);

    // Allow time for processing
    // In real code, use proper synchronization
    usleep(100000);

    // Cleanup
    scheduler_stop();
    scheduler_wait();
    scheduler_cleanup();
    aether_runtime_shutdown();

    return 0;
}
```

### Step 4: Build

```bash
# Compile Aether code
aetherc counter.ae counter.c

# Build with runtime
gcc -O2 main.c counter.c \
    -I$HOME/.aether/include/aether \
    -L$HOME/.aether/lib -laether \
    -lpthread -o myapp
```

## Approach 2: Direct Runtime API

For full control, create actors directly in C using the runtime API.

### Minimal Example

```c
#include "runtime/aether_runtime.h"
#include "runtime/scheduler/multicore_scheduler.h"

// Define your actor structure
typedef struct {
    ActorBase base;    // Must be first field
    int counter;       // Your state
} MyCounter;

// Message handler
void my_counter_step(void* self) {
    MyCounter* actor = (MyCounter*)self;
    Message msg;

    // Process messages from mailbox
    while (mailbox_receive(&actor->base.mailbox, &msg)) {
        switch (msg.type) {
            case 1:  // Increment
                actor->counter += msg.payload_int;
                break;
            case 2:  // Print
                printf("Counter: %d\n", actor->counter);
                break;
        }
    }
}

int main() {
    // Initialize runtime
    aether_runtime_init(0, AETHER_FLAG_AUTO_DETECT);
    scheduler_init(4);
    scheduler_start();

    // Spawn actor using pool
    MyCounter* actor = (MyCounter*)scheduler_spawn_pooled(
        0,                    // Preferred core
        my_counter_step,      // Step function
        sizeof(MyCounter)     // Size
    );
    actor->counter = 0;       // Initialize state

    // Send messages
    Message msg = {0};
    msg.type = 1;             // Increment
    msg.payload_int = 42;
    scheduler_send_remote((ActorBase*)actor, msg, -1);

    msg.type = 2;             // Print
    scheduler_send_remote((ActorBase*)actor, msg, -1);

    usleep(100000);

    // Cleanup
    scheduler_stop();
    scheduler_wait();
    scheduler_cleanup();
    aether_runtime_shutdown();

    return 0;
}
```

## Runtime API Reference

### Lifecycle Functions

```c
// Initialize runtime with automatic CPU feature detection
// num_cores: 0 = auto-detect, or specify explicitly
// flags: AETHER_FLAG_AUTO_DETECT recommended
void aether_runtime_init(int num_cores, int flags);

// Shutdown runtime and free resources
void aether_runtime_shutdown(void);

// Initialize scheduler with core count
// cores: 0 = auto-detect
void scheduler_init(int cores);

// Start scheduler threads
void scheduler_start(void);

// Signal scheduler threads to stop
void scheduler_stop(void);

// Wait for all scheduler threads to finish
void scheduler_wait(void);

// Free scheduler resources
void scheduler_cleanup(void);
```

### Actor Management

```c
// Spawn actor from pool (recommended)
// Returns actor pointer, NULL on failure
ActorBase* scheduler_spawn_pooled(
    int preferred_core,      // Core hint, -1 for round-robin
    void (*step)(void*),     // Message handler function
    size_t actor_size        // sizeof(YourActorType)
);

// Return actor to pool when done
void scheduler_release_pooled(ActorBase* actor);

// Register existing actor (alternative to pooled)
int scheduler_register_actor(ActorBase* actor, int preferred_core);
```

### Message Passing

```c
// Message structure
typedef struct {
    int type;           // Message ID
    int sender_core;    // Automatically set
    int payload_int;    // For single-int messages
    void* payload_ptr;  // For complex payloads
} Message;

// Send to actor on same core (fast path)
void scheduler_send_local(ActorBase* actor, Message msg);

// Send to actor on any core
// from_core: sender's core ID, -1 if unknown
void scheduler_send_remote(ActorBase* actor, Message msg, int from_core);
```

### Configuration Flags

```c
#define AETHER_FLAG_AUTO_DETECT      (1 << 0)  // Detect CPU features
#define AETHER_FLAG_LOCKFREE_MAILBOX (1 << 1)  // Lock-free mailboxes
#define AETHER_FLAG_LOCKFREE_POOLS   (1 << 2)  // Lock-free pools
#define AETHER_FLAG_ENABLE_SIMD      (1 << 3)  // SIMD optimizations
#define AETHER_FLAG_ENABLE_MWAIT     (1 << 4)  // MWAIT for idle
#define AETHER_FLAG_VERBOSE          (1 << 5)  // Print config on init
```

## Actor Structure Requirements

When defining actors in C, follow these rules:

```c
typedef struct {
    ActorBase base;     // MUST be first field
    // Your state fields here
    int my_value;
    char* my_string;
} MyActor;
```

The `ActorBase` contains:
- `mailbox`: Message queue
- `active`: Processing flag
- `id`: Unique actor ID
- `assigned_core`: Current core assignment
- `step`: Your message handler

## Message Handler Pattern

```c
void my_actor_step(void* self) {
    MyActor* actor = (MyActor*)self;
    Message msg;

    // Drain all pending messages
    while (mailbox_receive(&actor->base.mailbox, &msg)) {
        switch (msg.type) {
            case MSG_TYPE_1:
                // Handle message type 1
                handle_type_1(actor, msg.payload_int);
                break;

            case MSG_TYPE_2:
                // Handle message type 2
                handle_type_2(actor, (SomeStruct*)msg.payload_ptr);
                break;

            default:
                // Unknown message type
                break;
        }
    }
}
```

## Build Configuration

### Makefile Example

```makefile
AETHER_HOME ?= $(HOME)/.aether
CC = gcc
CFLAGS = -O2 -I$(AETHER_HOME)/include/aether
LDFLAGS = -L$(AETHER_HOME)/lib -laether -lpthread

# For Aether source files
%.c: %.ae
	aetherc $< $@

myapp: main.c counter.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
```

### CMake Example

```cmake
cmake_minimum_required(VERSION 3.10)
project(MyAetherApp)

set(AETHER_HOME "$ENV{HOME}/.aether")

include_directories(${AETHER_HOME}/include/aether)
link_directories(${AETHER_HOME}/lib)

add_executable(myapp main.c counter.c)
target_link_libraries(myapp aether pthread)
```

## Thread Safety

- Each actor runs on one core at a time
- Messages are delivered via thread-safe queues
- Actor state is only accessed by one thread
- Cross-core sends use lock-free queues

## Limitations

Current limitations when embedding:

1. **No direct state access**: Actor state should only be accessed through messages
2. **No synchronous replies**: Use callback patterns or shared atomic state
3. **Manual lifetime management**: Track actor references carefully
4. **Single runtime instance**: Only one `aether_runtime_init` per process

## Future: Header Generation

A planned `--emit-header` flag for `aetherc` will generate C headers with:
- Type-safe send helper functions
- Message struct definitions
- Actor spawn function prototypes

Example of what will be generated:

```c
// counter.h (auto-generated)
#ifndef COUNTER_H
#define COUNTER_H

#include "aether_runtime.h"

// Message: Increment
typedef struct { int amount; } Increment;

// Typed send helper
static inline void Counter_Increment(Counter* actor, int amount) {
    Message msg = {MSG_Increment, 0, amount, NULL};
    scheduler_send_remote((ActorBase*)actor, msg, -1);
}

Counter* spawn_Counter(void);

#endif
```

## See Also

- [C Interoperability](c-interop.md) - Calling C from Aether
- [Runtime Configuration](runtime-config.md) - Runtime flags and options
- [Actor Concurrency](actor-concurrency.md) - Actor model concepts
