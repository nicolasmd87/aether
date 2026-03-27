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
| `int` | 32-bit signed integer | `42`, `-17`, `0xFF`, `0b1010` |
| `float` | 64-bit floating point | `3.14`, `-0.5` |
| `string` | UTF-8 encoded strings | `"Hello"` |
| `bool` | Boolean type | `true`, `false` |
| `void` | No value (for functions) | - |
| `long` | 64-bit signed integer | `long x = 0` |
| `ptr` | Raw pointer (for C interop) | `null` |

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

### Numeric Literal Formats

Integer literals support hex, octal, and binary notation. Underscore separators are allowed anywhere in digits for readability.

| Format | Prefix | Example | Value |
|--------|--------|---------|-------|
| Decimal | (none) | `255` | 255 |
| Hexadecimal | `0x` / `0X` | `0xFF` | 255 |
| Octal | `0o` / `0O` | `0o377` | 255 |
| Binary | `0b` / `0B` | `0b1111_1111` | 255 |

```aether
mask = 0xFF
flags = 0b1010_0101
perms = 0o755
big = 1_000_000
```

All numeric literal formats work with bitwise operators and in any expression context.

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

### Null

The `null` keyword represents a null pointer, typed as `ptr`:

```aether
x = null             // inferred: ptr
if x == null {
    println("no value")
}
```

### Constants

Top-level constants are declared with `const`:

```aether
const MAX_SIZE = 100
const GREETING = "hello"
const PI = 3

main() {
    println(MAX_SIZE)           // 100
    half = MAX_SIZE / 2         // constants work in expressions
}
```

Constants are emitted as `#define` in generated C — zero runtime cost.

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

### Multi-Statement Arrow Bodies

Arrow functions can have block bodies with `-> { ... }`. The last expression is the implicit return value:

```aether
// Single expression (existing)
twice(x) -> x * 2

// Multi-statement with implicit return
sum_squares(a, b) -> {
    sq_a = a * a
    sq_b = b * b
    sq_a + sq_b
}

// Multi-statement with early return
clamp(x, lo, hi) -> {
    if x < lo {
        return lo
    }
    if x > hi {
        return hi
    }
    x
}
```

This allows complex logic in arrow-style functions without switching to block syntax.

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

### If-Expressions

`if`/`else` can be used as an expression that produces a value (like a ternary operator):

```aether
// Assign based on condition
max = if a > b { a } else { b }

// Use inline in function calls
println(if x > 0 { x } else { 0 - x })

// Nested if-expressions
grade = if score >= 90 { 4 } else { if score >= 80 { 3 } else { 2 } }
```

Both branches must produce a value of the same type. The `else` branch is required.

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

### Range-Based For Loops

Iterate over a range with `for VAR in START..END`:

```aether
// Prints 0 1 2 3 4
for i in 0..5 {
    print(i)
    print(" ")
}

// Sum with variable bound
sum = 0
for i in 1..n {
    sum += i
}
```

