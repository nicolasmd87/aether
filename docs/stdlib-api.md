# Aether Standard Library Guide

## Overview

Aether provides a standard library for strings, I/O, math, file system, networking, and actor concurrency. The library is automatically linked when you compile Aether programs.

> **Go-style result types.** Every stdlib function that can fail returns a `(value, err)` tuple. Check `err` first, then use `value`. The raw C-style externs are preserved under a `_raw` suffix for advanced callers who need direct access to the underlying pointer or int:
>
> ```aether
> body, err = http.get("http://example.com")
> if err != "" {
>     println("failed: ${err}")
>     return
> }
> println(body)
> ```
>
> See the [error handling example](../examples/basics/error-handling.ae) for the user-function pattern, and `examples/stdlib/http-client.ae` for the stdlib pattern.

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
| `import std.log` | `log` | `log.init(file, level)`, `log.write(level, msg)` |
| `import std.io` | `io` | `io.print(str)`, `io.read_file(path)`, `io.getenv(name)` |
| `import std.os` | `os` | `os.system(cmd)`, `os.exec(cmd)`, `os.getenv(name)`, `os.argv0()`, `os_execv(prog, argv)` |

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
import std.io           // Console I/O, environment variables
import std.os           // Shell execution, environment variables
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
    unsigned int magic;    // Always 0xAE57C0DE — enables runtime type detection
    int ref_count;
    size_t length;
    size_t capacity;
    char* data;
} AetherString;
```

> **Note:** All `std.string` functions accept both plain `char*` strings and managed `AetherString*` transparently. The `magic` field is used internally to distinguish between the two at runtime.

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

- `string.to_cstr(str)` - Get C string pointer
- `string.from_int(value)` - Convert int to string
- `string.from_float(value)` - Convert float to string

#### Parsing (Go-style)

All parsers return `(value, err)` tuples. Empty `err` means success.

```aether
n, err = string.to_int("42")
if err != "" { println("bad: ${err}"); return }
println(n)
```

- `string.to_int(s)` → `(int, string)` - Parse base-10 integer
- `string.to_long(s)` → `(long, string)` - Parse 64-bit integer
- `string.to_float(s)` → `(float, string)` - Parse float
- `string.to_double(s)` → `(float, string)` - Parse double

Raw out-parameter externs are preserved as `string_to_int_raw`, `string_to_long_raw`, `string_to_float_raw`, `string_to_double_raw` for code that needs to distinguish zero from parse failure without a tuple.

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
    // Read a file in one call (opens, reads, closes)
    content, err = file.read("data.txt")
    if err != "" {
        println("cannot read: ${err}")
        return
    }
    println(content)

    // Write a file
    werr = file.write("output.txt", "hello")
    if werr != "" {
        println("cannot write: ${werr}")
        return
    }

    // Get size
    size, serr = file.size("data.txt")
    if serr == "" {
        println("size: ${size} bytes")
    }

    // Create a directory
    derr = dir.create("output")
    if derr != "" {
        println("cannot mkdir: ${derr}")
    }
}
```

### File Operations (Go-style)

- `file.read(path)` → `(string, string)` - Read entire file (opens, reads, closes)
- `file.write(path, content)` → `string` - Write content, return error string
- `file.open(path, mode)` → `(ptr, string)` - Low-level open (caller must `file.close`)
- `file.close(handle)` - Close a file handle
- `file.size(path)` → `(int, string)` - Get file size in bytes
- `file.delete(path)` → `string` - Delete a file, return error string
- `file.exists(path)` → `int` - 1 if exists, 0 otherwise (infallible predicate)

Raw externs: `file_open_raw`, `file_read_all_raw`, `file_write_raw`, `file_delete_raw`, `file_size_raw`.

### Directory Operations (Go-style)

- `dir.create(path)` → `string` - Create directory, return error string
- `dir.delete(path)` → `string` - Delete empty directory, return error string
- `dir.list(path)` → `(ptr, string)` - List contents (caller must `dir.list_free`)
- `dir.exists(path)` → `int` - 1 if exists, 0 otherwise
- `dir.list_count(list)` / `dir.list_get(list, i)` / `dir.list_free(list)` - DirList accessors

Raw externs: `dir_create_raw`, `dir_delete_raw`, `dir_list_raw`.

### Path Utilities

