# Aether Language Reference

Complete syntax and semantics of the Aether programming language.

## Overview

Aether is a statically-typed, compiled language combining Erlang-inspired actor concurrency with ML-family type inference (Hindley-Milner). It features clean, minimal syntax and compiles to optimized C code for maximum performance.

## Types

### Primitive Types

- `int` - 32-bit signed integer
- `float` - 64-bit floating point
- `string` - UTF-8 encoded strings
- `bool` - Boolean type
- `void` - No value (for functions that don't return)

### User-Defined Types

- `struct` - Composite data type
- `actor` - Concurrency primitive with state and message handling
- `array` - Fixed-size homogeneous collections

## Variables

Variables support both explicit types and automatic type inference:

```aether
// Type inference (recommended)
x = 10
y = 20
name = "Alice"

// Explicit types (optional)
int z = 30
string greeting = "Hello"
```

Variables are inferred from their initialization or usage context.

## Functions

Functions support type inference for parameters and return types:

```aether
// Type inference (recommended)
add(a, b) {
    return a + b
}

greet(name) {
    print("Hello, ")
    print(name)
    print("!\n")
}

// Explicit types (optional, for clarity)
int add_explicit(int a, int b) {
    return a + b
}

void print_hello() {
    print("Hello\n")
}
```

Functions can return values or `void`. The `main()` function is the entry point.
Types are inferred from usage when not explicitly specified.

## Control Flow

### If Statements

```aether
if (x > 0) {
    print("Positive\n")
} else {
    print("Non-positive\n")
}
```

### While Loops

```aether
i = 0
while (i < 10) {
    print(i)
    print("\n")
    i = i + 1
}
```

### For Loops

```aether
for (i = 0; i < 10; i = i + 1) {
    print(i)
    print("\n")
}
```

## Structs

```aether
struct Point {
    int x
    int y
}

main() {
    p = Point{ x: 10, y: 20 }
    print(p.x)
    print(p.y)
}
```

Structs group related data. Fields are accessed with `.` operator.
Struct literals use the `StructName{ field: value }` syntax.

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

State variables are declared with `state` keyword and persist across messages:

```aether
actor Counter {
    state count = 0
    state total = 0
}
```

### Message Handling

The `receive` block defines how an actor processes messages. The `msg` parameter contains:
- `msg.type` - Message type (int)
- `msg.sender_id` - ID of sending actor (int)
- `msg.payload_int` - Integer payload (int)
- `msg.payload_ptr` - Pointer payload (void*)

### Spawning Actors

```aether
c = spawn_Counter()
```

The compiler generates `spawn_ActorName()` functions automatically.

### Sending Messages

```aether
send_Counter(c, 1, 0)
```

The compiler generates `send_ActorName()` functions. Parameters:
- Actor instance
- Message type
- Integer payload

### Processing Messages

```aether
Counter_step(c)
```

The compiler generates `ActorName_step()` functions that process one message from the mailbox.

## Expressions

### Arithmetic

```aether
sum = a + b
diff = a - b
prod = a * b
quot = a / b
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
i++
i--
```

### Member Access

```aether
point.x = 10
msg.type = 1
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
