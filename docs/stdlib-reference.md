# Aether Standard Library Reference

Complete reference for Aether's standard library modules.

> **Note:** The standard library is under active development. The runtime provides C implementations in `std/` that are linked via the `ae` tool.

## Using the Standard Library

Import modules with the `import` statement and call functions with namespace syntax:

```aether
import std.string
import std.file

main() {
    // Namespace-style calls
    s = string.new("hello");
    len = string.length(s);

    if (file.exists("data.txt") == 1) {
        print("File exists!\n");
    }

    string.release(s);
}
```

---

## Collections

### List (`std.list`)

Dynamic array (ArrayList) implementation.

```aether
import std.list

main() {
    mylist = list.new();
    defer list.free(mylist);

    list.add(mylist, item1);
    list.add(mylist, item2);

    item = list.get(mylist, 0);
    size = list.size(mylist);

    list.remove(mylist, 0);
    list.clear(mylist);
}
```

**Functions:**
- `list.new()` - Create new list
- `list.add(list, item)` - Append item
- `list.get(list, index)` - Get item at index
- `list.set(list, index, item)` - Set item at index
- `list.remove(list, index)` - Remove item at index
- `list.size(list)` - Get number of elements
- `list.clear(list)` - Remove all elements
- `list.free(list)` - Free list memory

### Map (`std.map`)

Hash map implementation.

```aether
import std.map
import std.string

main() {
    mymap = map.new();
    defer map.free(mymap);

    // Keys are raw string literals (const char*),
    // values are pointers (AetherString* here)
    val = string.new("Aether");
    defer string.release(val);

    map.put(mymap, "name", val);
    result = map.get(mymap, "name");
    exists = map.has(mymap, "name");

    map.remove(mymap, "name");
    size = map.size(mymap);

    map.clear(mymap);
}
```

**Functions:**
- `map.new()` - Create new map
- `map.put(map, key, value)` - Insert or update key-value pair
- `map.get(map, key)` - Get value by key
- `map.has(map, key)` - Check if key exists
- `map.remove(map, key)` - Remove key-value pair
- `map.size(map)` - Get number of entries
- `map.clear(map)` - Remove all entries
- `map.free(map)` - Free map memory

---

## Strings (`std.string`)

Reference-counted strings with comprehensive operations.

### String Types: `string` vs Managed Strings

Aether has two string representations:

| | `string` (raw) | Managed (`ptr` via `std.string`) |
|---|---|---|
| **C type** | `const char*` | `AetherString*` |
| **Allocation** | Static (literals) or manual | Heap (reference-counted) |
| **Memory** | None needed | `string.release()` or `defer string.free()` |
| **Knows length?** | No (`O(n)` via `strlen`) | Yes (`O(1)`) |
| **Manipulation** | Not possible | Full API (`trim`, `split`, `concat`, etc.) |

**`string`** — raw C string. String literals like `"hello"` are this type. Use for message fields, constants, and passing text to extern functions.

**Managed strings** — heap-allocated objects returned by `std.string` functions (`string.new`, `string.trim`, `string.split`, etc.). Typed as `ptr` in Aether code. Required for any string manipulation.

**Converting between them:**

```aether
import std.string

main() {
    // Raw literal → managed: use string.new()
    raw = "  hello  "
    managed = string.new(raw)
    trimmed = string.trim(managed)

    // Managed → raw: use string.to_cstr()
    print(string.to_cstr(trimmed))

    defer string.free(managed)
    defer string.free(trimmed)
}
```

**Best practices:**
- Use `string` for message fields — keeps payloads simple
- Use managed strings when you need to manipulate text (trim, split, concat)
- Always `defer string.free()` immediately after creating a managed string
- Use `string.to_cstr()` when passing managed strings to `print` or message fields

### Usage Examples

