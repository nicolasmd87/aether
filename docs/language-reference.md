# Aether Language Reference

Complete syntax and semantics of the Aether programming language.

## Overview

Aether is a statically-typed, compiled language combining Erlang-inspired actor concurrency with type inference. It features clean, minimal syntax and compiles to C code.

## Table of Contents

1. [Types](#types)
2. [Variables](#variables)
3. [Functions](#functions)
4. [Pattern Matching Functions](#pattern-matching-functions)
5. [Control Flow](#control-flow)
6. [Match Statements](#match-statements)
7. [Switch Statements](#switch-statements)
8. [Defer Statement](#defer-statement)
9. [Memory Management](#memory-management)
10. [Structs](#structs)
11. [Messages](#messages)
12. [Actors](#actors)
13. [Operators](#operators)
14. [Modules and Imports](#modules-and-imports)
15. [Extern Functions](#extern-functions)
16. [Built-in Functions](#built-in-functions)
17. [Comments](#comments)
18. [Compilation](#compilation)

---

## Types

### Primitive Types

| Type | Description | Example |
|------|-------------|---------|
| `int` | 32-bit signed integer | `42`, `-17` |
| `float` | 64-bit floating point | `3.14`, `-0.5` |
| `string` | UTF-8 encoded strings | `"Hello"` |
| `bool` | Boolean type | `true`, `false` |
| `void` | No value (for functions) | - |
| `ptr` | Raw pointer (for C interop) | - |

### Composite Types

| Type | Description |
|------|-------------|
| `struct` | User-defined composite data |
| `actor` | Concurrency primitive with state |
| `message` | Structured data for actor communication |
| `array` | Fixed-size homogeneous collections |

### Array Types

```aether
int[10] numbers;           // Array of 10 integers
string[5] names;           // Array of 5 strings
float[100] values;         // Array of 100 floats
```

---

## Variables

Variables support both explicit types and automatic type inference:

```aether
// Type inference (recommended)
x = 10;
y = 20;
name = "Alice";
pi = 3.14159;

// Explicit types (optional)
int z = 30;
string greeting = "Hello";
float temperature = 98.6;
```

Variables are inferred from their initialization or usage context.

---

## Functions

Functions support type inference for parameters and return types:

```aether
// Type inference (recommended)
add(a, b) {
    return a + b;
}

greet(name) {
    print("Hello, ");
    print(name);
    print("\n");
}

// Explicit types (optional, for clarity)
int add_explicit(int a, int b) {
    return a + b;
}

void print_hello() {
    print("Hello\n");
}
```

Functions can return values or `void`. The `main()` function is the entry point.

---

## Pattern Matching Functions

Aether supports Erlang-style function clauses with pattern matching and guard clauses:

### Basic Pattern Matching

```aether
// Match on literal values
factorial(0) -> 1;
factorial(n) -> n * factorial(n - 1);

// Fibonacci with multiple clauses
fib(0) -> 0;
fib(1) -> 1;
fib(n) -> fib(n - 1) + fib(n - 2);
```

### Guard Clauses

Guards add conditions using the `when` keyword:

```aether
// Classify numbers using guards
classify(x) when x < 0 -> print("negative\n");
classify(x) when x == 0 -> print("zero\n");
classify(x) when x > 0 -> print("positive\n");

// Factorial with guard
factorial(0) -> 1;
factorial(n) when n > 0 -> n * factorial(n - 1);

// Grade calculation with multiple ranges
grade(score) when score >= 90 -> "A";
grade(score) when score >= 80 -> "B";
grade(score) when score >= 70 -> "C";
grade(score) when score >= 60 -> "D";
grade(score) when score < 60 -> "F";
```

### Multi-parameter Guards

```aether
// Max of two numbers
max(a, b) when a >= b -> a;
max(a, b) when a < b -> b;

// GCD with pattern matching
gcd(a, 0) -> a;
gcd(a, b) when b > 0 -> gcd(b, a - (a / b) * b);
```

### Mutual Recursion with Guards

```aether
is_even(n) when n == 0 -> 1;
is_even(n) when n > 0 -> is_odd(n - 1);

is_odd(n) when n == 0 -> 0;
is_odd(n) when n > 0 -> is_even(n - 1);
```

---

## Control Flow

### If Statements

```aether
if (x > 0) {
    print("Positive\n");
} else if (x < 0) {
    print("Negative\n");
} else {
    print("Zero\n");
}
```

### While Loops

```aether
i = 0;
while (i < 10) {
    print(i);
    print("\n");
    i = i + 1;
}
```

### For Loops

```aether
for (i = 0; i < 10; i = i + 1) {
    print(i);
    print("\n");
}
```

### Loop Control

```aether
// Break - exit loop early
for (i = 0; i < 100; i = i + 1) {
    if (i == 50) {
        break;
    }
}

// Continue - skip to next iteration
for (i = 0; i < 10; i = i + 1) {
    if (i == 5) {
        continue;
    }
    print(i);
}
```

---

## Match Statements

Match statements provide pattern-based dispatch:

### Integer Matching

```aether
match (value) {
    0 -> { print("zero\n"); }
    1 -> { print("one\n"); }
    2 -> { print("two\n"); }
    _ -> { print("other\n"); }
}
```

### List Pattern Matching

Arrays can be matched with list patterns. Requires a corresponding `_len` variable:

```aether
nums = [1, 2, 3];
nums_len = 3;

match (nums) {
    [] -> { print("empty list\n"); }
    [x] -> {
        print("single element: ");
        print(x);
        print("\n");
    }
    [a, b] -> {
        print("pair: ");
        print(a);
        print(", ");
        print(b);
        print("\n");
    }
    [h|t] -> {
        print("head: ");
        print(h);
        print(", tail has rest\n");
    }
}
```

### List Pattern Types

| Pattern | Matches | Bindings |
|---------|---------|----------|
| `[]` | Empty array | None |
| `[x]` | Single-element array | `x` = element |
| `[x, y]` | Two-element array | `x`, `y` = elements |
| `[x, y, z]` | Three-element array | `x`, `y`, `z` = elements |
| `[h\|t]` | Non-empty array | `h` = first, `t` = rest |

---

## Switch Statements

C-style switch for simple value dispatch:

```aether
switch (month) {
    case 1: name = "January";
    case 2: name = "February";
    case 3: name = "March";
    // ... more cases
    default: name = "Invalid";
}
```

### Switch vs Match

| Feature | `switch` | `match` |
|---------|----------|---------|
| Pattern types | Integer/string literals | Literals, lists, wildcards |
| Binding | No | Yes (captures variables) |
| Use case | Simple dispatch | Pattern destructuring |

---

## Defer Statement

The `defer` statement schedules code to run when leaving the current scope:

```aether
process_file() {
    handle = open_resource();
    defer close_resource(handle);  // Runs when function exits

    use_resource(handle);
    use_resource(handle);
    // close_resource(handle) called automatically here
}
```

### LIFO Order

Multiple defers execute in Last-In-First-Out order:

```aether
example() {
    defer print("First\n");   // Runs third
    defer print("Second\n");  // Runs second
    defer print("Third\n");   // Runs first
}
// Output: Third, Second, First
```

### Use Cases

- Resource cleanup (files, connections)
- Unlocking mutexes
- Logging function exit
- Guaranteed cleanup regardless of return path

---

## Memory Management

Aether uses **deterministic scope-exit cleanup** — no garbage collector, no GC pauses.

### Auto Mode (default)

Variables assigned from stdlib `*_new()` calls are automatically freed at scope exit:

```aether
import std.list

main() {
    items = list_new()          // allocated
    list_add(items, "hello")
    print(list_size(items))
    print("\n")
    // list_free(items) injected automatically here
}
```

### `@manual` Annotation

Use `@manual` when a value **escapes its scope** — returned from a function, passed to an actor, or stored somewhere that outlives the current block:

```aether
import std.list

// Returns ownership to caller — must NOT free here
build_list(n) : ptr {
    @manual result = list_new()
    i = 0
    while i < n {
        list_add(result, i)
        i = i + 1
    }
    return result
}

main() {
    items = build_list(10)   // auto-free fires here instead
    print(list_size(items))
    print("\n")
}
```

Without `@manual`, auto mode would free `result` at the end of `build_list` — before the caller can use it.

**Use `@manual` when:**
- Returning a `*_new()` value from a function
- Passing it to an actor via `!` (actor owns it)
- Storing it in actor `state` (lives for actor's lifetime)

### Manual Mode

Disable all auto-free for an entire project:

```toml
# aether.toml
[memory]
mode = "manual"
```

Or for a single run:

```bash
ae run --no-auto-free file.ae
```

See [Memory Management Guide](memory-management.md) for the full reference.

---

## Structs

Structs group related data:

```aether
struct Point {
    x,
    y
}

struct Person {
    name,
    age
}

main() {
    p = Point { x: 10, y: 20 };
    print(p.x);  // 10
    print(p.y);  // 20
}
```

### Explicit Field Types

```aether
struct Point {
    int x,
    int y
}

struct Config {
    string name,
    int timeout,
    float threshold
}
```

---

## Messages

Messages define structured data for actor communication:

```aether
message Increment {
    amount: int
}

message Greet {
    name: string
}

message SetPosition {
    x: int,
    y: int
}

message Reset {}  // Empty message
```

---

## Actors

Actors are the core concurrency primitive with encapsulated state and message handling.

### Actor Definition

```aether
actor Counter {
    state count = 0;

    receive {
        Increment(amount) -> {
            count = count + amount;
        }
        GetCount() -> {
            print(count);
            print("\n");
        }
        _ -> {
            print("Unknown message\n");
        }
    }
}
```

### State Variables

State persists across messages:

```aether
actor BankAccount {
    state balance = 0;
    state transactions = 0;
    state int[100] history;
}
```

### Spawning Actors

```aether
counter = spawn(Counter());
calculator = spawn(Calculator());
```

### Sending Messages (Fire-and-Forget)

```aether
counter ! Increment { amount: 10 };
counter ! Reset {};
```

### Ask Pattern (Request-Reply)

The `?` operator sends a message and blocks until the actor replies. The compiler
infers the reply type from the actor's receive handler and extracts the first field
of the reply message automatically. Multiple concurrent asks to the same actor are
supported — each message carries its own reply slot.

```aether
// Synchronous request-reply — result is an int (from Result.value)
result = calculator ? Add { a: 5, b: 3 };
```

If the handler does not call `reply` within the timeout (default 5 seconds), `?`
returns 0.

### Reply Statement

The `reply` statement sends a response back to the waiting `?` caller. Omitting
`reply` in a handler that was invoked via `?` causes the caller to time out.

Actors respond using the `reply` statement:

```aether
actor Calculator {
    receive {
        Add(a, b) -> {
            result = a + b;
            reply Result { value: result };
        }
    }
}
```

### Wildcard Handler

The `_` pattern catches unmatched messages:

```aether
receive {
    Known() -> { /* handle */ }
    _ -> { print("Unknown message\n"); }
}
```

---

## Operators

### Arithmetic Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `+` | Addition | `a + b` |
| `-` | Subtraction | `a - b` |
| `*` | Multiplication | `a * b` |
| `/` | Division | `a / b` |
| `%` | Modulo | `a % b` |

### Comparison Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `==` | Equal | `a == b` |
| `!=` | Not equal | `a != b` |
| `<` | Less than | `a < b` |
| `>` | Greater than | `a > b` |
| `<=` | Less or equal | `a <= b` |
| `>=` | Greater or equal | `a >= b` |

### Logical Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `&&` | Logical AND | `a && b` |
| `\|\|` | Logical OR | `a \|\| b` |
| `!` | Logical NOT | `!a` |

### Assignment Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `=` | Assignment | `x = 5` |
| `+=` | Add and assign | `x += 5` |
| `-=` | Subtract and assign | `x -= 5` |
| `*=` | Multiply and assign | `x *= 5` |
| `/=` | Divide and assign | `x /= 5` |

### Postfix Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `++` | Post-increment | `i++` |
| `--` | Post-decrement | `i--` |

### Actor Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `!` | Send message (async) | `actor ! Msg {}` |
| `?` | Ask (request-reply) | `actor ? Query {}` |

### Member Access

| Operator | Description | Example |
|----------|-------------|---------|
| `.` | Field access | `point.x` |
| `[]` | Array index | `arr[0]` |

### Operator Precedence (High to Low)

1. `()` `[]` `.` - Grouping, indexing, member access
2. `!` `-` (unary) `++` `--` - Unary operators
3. `*` `/` `%` - Multiplicative
4. `+` `-` - Additive
5. `<` `>` `<=` `>=` - Relational
6. `==` `!=` - Equality
7. `&&` - Logical AND
8. `||` - Logical OR
9. `=` `+=` `-=` `*=` `/=` - Assignment
10. `!` `?` - Actor send/ask

---

## Modules and Imports

### Standard Library Imports

```aether
import std.fs;           // File system
import std.string;       // String utilities
import std.collections;  // Lists, maps
import std.net;          // Networking
import std.json;         // JSON parsing

// Use with namespace syntax
result = string.new("hello");
if (fs.file_exists("config.txt") == 1) { }
```

### Import with Alias

```aether
import std.collections as col;
import std.string as str;

list = col.list_new();
s = str.new("hello");
```

### Local Module Imports

```aether
import utils;           // Loads lib/utils/module.ae
import helpers;         // Loads lib/helpers/module.ae

result = utils.double_value(21);
```

### Available Standard Library Modules

| Module | Description |
|--------|-------------|
| `std.fs` | File operations |
| `std.string` | String manipulation |
| `std.collections` | Lists, maps, sets |
| `std.net` | TCP/UDP networking |
| `std.json` | JSON encoding/decoding |
| `std.io` | Input/output |
| `std.math` | Math functions |
| `std.log` | Logging utilities |

---

## Extern Functions

Declare external C functions:

```aether
extern puts(s: string) -> int;
extern malloc(size: int) -> ptr;
extern free(p: ptr) -> void;

main() {
    puts("Direct C call!");
}
```

Externs are useful for:
- Calling C standard library
- Custom C extensions
- Platform-specific APIs

---

## Built-in Functions

### I/O

| Function | Description |
|----------|-------------|
| `print(value)` | Print to stdout |

### Concurrency

| Function | Description |
|----------|-------------|
| `spawn(ActorName())` | Create actor instance |
| `wait_for_idle()` | Wait for all actors to finish |
| `sleep(ms)` | Pause execution (milliseconds) |

---

## Comments

```aether
// Single-line comment

/* Multi-line
   comment */
```

---

## Compilation

### Using the CLI

```bash
# Compile and run
ae run program.ae

# Compile to executable
ae build program.ae -o program

# Initialize new project
ae init myproject

# Run tests
ae test
```

### Using the Compiler Directly

```bash
# Compile to C
aetherc program.ae output.c

# Emit header for C embedding
aetherc program.ae output.c --emit-header
```

---

## Type System

Aether uses static typing with bidirectional type inference:

- **Local inference**: Variables infer types from initialization
- **Parameter inference**: Function parameters infer from usage
- **Return inference**: Return types infer from return statements
- **Constraint solving**: Complex expressions resolved iteratively

Explicit types are optional but can improve clarity:

```aether
// Both are valid:
x = 42;
int y = 42;
```

---

## Example Programs

### Hello World

```aether
main() {
    print("Hello, World!\n");
}
```

### Factorial with Pattern Matching

```aether
factorial(0) -> 1;
factorial(n) when n > 0 -> n * factorial(n - 1);

main() {
    print(factorial(10));  // 3628800
}
```

### Counter Actor

```aether
message Increment { amount: int }
message GetCount {}

actor Counter {
    state count = 0;

    receive {
        Increment(amount) -> {
            count = count + amount;
        }
        GetCount() -> {
            print(count);
            print("\n");
        }
    }
}

main() {
    c = spawn(Counter());
    c ! Increment { amount: 5 };
    c ! Increment { amount: 3 };
    c ! GetCount {};
    wait_for_idle();  // Output: 8
}
```

### Resource Management with Defer

```aether
extern fopen(path: string, mode: string) -> ptr;
extern fclose(file: ptr) -> int;

process_file(path) {
    file = fopen(path, "r");
    defer fclose(file);

    // Process file...
    // fclose called automatically
}
```
