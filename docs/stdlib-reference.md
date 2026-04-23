# Aether Standard Library Reference

Complete reference for Aether's standard library modules.

> **Note:** The standard library follows the canonical module pattern in [stdlib-module-pattern.md](stdlib-module-pattern.md) — fallible operations expose a `_raw` extern plus a Go-style `(value, err)` Aether wrapper; pure/infallible operations stay raw without a suffix. See the [error handling example](../examples/basics/error-handling.ae) for how the pattern is used from user code, and [std/fs/module.ae](../std/fs/module.ae) for the reference implementation.

## Platform support

| Target | Filesystem | Networking | Threading | Notes |
|---|---|---|---|---|
| Linux / macOS / BSD | full POSIX | full | full | Reference target. |
| Windows (MSYS2 / mingw-w64) | partial | full | full | Process exec uses POSIX fallbacks; `os.run` is POSIX-only until the `CreateProcessW` backend lands. Some fs operations (`symlink`, `readlink`) are stubs returning clean errors via the Go-style wrappers. |
| WASI (wasi-sdk) | per preopened paths | none | single-threaded | wasi-libc provides POSIX-compatible `fopen`/`fread`/`stat`/etc., so the normal fs code path compiles. Paths must be under a WASI preopen. |
| Emscripten (browser WASM) | off by default | off | cooperative | Builds pass `-DAETHER_NO_FILESYSTEM -DAETHER_NO_NETWORKING`. File ops return `(null, "cannot open file")` via the Go-style wrappers — no silent failures. To enable, compile with `-sFORCE_FILESYSTEM=1` and drop the define; untested in CI. |
| Bare embedded | off | off | cooperative | Same as Emscripten — stubs route all failures through the Go-style error tuples. |

When a target lacks a capability, the stub implementations in each stdlib module return `NULL` / `0` so the Go-style wrapper produces a descriptive error string rather than crashing. A call like `file.read("/etc/hosts")` on a no-fs target returns `("", "cannot open file")`, which the caller handles the same way as any other I/O error.

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
    mylist = list.new()
    defer list.free(mylist)

    // list.add returns an error string. Empty = success; non-empty
    // indicates a resize/OOM failure that was previously silent.
    list.add(mylist, 10)
    list.add(mylist, 20)

    item = list.get(mylist, 0)
    size = list.size(mylist)

    list.remove(mylist, 0)
    list.clear(mylist)
}
```

**Functions:**
- `list.new()` - Create new list
- `list.add(list, item)` → `string` - Append item, return error string
- `list.get(list, index)` - Get item at index
- `list.set(list, index, item)` - Set item at index
- `list.remove(list, index)` - Remove item at index
- `list.size(list)` - Get number of elements
- `list.clear(list)` - Remove all elements
- `list.free(list)` - Free list memory

Raw extern: `list_add_raw` (returns 1/0).

### Map (`std.map`)

Hash map implementation.

```aether
import std.map
import std.string

