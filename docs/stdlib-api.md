# Aether Runtime Library Guide

## Overview

Aether provides a runtime library for strings, I/O, math, and actor concurrency. The runtime is automatically linked when you compile Aether programs.

## String Library (`aether_string.h`)

### Types

```c
typedef struct AetherString {
    char* data;
    size_t length;
    size_t capacity;
    int ref_count;
} AetherString;
```

### Available Functions

#### Creating Strings

- `aether_string_new(const char* cstr)` - Create from C string
- `aether_string_from_literal(const char* cstr)` - Alias for new
- `aether_string_empty()` - Create empty string

#### String Operations

- `aether_string_concat(AetherString* a, AetherString* b)` - Concatenate strings
- `aether_string_length(AetherString* str)` - Get length
- `aether_string_char_at(AetherString* str, int index)` - Get character
- `aether_string_equals(AetherString* a, AetherString* b)` - Check equality
- `aether_string_compare(AetherString* a, AetherString* b)` - Compare (-1, 0, 1)

#### String Methods

- `aether_string_starts_with()` - Check prefix
- `aether_string_ends_with()` - Check suffix
- `aether_string_contains()` - Search for substring
- `aether_string_index_of()` - Find position
- `aether_string_substring()` - Extract substring
- `aether_string_to_upper()` - Convert to uppercase
- `aether_string_to_lower()` - Convert to lowercase
- `aether_string_trim()` - Remove whitespace

#### Conversion

- `aether_string_to_cstr(AetherString* str)` - Get C string
- `aether_string_from_int(int value)` - Convert int to string
- `aether_string_from_float(float value)` - Convert float to string

#### Memory Management

- `aether_string_retain(AetherString* str)` - Increment reference count
- `aether_string_release(AetherString* str)` - Decrement and free if zero

**Note**: Strings are reference-counted. Use retain/release for manual memory management.

---

## I/O Library (`aether_io.h`)

### Console Output

The primary I/O function in Aether is `print()`, which is a variadic function that works like C's `printf`:

```aether
print("Hello, World!\n")
print("Value: %d\n", x)
print("Float: %f\n", pi)
```

### File Operations

Complete filesystem library with file and directory operations:

```c
#include "std/fs/aether_fs.h"

// File operations
AetherFile* file = aether_file_open("data.txt", "r");
AetherString* content = aether_file_read_all(file);
aether_file_write(file, "Hello");
aether_file_close(file);

// Directory operations
aether_dir_create("output");
AetherDirList* files = aether_dir_list(".");
aether_dir_delete("temp");

// Path utilities
AetherString* path = aether_path_join("dir", "file.txt");
AetherString* ext = aether_path_extension("file.txt");
bool exists = aether_file_exists("test.txt");
```

See [std/fs/README.md](../std/fs/README.md) for complete documentation.

### Console Input

---

## Concurrency Functions

### Actor Management

- `spawn(ActorName())` - Create a new actor instance and return a reference to it

### Synchronization

- `wait_for_idle()` - Block until all actors have finished processing messages. Use this to synchronize the main thread with actor completion.

- `sleep(milliseconds)` - Pause execution for the specified number of milliseconds.

### Example

```aether
message Task { id: int }

actor Worker {
    state completed = 0

    receive {
        Task(id) -> {
            completed = completed + 1
        }
    }
}

main() {
    w = spawn(Worker())

    w ! Task { id: 1 }
    w ! Task { id: 2 }

    // Wait for all tasks to complete
    wait_for_idle()

    print("Completed: ")
    print(w.completed)
    print("\n")
}
```

---

## Math Library (`aether_math.h`)

### Basic Operations

- `aether_abs_int(int x)` - Absolute value (int)
- `aether_abs_float(float x)` - Absolute value (float)
- `aether_min_int(int a, int b)` - Minimum (int)
- `aether_max_int(int a, int b)` - Maximum (int)
- `aether_min_float(float a, float b)` - Minimum (float)
- `aether_max_float(float a, float b)` - Maximum (float)

### Advanced Math

- `aether_sqrt(float x)` - Square root
- `aether_pow(float base, float exp)` - Power
- `aether_sin(float x)` - Sine
- `aether_cos(float x)` - Cosine
- `aether_tan(float x)` - Tangent
- `aether_floor(float x)` - Floor
- `aether_ceil(float x)` - Ceiling
- `aether_round(float x)` - Round to nearest

### Random Numbers

- `aether_random_seed(unsigned int seed)` - Set random seed
- `aether_random_int(int min, int max)` - Random int in range
- `aether_random_float()` - Random float [0.0, 1.0)

### Constants

- `AETHER_PI` - 3.14159265358979323846
- `AETHER_E` - 2.71828182845904523536

---

## Actor Supervision (`aether_supervision.h`)

The supervision module provides:

- Supervision tree creation
- Restart strategies (one-for-one, one-for-all, rest-for-one)
- Automatic actor restart on failure
- Crash escalation

---

## Message Tracing (`aether_tracing.h`)

Message tracing is implemented for debugging actor systems:

- Initialize with `tracing_init(filename)`
- Enable/disable tracing
- Trace specific actors or all actors
- Logs message sends, receives, and processing

---

## Bounds Checking (`aether_bounds_check.h`)

Bounds checking is implemented for arrays and can be enabled at compile time:

```bash
# Debug mode (bounds checking enabled)
gcc -DAETHER_DEBUG output.c runtime/*.c -o program_debug

# Release mode (no overhead)
gcc output.c runtime/*.c -o program_release
```

Provides array bounds checking, null pointer checking, and assertions.

---

## Testing Framework (`aether_test.h`)

A basic testing framework is available for C-level tests:

```c
// Assertion macros available
assert_true(condition)
assert_eq(expected, actual)
assert_not_null(ptr)
```

---

## Memory Management

### Automatic

- Strings are reference-counted (use `retain`/`release`)
- Actors managed by runtime
- Stack allocations freed automatically

### Manual

For C interop or custom allocations:
- Use `malloc()` and `free()` from standard C library
- Manage string references with `aether_string_retain/release`
- Actors are freed when no longer referenced

---

## Best Practices

1. **Use `print()` for output** - Simple and reliable
2. **Enable bounds checking in debug** - Catches array errors
3. **Free string references** - Use `aether_string_release()` when done
4. **Test with all optimizations off first** - Easier debugging
5. **Use actors for concurrency** - Safer than manual threading

---

## Example: Complete Program

```aether
message Increment { amount: int }

actor Counter {
    state count = 0

    receive {
        Increment(amount) -> {
            count = count + amount
        }
    }
}

main() {
    print("Aether Runtime Example\n")
    counter = spawn(Counter())
    counter ! Increment { amount: 1 }
    counter ! Increment { amount: 1 }

    // Wait for all messages to be processed
    wait_for_idle()

    print("Final count: ")
    print(counter.count)
    print("\n")
}
```