- `path.join(path1, path2)` - Join two path components
- `path.dirname(path)` - Get directory name
- `path.basename(path)` - Get file name
- `path.extension(path)` - Get file extension
- `path.is_absolute(path)` - Check if path is absolute

---

## I/O Library

### Console Output

The primary I/O functions in Aether are `print()` and `println()`:

```aether
print("Hello, World!\n")
println("Hello, World!")       // same, with automatic newline
println("Value: ${x}")         // string interpolation
println("Float: ${pi}")
```

### Console Output (infallible)

- `io.print(str)` - Print string
- `io.print_line(str)` - Print string with newline
- `io.print_int(value)` - Print integer
- `io.print_float(value)` - Print float

### File I/O (Go-style)

All operations that can fail return an error string ("" on success).

```aether
content, err = io.read_file("data.txt")
if err != "" { println("failed: ${err}"); return }

werr = io.write_file("output.txt", "hello")
```

- `io.read_file(path)` → `(string, string)` - Read entire file
- `io.write_file(path, content)` → `string` - Write (overwrites)
- `io.append_file(path, content)` → `string` - Append to file
- `io.delete_file(path)` → `string` - Delete file
- `io.file_info(path)` → `(ptr, string)` - Get file metadata (caller must `io.file_info_free`)
- `io.file_exists(path)` → `int` - 1 if exists, 0 otherwise

### Environment variables

- `io.getenv(name)` → `string` - Returns the value, or null if unset (infallible)
- `io.setenv(name, value)` → `string` - Set env var, return error string
- `io.unsetenv(name)` → `string` - Unset env var, return error string

Raw externs: `io_read_file_raw`, `io_write_file_raw`, `io_append_file_raw`, `io_delete_file_raw`, `io_file_info_raw`, `io_setenv_raw`, `io_unsetenv_raw`.

---

## OS / Process Library

### Shell execution

- `os.system(cmd)` → `int` — Run a shell command, return exit code
- `os.exec(cmd)` → `(string, string)` — Run a command and capture stdout; returns `(output, err)` tuple
- `os.getenv(name)` → `string` — Read environment variable; returns null if unset

### Argv discovery

- `aether_args_count()` → `int` — Number of command-line arguments
- `aether_args_get(index)` → `string` — Get the i-th argument; returns null if out of range
- `aether_argv0()` → `string` — Path the OS launched the current process with (argv[0]); returns null before `aether_args_init` has run
- `os.argv0()` → `string` — Convenience wrapper around `aether_argv0()` that returns `""` instead of null

Typical use: a tool that needs to find its own binary (to locate sibling helpers next to itself, re-exec with different flags, or print a self-path in a diagnostic) can call `os.argv0()` and skip the argv-index bookkeeping.

### Process replacement

- `os_execv(prog, argv_list)` → `int` — Replace the current process image with `prog`, passing an explicit argv list. `argv_list` is a `list<ptr>` of C strings (element 0 is argv[0] for the new program). On success this call **never returns**; on failure it returns `-1` and the current process keeps running. `prog` is looked up on `PATH` if it does not contain a slash. Not available on Windows — returns `-1`.

