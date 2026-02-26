# Aether Standard Library Guide

## Overview

Aether provides a standard library for strings, I/O, math, file system, networking, and actor concurrency. The library is automatically linked when you compile Aether programs.

## Namespace Calling Convention

Functions are called using **namespace-style syntax**: `namespace.function()`

| Import | Namespace | Example Call |
|--------|-----------|--------------|
| `import std.string` | `string` | `string.new("hello")`, `string.release(s)` |
| `import std.file` | `file` | `file.exists("path")`, `file.open("path", "r")` |
| `import std.dir` | `dir` | `dir.exists("path")`, `dir.create("path")` |
| `import std.path` | `path` | `path.join("a", "b")`, `path.dirname("/a/b")` |
| `import std.json` | `json` | `json.parse(str)`, `json.create_object()` |
| `import std.http` | `http` | `http.get(url)`, `http.server_create(port)` |
| `import std.tcp` | `tcp` | `tcp.connect(host, port)`, `tcp.send(sock, data)` |
| `import std.list` | `list` | `list.new()`, `list.add(l, item)` |
| `import std.map` | `map` | `map.new()`, `map.put(m, key, val)` |
| `import std.math` | `math` | `math.sqrt(x)`, `math.sin(x)` |
| `import std.log` | `log` | `log.info(msg)`, `log.error(msg)` |

---

## Using the Standard Library

Import modules with the `import` statement:

```aether
import std.string       // String functions
import std.file         // File operations
import std.dir          // Directory operations
import std.json         // JSON parsing
import std.http         // HTTP client & server
import std.tcp          // TCP sockets
import std.list         // ArrayList
import std.map          // HashMap
import std.math         // Math functions
import std.log          // Logging
```

Call functions using namespace syntax:

```aether
import std.string
import std.file

main() {
    // String operations
    s = string.new("hello")
    len = string.length(s)
    string.release(s)

    // File operations
    if (file.exists("data.txt") == 1) {
        size = file.size("data.txt")
    }
}
```

Or use `extern` for direct C bindings:

```aether
extern my_c_function(x: int) -> ptr
```

---

## String Library

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

- `string_new(const char* cstr)` - Create from C string
- `string_from_literal(const char* cstr)` - Alias for new
- `string_empty()` - Create empty string
- `string_new_with_length(const char* data, size_t len)` - Create with explicit length

#### String Operations

- `string_concat(AetherString* a, AetherString* b)` - Concatenate strings
- `string_length(AetherString* str)` - Get length
- `string_char_at(AetherString* str, int index)` - Get character
- `string_equals(AetherString* a, AetherString* b)` - Check equality
- `string_compare(AetherString* a, AetherString* b)` - Compare (-1, 0, 1)

#### String Methods

- `string_starts_with()` - Check prefix
- `string_ends_with()` - Check suffix
- `string_contains()` - Search for substring
- `string_index_of()` - Find position
- `string_substring()` - Extract substring
- `string_to_upper()` - Convert to uppercase
- `string_to_lower()` - Convert to lowercase
- `string_trim()` - Remove whitespace

#### Conversion

- `string_to_cstr(AetherString* str)` - Get C string pointer
- `string_from_int(int value)` - Convert int to string
- `string_from_float(float value)` - Convert float to string

#### Memory Management

- `string.new(cstr)` - Allocate a new string (use `string.free` when done)
- `string.free(str)` - Free the string

Use `defer string.free(s)` right after `string.new()` to ensure cleanup at scope exit.

The underlying C implementation also exposes `string.retain()` / `string.release()` for advanced use cases (e.g., sharing ownership across C callbacks), but Aether programs should use `string.free()` directly.

---

## File System Library

Complete filesystem library with file and directory operations.

### Usage

```aether
import std.file
import std.dir

main() {
    if (file.exists("data.txt") == 1) {
        print("File exists!\n")
        size = file.size("data.txt")
        print("Size: ")
        print(size)
        print(" bytes\n")
    }

    dir.create("output")
}
```

### File Operations

- `file.open(path, mode)` - Open a file (returns File*)
- `file.close(file)` - Close a file
- `file.read_all(file)` - Read entire file contents
- `file.write(file, data, length)` - Write data to file
- `file.exists(path)` - Check if file exists (returns 1 or 0)
- `file.size(path)` - Get file size in bytes
- `file.delete(path)` - Delete a file

### Directory Operations