```aether
import std.string

main() {
    // Create strings
    s = string.new("Hello");
    s2 = string.new(" World");

    // Operations
    len = string.length(s);
    combined = string.concat(s, s2);

    // String methods
    upper = string.to_upper(s);
    lower = string.to_lower(s);
    trimmed = string.trim(s);

    // Searching
    contains = string.contains(s, "ell");
    index = string.index_of(s, "l");
    starts = string.starts_with(s, "He");
    ends = string.ends_with(s, "lo");

    // Substrings
    sub = string.substring(s, 0, 3);  // "Hel"

    // Splitting
    csv = string.new("a,b,c");
    parts = string.split(csv, ",");
    count = string.array_size(parts);  // 3
    first = string.array_get(parts, 0); // "a"
    string.array_free(parts);
    string.release(csv);

    // Conversion
    n = string.from_int(42);       // "42"
    f = string.from_float(3.14);   // "3.14"
    cstr = string.to_cstr(s);     // raw C string pointer

    // Memory management
    string.release(s);
    string.release(s2);
}
```

**Creation:**
- `string.new(cstr)` - Create from C string
- `string.from_literal(cstr)` - Create from string literal (alias for `new`)
- `string.from_cstr(cstr)` - Create from C string (alias for `new`)
- `string.empty()` - Create empty string

**Operations:**
- `string.length(str)` - Get length
- `string.concat(a, b)` - Concatenate two strings (returns new string)
- `string.char_at(str, index)` - Get character at index
- `string.equals(a, b)` - Check equality (returns 1/0)
- `string.compare(a, b)` - Lexicographic compare (returns -1, 0, 1)

**Searching:**
- `string.starts_with(str, prefix)` - Check prefix (returns 1/0)
- `string.ends_with(str, suffix)` - Check suffix (returns 1/0)
- `string.contains(str, sub)` - Check if substring exists (returns 1/0)
- `string.index_of(str, sub)` - Find position of substring (returns -1 if not found)

**Transformation:**
- `string.substring(str, start, end)` - Extract substring
- `string.to_upper(str)` - Convert to uppercase (returns new string)
- `string.to_lower(str)` - Convert to lowercase (returns new string)
- `string.trim(str)` - Remove leading/trailing whitespace

**Splitting:**
- `string.split(str, delimiter)` - Split string by delimiter (returns array)
- `string.array_size(arr)` - Get number of parts in split result
- `string.array_get(arr, index)` - Get string at index from split result
- `string.array_free(arr)` - Free split result array

**Conversion:**
- `string.to_cstr(str)` - Get raw C string pointer
- `string.from_int(value)` - Create string from integer
- `string.from_float(value)` - Create string from float

**Parsing:**
- `string.to_int(str, out_ptr)` - Parse integer (returns 1 on success, 0 on failure)
- `string.to_float(str, out_ptr)` - Parse float (returns 1 on success, 0 on failure)

**Memory:**
- `string.retain(str)` - Increment reference count
- `string.release(str)` - Decrement reference count (frees when zero)
- `string.free(str)` - Alias for `release`

---

## File System

### Files (`std.file`)

```aether
import std.file

main() {
    // Check existence
    if (file.exists("data.txt") == 1) {
        size = file.size("data.txt");
        print("Size: ");
        print(size);
        print(" bytes\n");
    }

    // Open and read
    f = file.open("data.txt", "r");
    if (f != 0) {
        content = file.read_all(f);
        file.close(f);
    }

    // Write
    f = file.open("output.txt", "w");
    file.write(f, "Hello", 5);
    file.close(f);

    // Delete
    file.delete("temp.txt");
}
```

**Functions:**
- `file.open(path, mode)` - Open file (returns handle)
- `file.close(file)` - Close file
- `file.read_all(file)` - Read entire contents
- `file.write(file, data, len)` - Write data
- `file.exists(path)` - Check if file exists (returns 1/0)
- `file.size(path)` - Get file size in bytes
- `file.delete(path)` - Delete file

> **Note:** `file.read_all()` returns a managed string — use `defer string.release(content)` to free it.

### Directories (`std.dir`)

