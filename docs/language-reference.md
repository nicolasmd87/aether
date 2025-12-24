# Aether Language Reference

Complete syntax and semantics of the Aether programming language.

## Overview

Aether is a statically-typed, compiled language with actor-based concurrency. It compiles to C for maximum performance.

## Types

### Primitive Types

- `int` - 32-bit signed integer
- `void` - No value (for functions that don't return)

### User-Defined Types

- `struct` - Composite data type
- `actor` - Concurrency primitive with state and message handling

## Variables

```aether
int x = 10;
int y;
y = 20;
```

Variables must be declared before use. Type is required.

## Functions

```aether
int add(int a, int b) {
    return a + b;
}

void print_hello() {
    print("Hello\n");
}
```

Functions can return values or `void`. The `main()` function is the entry point.

## Control Flow

### If Statements

```aether
if (x > 0) {
    print("Positive\n");
} else {
    print("Non-positive\n");
}
```

### While Loops

```aether
int i = 0;
while (i < 10) {
    print("%d\n", i);
    i = i + 1;
}
```

### For Loops

```aether
for (int i = 0; i < 10; i = i + 1) {
    print("%d\n", i);
}
```

## Structs

```aether
struct Point {
    int x;
    int y;
}

main() {
    Point p;
    p.x = 10;
    p.y = 20;
}
```

Structs group related data. Fields are accessed with `.` operator.

## Actors

Actors are the core concurrency primitive. They have state and process messages.

### Actor Definition

```aether
actor Counter {
    state int count = 0;
    
    receive(msg) {
        if (msg.type == 1) {
            count = count + 1;
        }
    }
}
```

### Actor State

State variables are declared with `state` keyword and persist across messages.

### Message Handling

The `receive` block defines how an actor processes messages. The `msg` parameter contains:
- `msg.type` - Message type (int)
- `msg.sender_id` - ID of sending actor (int)
- `msg.payload_int` - Integer payload (int)
- `msg.payload_ptr` - Pointer payload (void*)

### Spawning Actors

```aether
Counter c = spawn_Counter();
```

The compiler generates `spawn_ActorName()` functions automatically.

### Sending Messages

```aether
send_Counter(c, 1, 0);
```

The compiler generates `send_ActorName()` functions. Parameters:
- Actor instance
- Message type
- Integer payload

### Processing Messages

```aether
Counter_step(c);
```

The compiler generates `ActorName_step()` functions that process one message from the mailbox.

## Expressions

### Arithmetic

```aether
int sum = a + b;
int diff = a - b;
int prod = a * b;
int quot = a / b;
```

### Comparison

```aether
if (a == b) { }
if (a != b) { }
if (a < b) { }
if (a > b) { }
if (a <= b) { }
if (a >= b) { }
```

### Postfix Operators

```aether
i++;
i--;
```

### Member Access

```aether
point.x = 10;
msg.type = 1;
```

## Built-in Functions

- `print(format, ...)` - Print formatted string (similar to printf)

## Comments

```aether
// Single-line comment

/* Multi-line
   comment */
```

## Compilation

Aether programs compile to C code, which is then compiled to native executables.

```bash
aetherc program.ae output.c
gcc output.c -Iruntime -o program
```

## Type System

Aether uses static typing. All variables and function parameters must have explicit types. Type checking occurs during compilation.