- `dir.exists(path)` - Check if directory exists
- `dir.create(path)` - Create a directory
- `dir.delete(path)` - Delete an empty directory
- `dir.list(path)` - List directory contents (returns DirList*)
- `dir.list_free(list)` - Free directory listing

### Path Utilities

- `path.join(path1, path2)` - Join two path components
- `path.dirname(path)` - Get directory name
- `path.basename(path)` - Get file name
- `path.extension(path)` - Get file extension
- `path.is_absolute(path)` - Check if path is absolute

---

## I/O Library

### Console Output

The primary I/O function in Aether is `print()`, which works like C's `printf`:

```aether
print("Hello, World!\n")
print("Value: %d\n", x)
print("Float: %f\n", pi)
```

### Additional I/O

- `io.print(str)` - Print AetherString
- `io.read_line()` - Read line from stdin
- `io.read_file(path)` - Read entire file
- `io.write_file(path, content)` - Write to file
- `io.file_info(path)` - Get file metadata
- `io.getenv(name)` - Get environment variable
- `io.setenv(name, value)` - Set environment variable

---

## Math Library

### Basic Operations

- `abs_int(x)` - Absolute value (int)
- `abs_float(x)` - Absolute value (float)
- `min_int(a, b)` - Minimum (int)
- `max_int(a, b)` - Maximum (int)
- `min_float(a, b)` - Minimum (float)
- `max_float(a, b)` - Maximum (float)
- `clamp_int(x, min, max)` - Clamp value to range
- `clamp_float(x, min, max)` - Clamp value to range

### Advanced Math

- `math.sqrt(x)` - Square root
- `math.pow(base, exp)` - Power
- `math.sin(x)` - Sine
- `math.cos(x)` - Cosine
- `math.tan(x)` - Tangent
- `math.asin(x)` - Arc sine
- `math.acos(x)` - Arc cosine
- `math.atan(x)` - Arc tangent
- `math.atan2(y, x)` - Two-argument arc tangent
- `math.floor(x)` - Floor
- `math.ceil(x)` - Ceiling
- `math.round(x)` - Round to nearest
- `math.log(x)` - Natural logarithm
- `math.log10(x)` - Base-10 logarithm
- `math.exp(x)` - Exponential

### Random Numbers

- `random_seed(seed)` - Set random seed
- `random_int(min, max)` - Random int in range [min, max]
- `random_float()` - Random float in [0.0, 1.0)

### Constants

- `PI` - 3.14159265358979323846
- `E` - 2.71828182845904523536

---

## JSON Library

### Parsing and Serialization

```aether
import std.json

main() {
    obj = json.create_object()
    arr = json.create_array()
    num = json.create_number(42.5)
    bool_val = json.create_bool(1)
    null_val = json.create_null()

    json.array_add(arr, num)

    type = json.type(num)  // Returns JSON_NUMBER (2)

    value = json.get_number(num)
    is_true = json.get_bool(bool_val)

    json.free(obj)
}
```

### JSON Functions

- `json.parse(str)` - Parse JSON string
- `json.stringify(value)` - Convert to JSON string
- `json.free(value)` - Free JSON value
- `json.create_object()` - Create empty object
- `json.create_array()` - Create empty array
- `json.create_string(str)` - Create string value
- `json.create_number(num)` - Create number value
- `json.create_bool(val)` - Create boolean value
- `json.create_null()` - Create null value
- `json.type(value)` - Get value type
- `json.is_null(value)` - Check if null
- `json.get_number(value)` - Get number
- `json.get_bool(value)` - Get boolean
- `json.get_string(value)` - Get string
- `json.array_add(arr, value)` - Add to array
- `json.array_size(arr)` - Get array size
- `json.array_get(arr, index)` - Get array element
- `json.object_set(obj, key, value)` - Set object property
- `json.object_get(obj, key)` - Get object property

### JSON Type Constants

- `JSON_NULL` = 0
- `JSON_BOOL` = 1
- `JSON_NUMBER` = 2
- `JSON_STRING` = 3
- `JSON_ARRAY` = 4
- `JSON_OBJECT` = 5

---

## Networking Library

### HTTP Client

```aether
import std.http

main() {
    response = http.get("http://example.com")
    if (response != 0) {
        print("Status: ")
        print(response.status)
        print("\n")
        http.response_free(response)
    }
}
```

### HTTP Functions

- `http.get(url)` - HTTP GET request
- `http.post(url, body, content_type)` - HTTP POST request
- `http.put(url, body, content_type)` - HTTP PUT request
- `http.delete(url)` - HTTP DELETE request
- `http.response_free(response)` - Free response