```aether
import std.dir

main() {
    // Check and create
    if dir.exists("output") == 0 {
        dir.create("output")
    }

    // List contents
    list = dir.list(".")
    // Process list...
    dir.list_free(list)

    // Delete
    dir.delete("temp_dir")
}
```

**Functions:**
- `dir.exists(path)` - Check if directory exists (returns 1/0)
- `dir.create(path)` - Create directory
- `dir.delete(path)` - Delete empty directory
- `dir.list(path)` - List directory contents
- `dir.list_free(list)` - Free directory listing

### Paths (`std.path`)

Path functions return managed strings — use `defer string.release()` or `defer string.free()` to free them.

```aether
import std.path
import std.string

main() {
    joined = path.join("dir", "file.txt")
    defer string.release(joined)
    dirname = path.dirname("/a/b/file.txt")   // "/a/b"
    defer string.release(dirname)
    basename = path.basename("/a/b/file.txt")  // "file.txt"
    defer string.release(basename)
    ext = path.extension("file.txt")           // ".txt" (includes dot)
    defer string.release(ext)
    is_abs = path.is_absolute("/usr/bin")      // 1
}
```

**Functions:**
- `path.join(a, b)` - Join path components (returns managed string)
- `path.dirname(path)` - Get directory name (returns managed string)
- `path.basename(path)` - Get file name (returns managed string)
- `path.extension(path)` - Get file extension including dot (returns managed string)
- `path.is_absolute(path)` - Check if absolute path (returns 1/0)

---

## JSON (`std.json`)

JSON parsing, creation, and serialization.

```aether
import std.json
import std.string

main() {
    // Parse JSON string
    data = json.parse("{\"name\": \"Aether\", \"version\": 1}")
    name = json.object_get(data, "name")
    println(string.to_cstr(json.get_string(name)))  // "Aether"

    // Create values
    obj = json.create_object()
    json.object_set(obj, "key", json.create_string("value"))
    json.object_set(obj, "count", json.create_number(42.0))

    // Arrays
    arr = json.create_array()
    json.array_add(arr, json.create_number(1.0))
    json.array_add(arr, json.create_number(2.0))
    size = json.array_size(arr)

    // Serialize to string
    output = json.stringify(obj)
    println(string.to_cstr(output))
    string.release(output)

    // Type checking
    type = json.type(json.create_number(3.0))  // 2 = JSON_NUMBER

    // Cleanup
    json.free(data)
    json.free(obj)
    json.free(arr)
}
```

**JSON Type Constants:**
- `0` = NULL, `1` = BOOL, `2` = NUMBER, `3` = STRING, `4` = ARRAY, `5` = OBJECT

**Parsing / Serialization:**
- `json.parse(json_str)` - Parse JSON string into value tree
- `json.stringify(value)` - Serialize to JSON string (returns managed string, call `string.release()`)
- `json.free(value)` - Free a JSON value tree

**Type Checking:**
- `json.type(value)` - Get type constant (0-5)
- `json.is_null(value)` - Check if null (returns 1/0)

**Value Getters:**
- `json.get_number(value)` - Get float value
- `json.get_int(value)` - Get integer value
- `json.get_bool(value)` - Get boolean (1/0)
- `json.get_string(value)` - Get string (returns managed string)

**Object Operations:**
- `json.object_get(obj, key)` - Get value by key (key is a raw string)
- `json.object_set(obj, key, value)` - Set key-value pair
- `json.object_has(obj, key)` - Check if key exists (returns 1/0)

**Array Operations:**
- `json.array_get(arr, index)` - Get value at index
- `json.array_add(arr, value)` - Append value
- `json.array_size(arr)` - Get array length

**Value Creation:**
- `json.create_null()`, `json.create_bool(value)`, `json.create_number(value)`
- `json.create_string(value)`, `json.create_array()`, `json.create_object()`

---

## Networking

### HTTP (`std.net`)

> **Note:** HTTP and TCP are both in `std.net`. Use `import std.net` and call with `http.*` or `tcp.*` prefix.

