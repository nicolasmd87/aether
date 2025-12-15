# Aether Language Specification

## Overview

Aether is a systems programming language designed for high-performance concurrent applications. It features an actor-based concurrency model that eliminates shared state and provides compile-time safety guarantees.

## Key Features

- **Actor-based Concurrency**: Actors communicate through message passing, eliminating data races
- **Type Safety**: Compile-time type checking with actor-specific types
- **Zero-overhead Abstractions**: Direct compilation to C with minimal runtime overhead
- **Memory Safety**: Actor isolation prevents memory corruption
- **Fault Tolerance**: Built-in supervision patterns for resilient systems

## Language Syntax

### Basic Types

```aether
int x = 42;
float pi = 3.14159;
bool flag = true;
string name = "Aether";
```

### Actor Definition

```aether
actor Counter {
    state int count = 0;
    
    receive(msg) {
        match msg {
            Increment => {
                count += 1;
            }
            GetValue(sender) => {
                send(sender, count);
            }
        }
    }
}
```

### Function Definition

```aether
func add(a: int, b: int): int {
    return a + b;
}
```

### Main Function

```aether
main() {
    let counter = spawn_actor(Counter);
    send(counter, Increment);
}
```

### Control Flow

```aether
// If statement
if (x > 0) {
    print("Positive");
} else {
    print("Non-positive");
}

// For loop
for (int i = 0; i < 10; i++) {
    print("Iteration: %d\n", i);
}

// While loop
while (condition) {
    // loop body
}

// Switch statement
switch (value) {
    case 1:
        print("One");
        break;
    case 2:
        print("Two");
        break;
    default:
        print("Other");
}
```

### Message Passing

```aether
// Send message
send(actor_ref, message);

// Receive message (inside actor)
receive(msg) {
    match msg {
        MessageType => {
            // handle message
        }
    }
}
```

## Actor Model

### Actor Lifecycle

1. **Creation**: Actors are created with `spawn_actor()`
2. **Running**: Actors process messages in their receive function
3. **Termination**: Actors can be terminated with `aether_actor_terminate()`

### Message Types

Messages are defined by pattern matching in the receive function:

```aether
receive(msg) {
    match msg {
        Increment => { /* handle increment */ }
        GetValue(sender) => { /* handle get value */ }
        SetValue(value) => { /* handle set value */ }
    }
}
```

### Actor State

Actors maintain private state that can only be modified by the actor itself:

```aether
actor BankAccount {
    state int balance = 0;
    state string owner = "";
    
    receive(msg) {
        // Only this actor can modify balance and owner
    }
}
```

## Type System

### Basic Types

- `int`: 32-bit signed integer
- `float`: 32-bit floating point
- `bool`: Boolean (true/false)
- `string`: Null-terminated string
- `void`: No return value

### Actor Types

- `ActorRef<T>`: Reference to an actor of type T
- `ActorRef`: Untyped actor reference

### Array Types

- `int[10]`: Fixed-size array
- `int[]`: Dynamic array
- `ActorRef[]`: Array of actor references

### Type Annotations

```aether
let x: int = 42;
let actor: ActorRef<Counter> = spawn_actor(Counter);
let numbers: int[] = new int[10];
```

## Memory Management

Aether uses a combination of:

1. **Stack allocation**: For local variables
2. **Actor-owned memory**: Each actor manages its own memory
3. **Message passing**: Data is copied between actors
4. **Reference counting**: For actor references

## Concurrency Patterns

### Basic Actor

```aether
actor Worker {
    state int work_count = 0;
    
    receive(msg) {
        match msg {
            DoWork => {
                work_count += 1;
                print("Work done: %d\n", work_count);
            }
        }
    }
}
```

### Supervisor Pattern

```aether
actor Supervisor {
    state ActorRef[] workers;
    
    receive(msg) {
        match msg {
            WorkerFailed(worker_id) => {
                // Restart worker
                workers[worker_id] = spawn_actor(Worker);
            }
        }
    }
}
```

### Actor Pool

```aether
actor Pool {
    state ActorRef[] workers;
    state bool[] available;
    
    receive(msg) {
        match msg {
            SubmitTask(task) => {
                // Find available worker and assign task
            }
        }
    }
}
```

## Standard Library

### I/O Module (`io.ae`)

```aether
import io;

print("Hello, World!");
print_int(42);
print_float(3.14);
print_bool(true);
```

### Collections Module (`collections.ae`)

```aether
import collections;

let array = spawn_actor(DynamicArray);
send(array, Init(10));
send(array, Add(42));
```

### Actor Utilities (`actors.ae`)

```aether
import actors;

let registry = spawn_actor(ActorRegistry);
send(registry, Register("counter", counter_actor));
```

## Compilation

### Compile Aether to C

```bash
./aetherc_new input.ae output.c
```

### Compile and Run

```bash
gcc output.c ../runtime/*.c -o program -lpthread
./program
```

## Error Handling

### Compile-time Errors

- Type mismatches
- Undefined variables
- Syntax errors
- Actor reference errors

### Runtime Errors

- Actor not found
- Message delivery failure
- Memory allocation failure

## Best Practices

1. **Keep actors small and focused**
2. **Use immutable data in messages**
3. **Implement proper error handling**
4. **Use supervision patterns for fault tolerance**
5. **Profile and optimize message passing**

## Examples

See the `examples/` directory for complete programs:

- `hello_actors.ae`: Basic actor communication
- `ring_benchmark.ae`: Performance testing
- `supervisor.ae`: Fault tolerance patterns