main() {
    mymap = map.new();
    defer map.free(mymap);

    val = string.new("Aether");
    defer string.release(val);

    // map.put returns an error string.
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
- `map.put(map, key, value)` → `string` - Insert or update, return error string
- `map.get(map, key)` - Get value by key (null if missing)
- `map.has(map, key)` - Check if key exists
- `map.remove(map, key)` - Remove key-value pair
- `map.size(map)` - Get number of entries
- `map.clear(map)` - Remove all entries
- `map.free(map)` - Free map memory

Raw extern: `map_put_raw` (returns 1/0).

### Fixed-size int array (`std.intarr`)

Packed int buffer with O(1) random access. For DP tables, flat
int-keyed lookup, and other hot paths where `std.list`'s `void*`-boxed
items cost an allocation per entry. Size is fixed at allocation —
callers that need growth use `std.list`.

```aether
import std.intarr

main() {
    // Blame LCS DP table: M rows * N cols, flat buffer.
    rows = 100
    cols = 50
    dp, err = intarr.new(rows * cols)
    if err != "" { return }

    // Hot loop — _unchecked skips the bounds check (valid index required).
    r = 0
    while r < rows {
        c = 0
        while c < cols {
            intarr_set_unchecked(dp, r * cols + c, r + c)
            c = c + 1
        }
        r = r + 1
    }

    intarr_free(dp)
}
```

**Functions:**
- `intarr.new(size)` → `(ptr, string)` - Allocate zero-initialised array
- `intarr.new_filled(size, init)` → `(ptr, string)` - Allocate with every slot set to `init`
- `intarr.get(arr, i)` → `(int, string)` - Bounds-checked read
- `intarr.set(arr, i, value)` → `string` - Bounds-checked write
- `intarr_size(arr)` → `int` - Returns -1 for null
- `intarr_fill(arr, value)` - Reset every slot to `value`
- `intarr_free(arr)` - Release

**Hot-path (caller-validated) variants, no bounds check:**
- `intarr_get_raw(arr, i)` / `intarr_set_raw(arr, i, v)` - Safe on OOB (returns 0 / no-op), no error report
- `intarr_get_unchecked(arr, i)` / `intarr_set_unchecked(arr, i, v)` - Undefined behaviour on OOB, for inner loops

---

## Strings (`std.string`)

Reference-counted strings with comprehensive operations.

### String Types: Plain Strings vs Managed Strings

> **Most users don't need managed strings.** All `std.string` functions work on both plain strings
> and managed strings transparently. `string.length("hello")` just works — no conversion needed.
> Only create managed strings via `string.new()` when you need reference counting.

Aether has two string representations:

| | `string` (plain) | Managed (`ptr` via `string.new()`) |
|---|---|---|
| **C type** | `const char*` | `AetherString*` |
| **Allocation** | Static (literals) or manual | Heap (reference-counted) |
| **Memory** | None needed | `string.release()` or `defer string.free()` |
| **Knows length?** | Computed via `strlen` | Stored in struct (`O(1)`) |
| **std.string functions** | All work | All work |

**`string`** — plain C string. String literals like `"hello"` are this type. All `std.string` functions accept these directly.

**Managed strings** — heap-allocated objects returned by `string.new()`. Typed as `ptr` in Aether code. Use when you need reference counting or the result of transformation functions like `string.trim()`, `string.to_upper()`.

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
- `string.strip_prefix(s, prefix)` → `(rest, stripped)` - If `s` starts with `prefix`, returns the remainder and 1. Otherwise returns `s` and 0. Cleaner than manual `starts_with` + `substring` length arithmetic.
- `string.copy(s)` - Return an independently-owned copy of `s`. Equivalent to `string.concat(s, "")` but with a discoverable name; callers use it to snapshot a borrowed TLS buffer before the next C call overwrites it.

For a `split_once`-style operation (find the first `sep` in `s`, return the halves), use `string.index_of(s, sep)` + two `string.substring` calls — two lines of code that avoid a tuple-unification foot-gun the typechecker currently has around three-string tuples.

**Conversion:**
- `string.to_cstr(str)` - Get raw C string pointer
- `string.from_int(value)` - Create string from integer
- `string.from_float(value)` - Create string from float

**Parsing (Go-style):**
- `string.to_int(s)` → `(int, string)` - Parse base-10 integer
- `string.to_long(s)` → `(long, string)` - Parse 64-bit integer
- `string.to_float(s)` → `(float, string)` - Parse float
- `string.to_double(s)` → `(float, string)` - Parse double

Each returns `(value, "")` on success or `(0, "invalid ...")` on parse failure. Handles leading whitespace, sign, trailing whitespace; rejects trailing non-whitespace.

Raw out-parameter externs are preserved as `string_to_int_raw`, `string_to_long_raw`, `string_to_float_raw`, `string_to_double_raw` for callers who need to distinguish zero from parse failure without a tuple destructure.

**Memory:**
- `string.retain(str)` - Increment reference count
- `string.release(str)` - Decrement reference count (frees when zero)
- `string.free(str)` - Alias for `release`

---

## File System

### Files (`std.file`)

Go-style tuple returns. Check the error string first, then use the value.

```aether
import std.file

main() {
    // Check existence
    if file.exists("data.txt") == 1 {
        size, err = file.size("data.txt")
        if err == "" { println("Size: ${size} bytes") }
    }

    // Read entire file (opens, reads, closes in one call)
    content, rerr = file.read("data.txt")
    if rerr != "" {
        println("read failed: ${rerr}")
        return
    }
    println(content)

    // Write
    werr = file.write("output.txt", "Hello")
    if werr != "" {
        println("write failed: ${werr}")
        return
    }

    // Delete
    derr = file.delete("temp.txt")
    _ = derr  // ignore if missing
}
```

**Functions:**
- `file.read(path)` → `(string, string)` - Read entire file (opens, reads, closes)
- `file.write(path, content)` → `string` - Overwrite file, return error string
- `file.open(path, mode)` → `(ptr, string)` - Low-level open (caller must `file.close`)
- `file.close(handle)` - Close file
- `file.size(path)` → `(int, string)` - Get size in bytes
- `file.delete(path)` → `string` - Delete file
- `file.exists(path)` - 1 if exists, 0 otherwise (infallible)

Raw externs: `file_open_raw`, `file_read_all_raw`, `file_write_raw`, `file_delete_raw`, `file_size_raw`.

### Directories (`std.dir`)

```aether
import std.dir

main() {
    // Check and create
    if dir.exists("output") == 0 {
        err = dir.create("output")
        if err != "" { println("mkdir failed: ${err}") }
    }

    // List contents
    list, lerr = dir.list(".")
    if lerr == "" {
        // Process list with dir.list_count / dir.list_get...
        dir.list_free(list)
    }

    // Delete
    dir.delete("temp_dir")
}
```

**Functions:**
- `dir.create(path)` → `string` - Create directory, return error string
- `dir.delete(path)` → `string` - Delete empty directory, return error string
- `dir.list(path)` → `(ptr, string)` - List contents (caller must `dir.list_free`)
- `dir.exists(path)` - 1 if exists, 0 otherwise (infallible)
- `dir.list_free(list)` - Free directory listing

Raw externs: `dir_create_raw`, `dir_delete_raw`, `dir_list_raw`.

### Paths (`std.path`)

Path functions return heap-allocated plain strings (`char*`). Use `defer free(result)` if you want explicit cleanup.

```aether
import std.path

main() {
    joined = path.join("dir", "file.txt")
    println(joined)                            // "dir/file.txt"

    dirname = path.dirname("/a/b/file.txt")    // "/a/b"
    basename = path.basename("/a/b/file.txt")  // "file.txt"
    ext = path.extension("file.txt")           // ".txt" (includes dot)
    is_abs = path.is_absolute("/usr/bin")      // 1

    println("${dirname}/${basename}")
}
```

**Functions:**
- `path.join(a, b)` - Join path components
- `path.dirname(path)` - Get directory name
- `path.basename(path)` - Get file name
- `path.extension(path)` - Get file extension including dot
- `path.is_absolute(path)` - Check if absolute path (returns 1/0)

### Full-fat filesystem (`std.fs`)

`std.file` / `std.dir` / `std.path` cover most calls; `std.fs` re-exports
them and adds the accessors that need a bit more plumbing — durable
writes, atomic rename, one-shot stat, binary-safe read.

```aether
import std.fs

main() {
    // Durable write: staging + fsync + rename.
    err = fs.write_atomic("config.json", body, string.length(body))

    // Rename composes with write_atomic for stage-then-publish.
    err = fs.rename("config.json.new", "config.json")

    // One stat, four fields.
    kind, size, mtime, err = fs.file_stat("config.json")
    //   kind: 1=file, 2=dir, 3=symlink, 4=other

    // Binary-safe read with explicit length.
    data, n, err = fs.read_binary("payload.bin")
}
```

**Functions (beyond those re-exported from `std.file`/`std.dir`/`std.path`):**
- `fs.write_atomic(path, data, length)` → `string` - Stage to `<path>.tmp.<pid>.<n>`, fsync, rename over destination. Binary-safe via explicit length.
- `fs.write_binary(path, data, length)` → `string` - Non-atomic `fopen("wb")` + `fwrite` + `fclose`. Binary-safe via explicit length. Cheaper than `write_atomic` when a partial file on crash is acceptable (scratch writes, caches).
- `fs.rename(from, to)` → `string` - POSIX `rename(2)` wrapper. Atomic when source and target are on the same filesystem.
- `fs.file_stat(path)` → `(kind, size, mtime, err)` - One `lstat(2)`; symlinks report kind 3, target is not followed.
- `fs.read_binary(path)` → `(content, length, err)` - Length-aware read preserving embedded NULs.

---

## JSON (`std.json`)

JSON parsing, creation, and serialization. RFC 8259 conformant (318/318
JSONTestSuite cases — all mandatory `y_*` and `n_*` pass).

**Implementation.** Arena-allocated parser: every parsed value,
string, and container backing array comes from a single per-document
bump-pointer arena that `json.free` releases in one step.
Character-class lookup tables drive the hot-path dispatch (whitespace,
digit, structural, string-safe). UTF-8 validation uses Hoehrmann's
public-domain DFA. Numbers follow a three-path design: pure integers
go through an int64 accumulator; fractional/exponential values within
double's exact range (≤15 significant digits, |exponent| ≤ 22) take a
`POW10` fast-double path with one multiply and one cast; anything past
those bounds falls back to `strtod` for correct IEEE-754 rounding.
Strings are decoded in two phases — a pre-scan locates the closing
quote so the decode buffer is sized exactly once — and the inner
fast-loop over safe printable-ASCII bytes dispatches to SSE2 on
`__SSE2__`, NEON on `__ARM_NEON && __aarch64__`, or a scalar LUT
fallback compiled in for WASM / embedded / anywhere else. The SIMD
kernels are gated at compile time, not run-time detected, so there's
no branch cost on the happy path and the scalar fallback is always
linked. Compiles clean under `-Wall -Wextra -Werror -pedantic` on
every target in the CI matrix. Design rationale in
[json-parser-design.md](json-parser-design.md); measured throughput
in [benchmarks/json/baseline.md](../benchmarks/json/baseline.md).

**Security.** ASan+UBSan clean on the full bench corpus including a
10 MB synthesized document. JSONTestSuite conformance: every
`y_*` case accepted, every `n_*` case rejected, `i_*` outcomes
recorded. Fixed nesting depth limit of 256 prevents stack-overflow
DoS. First-error-wins diagnostics include `<reason> at <line>:<col>`.

```aether
import std.json

main() {
    // Parse JSON string — Go-style tuple return
    data, err = json.parse("{\"name\": \"Aether\", \"version\": 1}")
    if err != "" {
        println("parse failed: ${err}")
        return
    }
    name = json.object_get(data, "name")
    println(json.get_string(name))  // "Aether"

    // Create values
    obj = json.create_object()
    json.object_set(obj, "key", json.create_string("value"))
    json.object_set(obj, "count", json.create_number(42.0))

    // Arrays
    arr = json.create_array()
    json.array_add(arr, json.create_number(1.0))
    json.array_add(arr, json.create_number(2.0))
    size = json.array_size(arr)

    // Serialize to string — returns plain char*, print directly
    output = json.stringify(obj)
    println("JSON: ${output}")

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
- `json.parse(json_str)` → `(ptr, string)` - Parse JSON, returns `(value, err)` tuple
- `json.stringify(value)` - Serialize to JSON string (returns plain `char*`, infallible)
- `json.free(value)` - Free a JSON value tree

Raw extern: `json_parse_raw`.

**Type Checking:**
- `json.type(value)` - Get type constant (0-5)
- `json.is_null(value)` - Check if null (returns 1/0)

**Value Getters:**
- `json.get_number(value)` - Get float value
- `json.get_int(value)` - Get integer value
- `json.get_bool(value)` - Get boolean (1/0)
- `json.get_string(value)` - Get string value (returns plain `char*`)

**Object Operations:**
- `json.object_get(obj, key)` - Get value by key (key is a raw string)
- `json.object_set(obj, key, value)` - Set key-value pair
- `json.object_has(obj, key)` - Check if key exists (returns 1/0)
- `json.object_size(obj)` - Number of entries (`0` for empty, `-1` if not an object)
- `json.object_entry(obj, i)` - `(key, value, err)` for the i-th entry; keys
  are yielded in insertion order (same as parsed input; same as the order
  `object_set` was called). Mutating `obj` during iteration is not supported.

**Array Operations:**
- `json.array_get(arr, index)` - Get value at index
- `json.array_add(arr, value)` - Append value
- `json.array_size(arr)` - Get array length

**Value Creation:**
- `json.create_null()`, `json.create_bool(value)`, `json.create_number(value)`
- `json.create_string(value)`, `json.create_array()`, `json.create_object()`

---

## Networking

### HTTP (`std.http`)

> **Note:** Use `import std.http` for the `http.*` prefix shown below. You can also `import std.net` which includes both HTTP and TCP functions, but the namespace prefix becomes `net` — e.g. the raw client extern is reached as `net.http_get_raw(url)`, and the Go-style wrapper as `net.get(url)`.

```aether
import std.http

main() {
    // HTTP Client — Go-style
    body, err = http.get("http://example.com")
    if err != "" {
        println("failed: ${err}")
        return
    }
    println("got: ${body}")

    // HTTP Server
    server = http.server_create(8080)
    berr = http.server_bind(server, "127.0.0.1", 8080)
    if berr != "" {
        println("bind failed: ${berr}")
        return
    }
    serr = http.server_start(server)
    if serr != "" { println("start failed: ${serr}") }
    http.server_free(server)
}
```

**Client (Go-style):**
- `http.get(url)` → `(string, string)` - HTTP GET, returns `(body, err)`
- `http.post(url, body, content_type)` → `(string, string)` - HTTP POST
- `http.put(url, body, content_type)` → `(string, string)` - HTTP PUT
- `http.delete(url)` → `(string, string)` - HTTP DELETE

All four wrappers auto-free the underlying response and return an error string for transport failures or non-2xx status codes. Raw externs: `http_get_raw`, `http_post_raw`, `http_put_raw`, `http_delete_raw`.

**Response accessors (used with raw externs):**
- `http.response_status(response)` - Read HTTP status code (0 on transport failure)
- `http.response_body(response)` - Read body as string
- `http.response_headers(response)` - Read headers as string
- `http.response_error(response)` - Read transport error, empty string on success
- `http.response_ok(response)` - 1 if request succeeded (no transport error, 2xx status), else 0
- `http.response_free(response)` - Free response

**Server Lifecycle:**
- `http.server_create(port)` - Create server (never fails)
- `http.server_bind(server, host, port)` → `string` - Bind to address, return error string
- `http.server_start(server)` → `string` - Start serving (blocking), return error string
- `http.server_stop(server)` - Stop server
- `http.server_free(server)` - Free server

Raw externs: `http_server_bind_raw`, `http_server_start_raw`.

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

> **Note:** `send` and `receive` are reserved actor keywords in Aether, so
> the TCP byte-transfer wrappers are named `write`/`read`. The raw externs
> retain their `send_raw`/`receive_raw` names.

```aether
import std.tcp

main() {
    // Client — Go-style
    sock, cerr = tcp.connect("localhost", 8080)
    if cerr != "" { println("connect failed: ${cerr}"); return }

    _, werr = tcp.write(sock, "Hello")
    if werr != "" { println("write failed: ${werr}") }

    data, rerr = tcp.read(sock, 1024)
    if rerr == "" { println("got: ${data}") }
    tcp.close(sock)

    // Server
    server, lerr = tcp.listen(8080)
    if lerr != "" { return }
    client, aerr = tcp.accept(server)
    if aerr == "" {
        tcp.write(client, "Welcome")
        tcp.close(client)
    }
    tcp.server_close(server)
}
```

**Functions (Go-style):**
- `tcp.connect(host, port)` → `(ptr, string)` - Connect, return `(socket, err)`
- `tcp.write(sock, data)` → `(int, string)` - Write bytes, return `(bytes_sent, err)`
- `tcp.read(sock, max)` → `(string, string)` - Read bytes, return `(data, err)`
- `tcp.listen(port)` → `(ptr, string)` - Create server socket
- `tcp.accept(server)` → `(ptr, string)` - Accept connection
- `tcp.close(sock)` - Close socket (infallible)
- `tcp.server_close(server)` - Close server socket

Raw externs: `tcp_connect_raw`, `tcp_send_raw`, `tcp_receive_raw`, `tcp_listen_raw`, `tcp_accept_raw`.

### Reactor-pattern async I/O (`await_io`)

Aether's runtime already owns a per-core I/O reactor (epoll on Linux,
kqueue on macOS/BSD, poll() elsewhere). `net.await_io` exposes that
reactor to Aether code so an actor can suspend on a file descriptor
*without blocking any scheduler thread*. When the fd becomes ready,
the scheduler delivers an `IoReady { fd, events }` message to the
actor's mailbox and resumes it on any available core.

```aether
import std.net

message IoReady { fd: int, events: int }
message Begin { fd: int }

actor Echo {
    state my_fd = 0
    receive {
        Begin(fd) -> {
            my_fd = fd
            err = net.await_io(fd)
            if err != "" {
                println("await_io failed: ${err}")
                exit(1)
            }
        }
        IoReady(fd, events) -> {
            // Resumed here — no OS thread was blocked while we waited.
            data, rerr = tcp.read(/*...*/)
            // ... process, then re-arm ...
            net.await_io(fd)
        }
    }
}
```

**The `IoReady` message name is reserved.** The runtime scheduler
delivers I/O-readiness notifications under a fixed message ID; the
Aether message registry assigns that same ID to any user message
named `IoReady` so your handler sees the event as a normal receive
arm.

| Function | Returns | Description |
|---|---|---|
| `net.await_io(fd)` | `string` | Register `fd` with the current core's I/O poller and suspend the calling actor. Returns `""` on success, error string on failure. One-shot: the fd is automatically unregistered after the next `IoReady` delivery. |
| `net.ae_io_cancel(fd)` | — | Abandon a pending `await_io` without waiting for the message. Rarely needed due to one-shot policy. |

**Constraints:**

- `await_io` must be called from inside an actor's `receive` handler
  (not from `main()`). The bridge reads the current actor from a TLS
  set at the top of every generated `_step()` function.
- Single-actor programs run in main-thread mode which bypasses the
  scheduler loop — and therefore the I/O reactor. Spawn at least two
  actors to force multi-threaded scheduler mode if you want `await_io`
  to function.
- The fd must outlive the `await_io` registration. If you close the
  fd before the `IoReady` fires, behavior depends on the backend
  (epoll reports EPOLLHUP; kqueue silently drops the one-shot).

**Performance:** PR #140 (C-level benchmark) demonstrated the raw
reactor pattern delivering substantially higher HTTP throughput than
the blocking keep-alive worker it replaced. `await_io` is the
Aether-language surface over the same runtime machinery — rerun the
HTTP benchmark on your own target host before relying on historical
numbers.

---

## Logging (`std.log`)

Structured logging with levels.

```aether
import std.log

main() {
    err = log.init("app.log", 0)  // 0 = LOG_DEBUG
    if err != "" {
        println("log file unavailable, falling back to stderr: ${err}")
    }

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
- `log.init(filename, level)` → `string` - Initialize logging, return error string if the log file could not be opened (logging still works via stderr as a fallback)
- `log.shutdown()` - Shutdown logging
- `log.write(level, message)` - Write a log message at the given level
- `log.set_level(level)` - Set minimum level
- `log.set_colors(enabled)` - Enable/disable colored output (1/0)
- `log.set_timestamps(enabled)` - Enable/disable timestamps (1/0)
- `log.print_stats()` - Print logging statistics

Raw extern: `log_init_raw` (returns 1/0).

---

## OS (`std.os`)

Shell execution, command output capture, and environment variables.

```aether
import std.os

main() {
    // Run a shell command, get exit code
    code = os.system("echo hello")
    println("Exit: ${code}")

    // Capture command output — Go-style tuple return
    output, err = os.exec("date")
    if err != "" {
        println("exec failed: ${err}")
        return
    }
    println("Date: ${output}")

    // Get environment variable
    home = os.getenv("HOME")
    if home != 0 {
        println("HOME = ${home}")
    }
}
```

**Functions:**
- `os.system(cmd)` - Run shell command, returns exit code (0 = success, POSIX convention)
- `os.exec(cmd)` → `(string, string)` - Run command and capture stdout, return `(output, err)`
- `os.getenv(name)` - Get environment variable (returns string, or null if not set — infallible)
- `aether_args_count()` → `int` - Number of command-line arguments
- `aether_args_get(index)` → `string` - Get the i-th argument; null if out of range
- `aether_argv0()` → `string` - Path the OS launched the current process with (argv[0]); null before `aether_args_init` runs
- `os.argv0()` → `string` - Convenience wrapper around `aether_argv0()` that returns `""` instead of null and hands back a fresh copy
- `os_execv(prog, argv_list)` → `int` - Replace the current process image with `prog`, passing an explicit `list<ptr>` argv. Uses POSIX `execvp(3)` so `prog` is looked up on `PATH` when it does not contain a slash. Flushes stdio before the exec so pre-exec output is not lost. On success this call **never returns**; on failure returns `-1` and the current process continues. Not available on Windows — use `os_run` + `exit(rc)` instead.

Raw extern: `os_exec_raw`.

**Process replacement example:**

```aether
import std.os
import std.list

main() {
    argv = list.new()
    _e1 = list.add(argv, "echo")
    _e2 = list.add(argv, "hello from")
    _e3 = list.add(argv, os.argv0())
    rc = os_execv("/bin/echo", argv)
    // Only reached if exec failed.
    println("exec failed: ${rc}")
    exit(rc)
}
```

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

    // getenv is infallible — returns the value or null if unset
    home = io.getenv("HOME")
    if home != 0 {
        io.print_line(home)
    }

    // read_file is Go-style
    content, err = io.read_file("myfile.txt")
    if err != "" {
        println("read failed: ${err}")
    } else {
        io.print_line(content)
    }
}
```

**Console Output (infallible):**
- `io.print(str)` - Print string
- `io.print_line(str)` - Print string with newline
- `io.print_int(value)` - Print integer
- `io.print_float(value)` - Print float

**File Operations (Go-style):**
- `io.read_file(path)` → `(string, string)` - Read entire file
- `io.write_file(path, content)` → `string` - Write (overwrites), return error string
- `io.append_file(path, content)` → `string` - Append to file
- `io.delete_file(path)` → `string` - Delete file
- `io.file_info(path)` → `(ptr, string)` - Get file metadata
- `io.file_info_free(info)` - Free file info
- `io.file_exists(path)` - 1 if exists, 0 otherwise (infallible)

**Environment:**
- `io.getenv(name)` - Get environment variable (returns string or null, infallible)
- `io.setenv(name, value)` → `string` - Set env var, return error string
- `io.unsetenv(name)` → `string` - Unset env var, return error string

Raw externs: `io_read_file_raw`, `io_write_file_raw`, `io_append_file_raw`, `io_delete_file_raw`, `io_file_info_raw`, `io_setenv_raw`, `io_unsetenv_raw`.

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