```aether
import std.net

main() {
    // HTTP Client
    response = http.get("http://example.com")
    if response != 0 {
        http.response_free(response)
    }

    // HTTP Server
    server = http.server_create(8080)
    http.server_bind(server, "127.0.0.1", 8080)
    http.server_start(server)
    http.server_free(server)
}
```

**Client:**
- `http.get(url)` - HTTP GET request
- `http.post(url, body, content_type)` - HTTP POST
- `http.put(url, body, content_type)` - HTTP PUT
- `http.delete(url)` - HTTP DELETE
- `http.response_free(response)` - Free response

**Server Lifecycle:**
- `http.server_create(port)` - Create server
- `http.server_bind(server, host, port)` - Bind to address
- `http.server_start(server)` - Start serving (blocking)
- `http.server_stop(server)` - Stop server
- `http.server_free(server)` - Free server

**Server Routing:**
- `http.server_get(server, path, handler, user_data)` - Register GET route
- `http.server_post(server, path, handler, user_data)` - Register POST route
- `http.server_put(server, path, handler, user_data)` - Register PUT route
- `http.server_delete(server, path, handler, user_data)` - Register DELETE route
- `http.server_use_middleware(server, middleware, user_data)` - Add middleware

**Request Accessors:**
- `http.get_header(req, name)` - Get request header
- `http.get_query_param(req, name)` - Get query parameter
- `http.get_path_param(req, name)` - Get URL path parameter
- `http.request_free(req)` - Free request

**Response Building:**
- `http.response_create()` - Create response
- `http.response_set_status(res, code)` - Set HTTP status code
- `http.response_set_header(res, name, value)` - Set response header
- `http.response_set_body(res, body)` - Set response body
- `http.response_json(res, json)` - Set JSON response
- `http.server_response_free(res)` - Free response

### TCP (`std.tcp`)

```aether
import std.tcp

main() {
    // Client
    sock = tcp.connect("localhost", 8080);
    tcp.send(sock, "Hello");
    data = tcp.receive(sock, 1024);
    tcp.close(sock);

    // Server
    server = tcp.listen(8080);
    client = tcp.accept(server);
    tcp.send(client, "Welcome");
    tcp.close(client);
    tcp.server_close(server);
}
```

**Functions:**
- `tcp.connect(host, port)` - Connect to server
- `tcp.send(sock, data)` - Send data
- `tcp.receive(sock, max)` - Receive data
- `tcp.close(sock)` - Close socket
- `tcp.listen(port)` - Create server socket
- `tcp.accept(server)` - Accept connection
- `tcp.server_close(server)` - Close server

---

## Logging (`std.log`)

Structured logging with levels.

```aether
import std.log

main() {
    log.init("app.log", 0)  // 0 = LOG_DEBUG

    log.write(0, "Debug message")
    log.write(1, "Info message")
    log.write(2, "Warning message")
    log.write(3, "Error message")

    log.print_stats()
    log.shutdown()
}
```

**Log Levels:**
- `0` = DEBUG
- `1` = INFO
- `2` = WARN
- `3` = ERROR
- `4` = FATAL

**Functions:**
- `log.init(filename, level)` - Initialize logging to file with minimum level
- `log.shutdown()` - Shutdown logging
- `log.write(level, message)` - Write a log message at the given level
- `log.set_level(level)` - Set minimum level
- `log.set_colors(enabled)` - Enable/disable colored output (1/0)
- `log.set_timestamps(enabled)` - Enable/disable timestamps (1/0)
- `log.print_stats()` - Print logging statistics

---

## Math (`std.math`)

Mathematical functions. Note: `abs`, `min`, `max`, and `clamp` have separate int/float variants.

