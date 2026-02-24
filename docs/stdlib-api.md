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

- `string_new(const char* cstr)` - Allocate a new string (use `string_free` when done)
- `string_free(AetherString* str)` - Free the string

Use `defer string_free(s)` right after `string_new()` to ensure cleanup at scope exit. In opt-in auto mode (`--auto-free`), `string_free` is injected automatically.

The underlying C implementation also exposes `string_retain` / `string_release` for advanced use cases (e.g., sharing ownership across C callbacks), but Aether programs should use `string_free` directly.

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

- `file_open(path, mode)` - Open a file (returns File*)
- `file_close(file)` - Close a file
- `file_read_all(file)` - Read entire file contents
- `file_write(file, data, length)` - Write data to file
- `file_exists(path)` - Check if file exists (returns 1 or 0)
- `file_size(path)` - Get file size in bytes
- `file_delete(path)` - Delete a file

### Directory Operations

- `dir_exists(path)` - Check if directory exists
- `dir_create(path)` - Create a directory
- `dir_delete(path)` - Delete an empty directory
- `dir_list(path)` - List directory contents (returns DirList*)
- `dir_list_free(list)` - Free directory listing

### Path Utilities

- `path_join(path1, path2)` - Join two path components
- `path_dirname(path)` - Get directory name
- `path_basename(path)` - Get file name
- `path_extension(path)` - Get file extension
- `path_is_absolute(path)` - Check if path is absolute

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

- `io_print(str)` - Print AetherString
- `io_read_line()` - Read line from stdin
- `io_read_file(path)` - Read entire file
- `io_write_file(path, content)` - Write to file
- `io_file_info(path)` - Get file metadata
- `io_getenv(name)` - Get environment variable
- `io_setenv(name, value)` - Set environment variable

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

- `sqrt(x)` - Square root
- `pow(base, exp)` - Power
- `sin(x)` - Sine
- `cos(x)` - Cosine
- `tan(x)` - Tangent
- `asin(x)` - Arc sine
- `acos(x)` - Arc cosine
- `atan(x)` - Arc tangent
- `atan2(y, x)` - Two-argument arc tangent
- `floor(x)` - Floor
- `ceil(x)` - Ceiling
- `round(x)` - Round to nearest
- `log(x)` - Natural logarithm
- `log10(x)` - Base-10 logarithm
- `exp(x)` - Exponential

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
    // Create JSON values
    obj = json_create_object()
    arr = json_create_array()
    num = json_create_number(42.5)
    bool_val = json_create_bool(1)
    null_val = json_create_null()

    // Add to array
    json_array_add(arr, num)

    // Check types
    type = json_type(num)  // Returns JSON_NUMBER (2)

    // Get values
    value = json_get_number(num)
    is_true = json_get_bool(bool_val)

    // Cleanup
    json_free(obj)
}
```

### JSON Functions

- `json_parse(str)` - Parse JSON string
- `json_stringify(value)` - Convert to JSON string
- `json_free(value)` - Free JSON value
- `json_create_object()` - Create empty object
- `json_create_array()` - Create empty array
- `json_create_string(str)` - Create string value
- `json_create_number(num)` - Create number value
- `json_create_bool(val)` - Create boolean value
- `json_create_null()` - Create null value
- `json_type(value)` - Get value type
- `json_is_null(value)` - Check if null
- `json_get_number(value)` - Get number
- `json_get_bool(value)` - Get boolean
- `json_get_string(value)` - Get string
- `json_array_add(arr, value)` - Add to array
- `json_array_size(arr)` - Get array size
- `json_array_get(arr, index)` - Get array element
- `json_object_set(obj, key, value)` - Set object property
- `json_object_get(obj, key)` - Get object property

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
import std.net

main() {
    response = http_get("http://example.com")
    if (response != 0) {
        print("Status: ")
        print(response.status)
        print("\n")
        http_response_free(response)
    }
}
```

### HTTP Functions

- `http_get(url)` - HTTP GET request
- `http_post(url, body, content_type)` - HTTP POST request
- `http_put(url, body, content_type)` - HTTP PUT request
- `http_delete(url)` - HTTP DELETE request
- `http_response_free(response)` - Free response

### HTTP Server

```aether
import std.net

main() {
    server = http_server_create(8080)
    http_server_bind(server, "127.0.0.1", 8080)
    http_server_start(server)  // Blocks
    http_server_free(server)
}
```

### Server Functions

- `http_server_create(port)` - Create server
- `http_server_bind(server, host, port)` - Bind to address
- `http_server_start(server)` - Start serving (blocking)
- `http_server_stop(server)` - Stop server
- `http_server_free(server)` - Free server
- `http_server_get(server, path, handler, data)` - Register GET handler
- `http_server_post(server, path, handler, data)` - Register POST handler
- `http_response_json(res, json)` - Send JSON response
- `http_response_set_status(res, code)` - Set status code
- `http_response_set_header(res, name, value)` - Set header

### TCP Sockets

- `tcp_connect(host, port)` - Connect to server
- `tcp_send(sock, data)` - Send data
- `tcp_receive(sock, max_bytes)` - Receive data
- `tcp_close(sock)` - Close socket
- `tcp_listen(port)` - Create listening server socket
- `tcp_accept(server)` - Accept connection
- `tcp_server_close(server)` - Close server socket

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

- `log_init(filename, level)` - Initialize logging
- `log_shutdown()` - Shutdown logging
- `log_set_level(level)` - Set minimum level
- `log_debug(msg)` - Debug message
- `log_info(msg)` - Info message
- `log_warn(msg)` - Warning message
- `log_error(msg)` - Error message
- `log_get_stats()` - Get logging statistics

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
- Manage string references with `string_retain`/`string_release`
- Actors are freed when no longer referenced

---

## Best Practices

1. **Use `import` for stdlib** - Cleaner than `extern`
2. **Use `print()` for output** - Simple and reliable
3. **Free resources** - Use `*_free()` functions when done
4. **Enable bounds checking in debug** - Catches array errors
5. **Use actors for concurrency** - Safer than manual threading

---

## Example: Complete Program

```aether
import std.fs

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

    // Check if a file exists
    if (file_exists("README.md") == 1) {
        print("README.md found!\n")
    }

    // Use actors
    counter = spawn(Counter())
    counter ! Increment { amount: 1 }
    counter ! Increment { amount: 1 }

    wait_for_idle()

    print("Final count: ")
    print(counter.count)
    print("\n")
}
```
