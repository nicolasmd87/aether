# Aether Architecture

Internal design of the Aether compiler and runtime.

## Compiler Pipeline

Aether uses a traditional multi-stage compiler:

```
Source (.ae) → Lexer → Parser → AST → Type Checker → Code Generator → C Code
```

### Lexer (`compiler/lexer.c`)

Tokenizes source code into tokens:
- Keywords: `actor`, `state`, `receive`, `send`, `spawn`
- Identifiers: variable and function names
- Literals: integers, strings
- Operators: `+`, `-`, `*`, `/`, `==`, etc.

### Parser (`compiler/parser.c`)

Builds Abstract Syntax Tree (AST) from tokens:
- Expression parsing with operator precedence
- Statement parsing (if, while, for, return)
- Declaration parsing (variables, functions, actors, structs)

### Type Checker (`compiler/typechecker.c`)

Validates program correctness:
- Type checking for expressions
- Variable declaration checking
- Function call validation
- Actor and struct field validation

### Code Generator (`compiler/codegen.c`)

Generates C code from AST:
- Function code generation
- Actor struct and step function generation
- Spawn and send function generation
- Expression and statement translation

## Runtime Components

### Actor State Machine (`runtime/actor_state_machine.h`)

Core actor primitives:
- `Message` struct
- `Mailbox` ring buffer
- `mailbox_send()` and `mailbox_receive()` functions

### Multi-Core Scheduler (`runtime/multicore_scheduler.{c,h}`)

Scheduler implementation:
- Per-core pthread threads
- Actor registration and assignment
- Local and remote message routing
- Lock-free cross-core queues

### Lock-Free Queue (`runtime/lockfree_queue.h`)

Single Producer, Single Consumer (SPSC) queue:
- Atomic head/tail pointers
- Cache line padding to prevent false sharing
- Used for cross-core message passing

## Code Generation Details

### Actor Struct Generation

```aether
actor Counter {
    state int count = 0;
}
```

Generates:

```c
typedef struct Counter {
    int id;
    int active;
    int assigned_core;
    Mailbox mailbox;
    void (*step)(void*);
    int count;  // User state
} Counter;
```

### Step Function Generation

```aether
receive(msg) {
    count = count + 1;
}
```

Generates:

```c
void Counter_step(Counter* self) {
    Message msg;
    if (!mailbox_receive(&self->mailbox, &msg)) {
        self->active = 0;
        return;
    }
    self->count = self->count + 1;
}
```

### Spawn Function Generation

```aether
Counter c = spawn_Counter();
```

Generates:

```c
Counter* spawn_Counter() {
    Counter* actor = malloc(sizeof(Counter));
    actor->id = atomic_fetch_add(&next_actor_id, 1);
    actor->active = 1;
    actor->assigned_core = -1;
    actor->step = (void (*)(void*))Counter_step;
    mailbox_init(&actor->mailbox);
    actor->count = 0;  // Initialize state
    scheduler_register_actor((ActorBase*)actor, -1);
    return actor;
}
```

## Design Decisions

### Why C as Target?

- Maximum performance (no VM overhead)
- Easy integration with existing C libraries
- Predictable memory layout
- Direct control over generated code

### Why Fixed Core Partitioning?

- Simpler than work-stealing (no locks needed)
- Predictable performance
- Good cache locality
- Linear scaling for local messages

### Why Ring Buffer Mailboxes?

- Constant-time enqueue/dequeue
- Fixed memory footprint
- Cache-friendly (small, contiguous)
- No dynamic allocation

### Why State Machine Model?

- No context switching overhead
- Deterministic execution
- Easy to reason about
- Compiles to simple function calls

## Performance Characteristics

### Single-Core

- 166.7 M msg/sec throughput
- 264 bytes per actor
- ~6ns per message (on modern CPUs)

### Multi-Core

- Linear scaling for local messages
- ~100ns overhead for cross-core messages
- Lock-free queues prevent contention

## Future Improvements

Potential enhancements:
- Pattern matching in receive blocks
- Work-stealing scheduler (for imbalanced loads)
- NUMA-aware actor placement
- Actor supervision trees
- Automatic memory management