```aether
import std.math

main() {
    // Basic operations (type-specific variants)
    a = math.abs_int(-5)           // 5
    af = math.abs_float(-3.14)     // 3.14
    lo = math.min_int(3, 7)        // 3
    hi = math.max_int(3, 7)        // 7
    c = math.clamp_int(15, 0, 10)  // 10

    // Trigonometry
    s = math.sin(0.5)
    c = math.cos(0.5)
    t = math.tan(0.5)

    // Inverse trig
    as = math.asin(0.5)
    ac = math.acos(0.5)
    at = math.atan2(1.0, 1.0)

    // Power, roots, logarithms
    sq = math.sqrt(16.0)    // 4.0
    p = math.pow(2.0, 3.0)  // 8.0
    l = math.log(2.718)     // ~1.0
    e = math.exp(1.0)       // ~2.718

    // Rounding
    fl = math.floor(3.7)    // 3.0
    ce = math.ceil(3.2)     // 4.0
    ro = math.round(3.5)    // 4.0

    // Random
    math.random_seed(12345)
    r = math.random_int(1, 100)
    f = math.random_float()
}
```

**Basic (int/float variants):**
- `math.abs_int(x)` / `math.abs_float(x)` - Absolute value
- `math.min_int(a, b)` / `math.min_float(a, b)` - Minimum
- `math.max_int(a, b)` / `math.max_float(a, b)` - Maximum
- `math.clamp_int(x, min, max)` / `math.clamp_float(x, min, max)` - Clamp to range

**Trigonometry:**
- `math.sin(x)`, `math.cos(x)`, `math.tan(x)` - Trig functions
- `math.asin(x)`, `math.acos(x)`, `math.atan(x)` - Inverse trig
- `math.atan2(y, x)` - Two-argument arctangent

**Power / Logarithms:**
- `math.sqrt(x)` - Square root
- `math.pow(base, exp)` - Power
- `math.log(x)` - Natural logarithm
- `math.log10(x)` - Base-10 logarithm
- `math.exp(x)` - Exponential (e^x)

**Rounding:**
- `math.floor(x)`, `math.ceil(x)`, `math.round(x)`

**Random:**
- `math.random_seed(seed)` - Seed RNG
- `math.random_int(min, max)` - Random integer in range
- `math.random_float()` - Random float 0.0-1.0

---

## I/O (`std.io`)

Console output, file operations, and environment variable access.

```aether
import std.io

main() {
    io.print("Hello ")
    io.print_line("World")
    io.print_int(42)
    io.print_line("")

    // io.getenv() and io.read_file() return strings directly.
    home = io.getenv("HOME")
    if (home != 0) {
        io.print_line(home)
    }

    content = io.read_file("myfile.txt")
    if (content != 0) {
        io.print_line(content)
    }
}
```

**Console Output:**
- `io.print(str)` - Print string (takes raw `const char*`)
- `io.print_line(str)` - Print string with newline (takes raw `const char*`)
- `io.print_int(value)` - Print integer
- `io.print_float(value)` - Print float

**File Operations:**
- `io.read_file(path)` - Read entire file contents as a string
- `io.write_file(path, content)` - Write content to file (returns 1 on success, 0 on failure)
- `io.append_file(path, content)` - Append content to file (returns 1 on success, 0 on failure)
- `io.file_exists(path)` - Check if file exists (returns 1/0)
- `io.delete_file(path)` - Delete file (returns 1 on success, 0 on failure)

**File Info:**
- `io.file_info(path)` - Get file metadata (returns ptr)
- `io.file_info_free(info)` - Free file info

**Environment:**
- `io.getenv(name)` - Get environment variable value as a string
- `io.setenv(name, value)` - Set environment variable (returns 1 on success)
- `io.unsetenv(name)` - Unset environment variable (returns 1 on success)

---

## Concurrency

### Built-in Functions

- `spawn(ActorName())` - Create actor instance
- `wait_for_idle()` - Block until all actors finish
- `sleep(milliseconds)` - Pause execution

---

## See Also

- [Getting Started](getting-started.md)
- [Tutorial](tutorial.md)
- [Module System Design](module-system-design.md)
- [Standard Library API](stdlib-api.md)