The range `start..end` is exclusive of `end` (like Python's `range(start, end)`). It desugars to a C-style for loop internally.

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

### String Matching

Strings are compared by content (via `strcmp`), so string literal arms work correctly:

```aether
match (command) {
    "start" -> { println("starting...") }
    "stop" -> { println("stopping...") }
    "help" -> { println("available: start, stop, help") }
    _ -> { println("unknown command") }
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

Aether uses **deterministic scope-exit cleanup** -- no garbage collector, no GC pauses. The primary mechanism is `defer`.

### `defer` for Cleanup (default)

Allocate, immediately defer the free, then use the resource. Cleanup runs at scope exit in LIFO order:

```aether
import std.list

main() {
    items = list.new()
    defer list.free(items)

    list.add(items, "hello")
    print(list.size(items))
    print("\n")
    // list.free(items) runs here (scope exit)
}
```

This works with any function, not just stdlib types.

### Returning Allocated Values

The caller receives ownership and is responsible for cleanup:

```aether
import std.list

build_list(n) : ptr {
    result = list.new()
    i = 0
    while i < n {
        list.add(result, i)
        i = i + 1
    }
    return result
}

main() {
    items = build_list(10)
    defer list.free(items)

    print(list.size(items))
    print("\n")
}
```

See [Memory Management Guide](memory-management.md) for the full reference.

### Multiple Return Values (Result Types)

Functions can return multiple values using comma-separated returns:

```aether
safe_divide(a: int, b: int) -> {
    if b == 0 {
        return 0, "division by zero"
    }
    return a / b, ""
}

main() {
    result, err = safe_divide(10, 3)
    if err != "" {
        println("Error: ${err}")
    } else {
        println("Result: ${result}")
    }

    // Discard unwanted values with _
    val, _ = safe_divide(42, 7)
    println(val)
}
```

The error convention: empty string `""` means no error, non-empty string is the error message. Use `!= ""` to check for errors.

Tuple destructuring creates typed local variables. The compiler generates C structs for each unique tuple type (`_tuple_int_string`, etc.).

Error propagation across function boundaries works correctly:

```aether
checked_op(x: int) -> {
    val, err = safe_divide(x, 2)
    if err != "" {
        return 0, err    // propagate the error
    }
    return val, ""
}
```

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

### Receive Timeouts

The `after` clause fires a handler if no message arrives within N milliseconds:

```aether
actor Monitor {
    state alive = 1

    receive {
        Heartbeat -> { alive = 1 }
    } after 5000 -> {
        println("No heartbeat for 5 seconds")
        alive = 0
    }
}
```

The timeout is one-shot: it is cancelled when any message is received. The countdown starts when the actor's mailbox becomes empty.

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

> **String comparison:** When both operands are strings, `==` and `!=` compare by content (using `strcmp` in the generated C), not by pointer identity. Two strings with the same content are always equal regardless of how they were allocated.

### Bitwise Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `&` | Bitwise AND | `flags & mask` |
| `\|` | Bitwise OR | `flags \| bit` |
| `^` | Bitwise XOR | `a ^ b` |
| `~` | Bitwise NOT | `~mask` |
| `<<` | Left shift | `1 << 4` |
| `>>` | Right shift | `n >> 2` |

Bitwise operators work on `int` and `long` values and map directly to C operators (zero runtime cost).

```aether
flags = 255
mask = flags & 15       // 15
set = flags | 256       // 511
flipped = flags ^ 255   // 0
shifted = 1 << 4        // 16
```

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
| `%=` | Modulo and assign | `x %= 5` |
| `&=` | Bitwise AND assign | `x &= mask` |
| `\|=` | Bitwise OR assign | `x \|= bit` |
| `^=` | Bitwise XOR assign | `x ^= mask` |
| `<<=` | Left shift assign | `x <<= 4` |
| `>>=` | Right shift assign | `x >>= 2` |

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
2. `!` `-` `~` (unary) `++` `--` - Unary operators
3. `*` `/` `%` - Multiplicative
4. `+` `-` - Additive
5. `<<` `>>` - Bitwise shift
6. `<` `>` `<=` `>=` - Relational
7. `==` `!=` - Equality
8. `&` - Bitwise AND
9. `^` - Bitwise XOR
10. `|` - Bitwise OR
11. `&&` - Logical AND
12. `||` - Logical OR
13. `=` `+=` `-=` `*=` `/=` `%=` `&=` `|=` `^=` `<<=` `>>=` - Assignment
14. `!` `?` - Actor send/ask

---

## Modules and Imports

### Standard Library Imports

```aether
import std.file;         // File operations
import std.string;       // String utilities
import std.list;         // ArrayList
import std.http;         // HTTP client & server
import std.json;         // JSON parsing

// Use with namespace syntax
result = string.new("hello");
if (file.exists("config.txt") == 1) { }
```

### Import with Alias (Planned)

> **Note:** Import aliasing is parsed but not yet fully functional. Use the default namespace for now.

```aether
// Planned syntax:
// import std.string as str;
// s = str.new("hello")

// Current workaround: use the module name directly
import std.string
s = string.new("hello")
len = string.length(s)
string.release(s)
```

### Local Module Imports

```aether
import utils;           // Loads lib/utils/module.ae
import helpers;         // Loads lib/helpers/module.ae

result = utils.double_value(21);
```

### Available Standard Library Modules

| Module | Namespace | Description |
|--------|-----------|-------------|
| `std.file` | `file` | File operations (`file.open()`, `file.exists()`) |
| `std.dir` | `dir` | Directory operations (`dir.list()`, `dir.create()`) |
| `std.path` | `path` | Path utilities (`path.join()`, `path.basename()`) |
| `std.string` | `string` | String manipulation (`string.new()`, `string.length()`) |
| `std.list` | `list` | Dynamic array (`list.new()`, `list.add()`) |
| `std.map` | `map` | Hash map (`map.new()`, `map.put()`) |
| `std.json` | `json` | JSON encoding/decoding (`json.parse()`, `json.free()`) |
| `std.http` | `http` | HTTP client & server (`http.get()`, `http.server_create()`) |
| `std.tcp` | `tcp` | TCP sockets (`tcp.connect()`, `tcp.send()`) |
| `std.math` | `math` | Math functions (`math.sqrt()`, `math.sin()`) |
| `std.log` | `log` | Logging utilities (`log.init()`, `log.write()`) |
| `std.io` | `io` | Input/output (`io.print()`, `io.getenv()`) |

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
| `print(value)` | Print to stdout (no newline) |
| `println(value)` | Print to stdout followed by a newline |
| `print_char(code)` | Print a single character by ASCII/Unicode code point |

String interpolation is supported inside double-quoted strings using `${expr}`:

```aether
name = "Alice"
age = 30
println("Hello, ${name}! You are ${age} years old.")
```

Interpolated strings produce a `ptr` (heap-allocated C string) when used as values:

```aether
msg = "Hello, ${name}!"     // msg is a ptr (char*), not an int
tcp_send(conn, msg)          // can be passed to any function expecting ptr
```

When used directly inside `print`/`println`, the compiler optimizes to a `printf` call (no allocation).

### Timing

| Function | Description |
|----------|-------------|
| `clock_ns()` | Returns current time in nanoseconds (`long`) |
| `sleep(ms)` | Pause execution (milliseconds) |

### Concurrency

| Function | Description |
|----------|-------------|
| `spawn(ActorName())` | Create actor instance |
| `wait_for_idle()` | Wait for all actors to finish |

### Environment & Process

| Function | Description |
|----------|-------------|
| `getenv(name)` | Get environment variable (returns string) |
| `atoi(s)` | Convert string to int |
| `exit(code)` | Terminate program with exit code (defaults to 0) |

---

## Keywords

The following identifiers are reserved:

| Keyword | Purpose |
|---------|---------|
| `if`, `else` | Conditionals |
| `while`, `for`, `in`, `break`, `continue` | Loops |
| `return` | Function return |
| `match`, `switch`, `case`, `default` | Pattern matching / dispatch |
| `actor`, `receive`, `spawn`, `reply`, `after` | Actor system |
| `message`, `struct` | Type definitions |
| `state` | Actor state (only reserved inside actor bodies) |
| `import`, `extern` | Modules and C interop |
| `const` | Top-level constants |
| `defer` | Scope-exit cleanup |
| `null`, `true`, `false` | Literals |
| `when` | Guard clauses |
| `int`, `float`, `string`, `bool`, `void`, `ptr`, `long` | Type names |

Note: `state` is context-sensitive — it is a keyword only inside actor bodies. In all other code, `state` can be used as a regular variable name.

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
ae run program.ae           # Compile and run (fast, -O0)
ae build program.ae -o out  # Compile to optimised executable (-O2 + aether.toml cflags)
ae init myproject           # Scaffold a new project
ae test                     # Discover and run .ae test files
ae cache                    # Show build cache info
ae cache clear              # Purge build cache
```

`ae run` and `ae build` also accept:

```bash
# Include extra C source files (e.g. FFI helpers, renderer backends)
ae build main.ae -o app --extra src/ffi.c --extra src/renderer.c

# Multiple --extra flags are additive; also merged with extra_sources from aether.toml
```

### Using the Compiler Directly

```bash
# Compile to C
aetherc program.ae output.c

# Emit a C header for embedding Aether actors in a C application
# Generates message structs, MSG_* constants, and spawn function prototypes
aetherc program.ae output.c --emit-header

# Print parsed AST (for debugging, no code generation)
aetherc --dump-ast program.ae
```

---

## Type System

Aether uses static typing with full type inference — explicit annotations are never required, but are always accepted.

### Inference rules

- **Local variables**: Inferred from their initializer (`x = 42` → `int`)
- **Function parameters**: Inferred from call sites across the whole program, including through deep call chains (`main → f → g → h`)
- **Return types**: Inferred from `return` statements and arrow-body expressions
- **Constraint solving**: Iterative constraint propagation handles complex interdependencies

### Type annotations are optional

```aether
// All three are equivalent:
add(a, b) { return a + b; }          // fully inferred from call sites
add(a: int, b: int) { return a + b; } // explicit
add(a, b: int) { return a + b; }     // mixed
```

Annotations are useful for documentation or when the type cannot be determined from call sites alone (e.g. a function that is never called, or an `extern` parameter).

### `extern` requires annotations

The compiler cannot infer types of external C functions — parameter types must be declared explicitly:

```aether
extern malloc(n: int) -> ptr
extern free(p: ptr) -> void
```

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