Paired with `os_run` / `os_run_capture` (see PR #148), this gives Aether programs a full argv-based process-launch surface with no shell in the middle, so paths with spaces, quotes, or `$`-signs are safe. Stdio is flushed before the exec, so pre-exec diagnostics are not lost.

Example:

```aether
import std.os
import std.list

main() {
    argv = list.new()
    _e1 = list.add(argv, "echo")
    _e2 = list.add(argv, "from")
    _e3 = list.add(argv, os.argv0())
    rc = os_execv("/bin/echo", argv)
    // Only reached if exec failed.
    println("exec failed: ${rc}")
    exit(rc)
}
```

---

## Math Library

### Basic Operations

- `math.abs_int(x)` - Absolute value (int)
- `math.abs_float(x)` - Absolute value (float)
- `math.min_int(a, b)` - Minimum (int)
- `math.max_int(a, b)` - Maximum (int)
- `math.min_float(a, b)` - Minimum (float)
- `math.max_float(a, b)` - Maximum (float)
- `math.clamp_int(x, min, max)` - Clamp value to range
- `math.clamp_float(x, min, max)` - Clamp value to range

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

- `math.random_seed(seed)` - Set random seed
- `math.random_int(min, max)` - Random int in range [min, max]
- `math.random_float()` - Random float in [0.0, 1.0)

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

- `json.parse(str)` → `(ptr, string)` - Parse JSON, returns `(value, err)` tuple. Caller owns the returned value and must `json.free` it.
- `json.stringify(value)` → `string` - Convert to JSON string (infallible)
- `json.free(value)` - Free JSON value

Raw extern: `json_parse_raw`.
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

### HTTP Client (Go-style)

```aether
import std.http

main() {
    body, err = http.get("http://example.com")
    if err != "" {
        println("failed: ${err}")
        return
    }
    println(body)
}
```

See `examples/stdlib/http-client.ae` for a runnable version.

### HTTP Client Functions

- `http.get(url)` → `(string, string)` - HTTP GET, auto-frees response
- `http.post(url, body, content_type)` → `(string, string)` - HTTP POST
- `http.put(url, body, content_type)` → `(string, string)` - HTTP PUT
- `http.delete(url)` → `(string, string)` - HTTP DELETE

All wrappers return `("", err)` for transport failures and for any non-2xx HTTP status. If you need status codes or headers, use the raw extern + accessor pattern:

```aether
response = http.get_raw(url)
status = http.response_status(response)
body = http.response_body(response)
http.response_free(response)
```

Raw externs: `http_get_raw`, `http_post_raw`, `http_put_raw`, `http_delete_raw`.

Response accessors (used with the raw API):

- `http.response_status(response)` - Read HTTP status code (0 on transport failure)
- `http.response_body(response)` - Read response body as string
- `http.response_headers(response)` - Read response headers as string
- `http.response_error(response)` - Read transport error string
- `http.response_ok(response)` - 1 if transport succeeded AND status is 2xx, else 0
- `http.response_free(response)` - Free response

### HTTP Server

```aether
import std.http

main() {
    server = http.server_create(8080)

    berr = http.server_bind(server, "127.0.0.1", 8080)
    if berr != "" {
        println("bind failed: ${berr}")
        return
    }

    serr = http.server_start(server)  // Blocks
    if serr != "" {
        println("start failed: ${serr}")
    }
    http.server_free(server)
}
```

### Server Functions

- `http.server_create(port)` - Create server (never fails)
- `http.server_bind(server, host, port)` → `string` - Bind to address, return error string
- `http.server_start(server)` → `string` - Start serving (blocking), return error string
- `http.server_stop(server)` - Stop server
- `http.server_free(server)` - Free server
- `http.server_get(server, path, handler, data)` - Register GET handler
- `http.server_post(server, path, handler, data)` - Register POST handler
- `http.response_json(res, json)` - Send JSON response
- `http.response_set_status(res, code)` - Set status code
- `http.response_set_header(res, name, value)` - Set header

Raw externs: `http_server_bind_raw`, `http_server_start_raw`.

### TCP Sockets (Go-style)

> Note: `send` and `receive` are reserved actor keywords in Aether, so
> the TCP wrappers use `write`/`read` instead. The raw externs retain
> the `send_raw`/`receive_raw` naming.

- `tcp.connect(host, port)` → `(ptr, string)` - Connect, return `(socket, err)`
- `tcp.write(sock, data)` → `(int, string)` - Write, return `(bytes, err)`
- `tcp.read(sock, max_bytes)` → `(string, string)` - Read, return `(data, err)`
- `tcp.listen(port)` → `(ptr, string)` - Create listening socket
- `tcp.accept(server)` → `(ptr, string)` - Accept connection
- `tcp.close(sock)` - Close socket (infallible)
- `tcp.server_close(server)` - Close server socket

Raw externs: `tcp_connect_raw`, `tcp_send_raw`, `tcp_receive_raw`, `tcp_listen_raw`, `tcp_accept_raw`.

### Reactor-Pattern Async I/O (`await_io`)

Aether's scheduler has a per-core I/O reactor (epoll on Linux, kqueue
on macOS/BSD, poll() elsewhere) that can suspend an actor on a file
descriptor without blocking any OS thread. When the fd becomes ready,
the scheduler delivers an `IoReady` message to the actor's mailbox
and resumes it on any available core.

```aether
import std.net

message IoReady { fd: int, events: int }
message Connection { fd: int }

actor Worker {
    receive {
        Connection(fd) -> {
            req = ae_http_recv(fd)
            http_response_json(res, "{\"hello\":\"world\"}")
            net.await_io(fd)   // suspends — zero CPU until data arrives
        }
        IoReady(fd, events) -> {
            // Resumed here when fd is readable again
            req = ae_http_recv(fd)
            http_response_json(res, "{\"hello\":\"world\"}")
            net.await_io(fd)
        }
    }
}
```