### HTTP Server

```aether
import std.http

main() {
    server = http.server_create(8080)
    http.server_bind(server, "127.0.0.1", 8080)
    http.server_start(server)  // Blocks
    http.server_free(server)
}
```

### Server Functions

- `http.server_create(port)` - Create server
- `http.server_bind(server, host, port)` - Bind to address
- `http.server_start(server)` - Start serving (blocking)
- `http.server_stop(server)` - Stop server
- `http.server_free(server)` - Free server
- `http.server_get(server, path, handler, data)` - Register GET handler
- `http.server_post(server, path, handler, data)` - Register POST handler
- `http.response_json(res, json)` - Send JSON response
- `http.response_set_status(res, code)` - Set status code
- `http.response_set_header(res, name, value)` - Set header

### TCP Sockets

- `tcp.connect(host, port)` - Connect to server
- `tcp.send(sock, data)` - Send data
- `tcp.receive(sock, max_bytes)` - Receive data
- `tcp.close(sock)` - Close socket
- `tcp.listen(port)` - Create listening server socket
- `tcp.accept(server)` - Accept connection
- `tcp.server_close(server)` - Close server socket

---

## Collections Library

### ArrayList

```aether
import std.list

main() {
    mylist = list.new()
    defer list.free(mylist)

    list.add(mylist, some_ptr)
    item = list.get(mylist, 0)
    size = list.size(mylist)
}
```

### ArrayList Functions

- `list.new()` - Create new list
- `list.add(list, item)` - Add item
- `list.get(list, index)` - Get item
- `list.set(list, index, item)` - Set item
- `list.remove(list, index)` - Remove item
- `list.size(list)` - Get size
- `list.clear(list)` - Clear all items
- `list.free(list)` - Free list

### HashMap

```aether
import std.map
import std.string

main() {
    mymap = map.new()
    defer map.free(mymap)

    key = string.new("name")
    defer string.free(key)

    map.put(mymap, key, value_ptr)
    result = map.get(mymap, key)
}
```

### HashMap Functions

- `map.new()` - Create new map
- `map.put(map, key, value)` - Put key-value pair
- `map.get(map, key)` - Get value by key
- `map.remove(map, key)` - Remove key
- `map.has(map, key)` - Check if key exists
- `map.size(map)` - Get number of entries
- `map.clear(map)` - Clear all entries
- `map.free(map)` - Free map

---

## Logging Library

```aether
import std.log

main() {
    log.init("app.log", LOG_DEBUG)
    log.debug("Starting application")
    log.info("Processing...")
    log.warn("Resource low")
    log.error("Something failed")
    log.shutdown()
}
```

### Logging Functions

- `log.init(filename, level)` - Initialize logging
- `log.shutdown()` - Shutdown logging
- `log.set_level(level)` - Set minimum level
- `log.debug(msg)` - Debug message
- `log.info(msg)` - Info message
- `log.warn(msg)` - Warning message
- `log.error(msg)` - Error message
- `log.get_stats()` - Get logging statistics

### Log Levels

- `LOG_DEBUG` = 0
- `LOG_INFO` = 1
- `LOG_WARN` = 2
- `LOG_ERROR` = 3

---

## Concurrency Functions

### Actor Management

- `spawn(ActorName())` - Create a new actor instance

### Synchronization

- `wait_for_idle()` - Block until all actors finish processing
- `sleep(milliseconds)` - Pause execution

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

    wait_for_idle()

    print("Completed: ")
    print(w.completed)
    print("\n")
}
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
- Manage string references with `string.retain()`/`string.release()`
- Actors are freed when no longer referenced

---

## Best Practices

1. **Use `import` for stdlib** - Cleaner than `extern`
2. **Use `print()` for output** - Simple and reliable
3. **Free resources** - Use `defer type.free(x)` after allocation, or explicit `.free()` calls
4. **Enable bounds checking in debug** - Catches array errors
5. **Use actors for concurrency** - Safer than manual threading

---

## Example: Complete Program

```aether
import std.file

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

    if (file.exists("README.md") == 1) {
        print("README.md found!\n")
    }

    counter = spawn(Counter())
    counter ! Increment { amount: 1 }
    counter ! Increment { amount: 1 }

    wait_for_idle()

    print("Final count: ")
    print(counter.count)
    print("\n")
}
```