**The `IoReady` message name is reserved.** The Aether message
registry assigns it the ID that the runtime scheduler uses for I/O
readiness notifications, so any actor that defines `message IoReady {
fd: int, events: int }` will receive scheduler-delivered events in
that arm.

Functions:

- `net.await_io(fd)` → `string` — Register `fd` with the current
  core's I/O poller and mark the calling actor as waiting. Returns
  `""` on success, error string otherwise (invalid fd, no active
  actor context, or scheduler refused the registration). One-shot:
  the fd is automatically unregistered after the `IoReady` delivery.
- `net.ae_io_cancel(fd)` — Abandon a prior `await_io` without waiting
  for the message. Rare; the one-shot policy makes this unnecessary
  in most flows.

Performance note: PR #140 demonstrated the raw reactor pattern
delivering a 5x throughput improvement (45K → 264K req/s) on the HTTP
benchmark versus a blocking keep-alive worker. `await_io` is the
Aether-language surface over that same machinery.

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

- `list.new()` - Create new list (never fails)
- `list.add(list, item)` → `string` - Append item, return error string (non-empty on resize/OOM)
- `list.get(list, index)` - Get item (returns null for out-of-bounds)
- `list.set(list, index, item)` - Set item at index
- `list.remove(list, index)` - Remove item at index
- `list.size(list)` - Get size
- `list.clear(list)` - Clear all items
- `list.free(list)` - Free list

Raw extern: `list_add_raw` (returns 1/0).

### HashMap

```aether
import std.map

main() {
    mymap = map.new()
    defer map.free(mymap)

    map.put(mymap, "name", some_ptr)
    result = map.get(mymap, "name")
    exists = map.has(mymap, "name")
}
```

### HashMap Functions

- `map.new()` - Create new map (never fails)
- `map.put(map, key, value)` → `string` - Put key-value pair, return error string (non-empty on resize/OOM)
- `map.get(map, key)` - Get value by key (returns null if missing)
- `map.remove(map, key)` - Remove key
- `map.has(map, key)` - Check if key exists
- `map.size(map)` - Get number of entries
- `map.clear(map)` - Clear all entries
- `map.free(map)` - Free map

Raw extern: `map_put_raw` (returns 1/0).

---

## Logging Library

```aether
import std.log

main() {
    err = log.init("app.log", 0)  // 0 = DEBUG level
    if err != "" {
        println("cannot open log file: ${err}")
        // falls back to stderr automatically
    }

    log.write(0, "Debug message")
    log.write(1, "Info message")
    log.write(2, "Warning message")
    log.write(3, "Error message")

    log.print_stats()
    log.shutdown()
}
```

### Logging Functions

- `log.init(filename, level)` → `string` - Initialize logging, return error string if the log file could not be opened (logging still falls back to stderr)
- `log.shutdown()` - Shutdown logging
- `log.write(level, message)` - Write a log message at the given level
- `log.set_level(level)` - Set minimum level
- `log.set_colors(enabled)` - Enable/disable colored output (1/0)
- `log.set_timestamps(enabled)` - Enable/disable timestamps (1/0)
- `log.get_stats()` - Get logging statistics
- `log.print_stats()` - Print logging statistics

Raw extern: `log_init_raw` (returns 1/0).

### Log Levels

- `0` = DEBUG
- `1` = INFO
- `2` = WARN
- `3` = ERROR
- `4` = FATAL

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

Aether uses **manual memory management** with `defer` as the primary tool.

### defer

Use `defer` immediately after allocation to ensure cleanup at scope exit:

```aether
import std.list
import std.string

main() {
    mylist = list.new()
    defer list.free(mylist)

    s = string.new("hello")
    defer string.free(s)

    // ... use mylist and s ...
    // Automatically freed when scope exits
}
```

### Guidelines

- **`defer type.free(x)`** — primary cleanup pattern for all allocations
- **Stack allocations** — freed automatically (no `defer` needed)
- **Actors** — managed by the runtime
- **Managed strings** — reference-counted internally; use `string.free()` (alias for `string.release()`)
- **`string.retain(str)`** — advanced: increment reference count when sharing ownership across C callbacks

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
