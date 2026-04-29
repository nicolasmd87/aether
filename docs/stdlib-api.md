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
| `import std.cryptography` | `cryptography` | `cryptography.sha256_hex(data, n)`, `cryptography.base64_encode(data, n)` |
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
    // Parse — Go-style (value, err) tuple.
    v, err = json.parse("{\"name\":\"Aether\",\"count\":42}")
    if err != "" {
        println("parse failed: ${err}")
        return
    }

    // Typed readers are infallible (sentinel 0/0.0 on wrong type).
    name_val, _ = json.object_get(v, "name")
    name, _ = json.get_string(name_val)
    count_val, _ = json.object_get(v, "count")
    count = json.get_int(count_val)
    println("${name} / ${count}")

    // Build values — each create_* allocates a standalone value. Passing
    // it to object_set / array_add transfers ownership to the container.
    obj = json.create_object()
    _ = json.object_set(obj, "x", json.create_number(1.5))
    _ = json.object_set(obj, "flag", json.create_bool(1))

    arr = json.create_array()
    _ = json.array_add(arr, json.create_number(10.0))
    _ = json.array_add(arr, json.create_number(20.0))

    out, _ = json.stringify(obj)
    println(out)

    json.free(v)
    json.free(obj)
    json.free(arr)
}
```

### JSON Functions

Fallible calls return Go-style tuples. The typed readers (`get_bool` /
`get_number` / `get_int`) return sentinels (0, 0.0) on wrong-type input
so they stay infallible.

- `json.parse(str)` → `(ptr, string)` — parse into a tree. Error is a
  position-qualified message like `"expected ':' at 3:17"`.
- `json.stringify(value)` → `(string, string)` — `(output, err)`.
- `json.free(value)` — release the value. Safe on both parsed roots
  (frees the arena) and standalone-created values.
- `json.get_string(value)` → `(string, string)` — `(text, err)`; errors
  if `value` is not a `JSON_STRING`.
- `json.object_get(obj, key)` → `(ptr, string)` — `(child, err)`.
  Absent key returns `(null, "")`, which is distinct from the error
  case `(null, "not an object")`.
- `json.object_set(obj, key, value)` → `string` — error string or `""`.
- `json.array_get(arr, index)` → `(ptr, string)` — same shape;
  out-of-range returns `(null, "")`.
- `json.array_add(arr, value)` → `string` — error string or `""`.

Infallible externs (no tuple):

- `json.type(value)` → `int` — returns one of the `JSON_*` constants.
- `json.is_null(value)` → `int`.
- `json.get_bool(value)` → `int` — 0 on wrong type.
- `json.get_number(value)` → `float` — 0.0 on wrong type.
- `json.get_int(value)` → `int` — truncates the double.
- `json.object_has(obj, key)` → `int`.
- `json.array_size(arr)` → `int`.
- `json.create_null()`, `json.create_bool(v)`, `json.create_number(v)`,
  `json.create_string(s)`, `json.create_array()`, `json.create_object()`
  → `ptr` — allocate standalone values.
- `json.last_error()` → `string` — the last parser error on the current
  thread; redundant with `json.parse`'s tuple but useful when calling
  the raw extern directly.

Raw externs (bypass the Go-style wrappers): `json_parse_raw`,
`json_parse_raw_n` (length-taking, for non-null-terminated input),
`json_stringify_raw`, `json_get_string_raw`, `json_object_get_raw`,
`json_object_set_raw`, `json_array_get_raw`, `json_array_add_raw`.
All documented in [stdlib-module-pattern.md](stdlib-module-pattern.md).

### JSON Type Constants

- `JSON_NULL` = 0
- `JSON_BOOL` = 1
- `JSON_NUMBER` = 2
- `JSON_STRING` = 3
- `JSON_ARRAY` = 4
- `JSON_OBJECT` = 5

### What `std.json` doesn't do

Coming from Go's `json.Unmarshal`, Java's Jackson, Python's `json.load` + dataclasses, or C#'s `JsonSerializer`, expect to do more by hand:

- **No struct ↔ JSON mapping.** Aether has no runtime reflection — no `instanceof`, no `T.GetType()`, no `reflect.TypeOf` — so a library function that takes a struct type and a JSON tree and populates the struct fields can't exist as a stdlib API. Callers walk the tree by hand: `json.object_get(v, "name")` then `json.get_string(...)`, repeated per field. For tree-shaped or dynamically-shaped JSON the Aether code looks similar to other languages; for struct-shaped JSON it's more verbose. A future codegen step (a `--derive-json` flag on struct definitions, or a build-step macro) could close this gap without runtime reflection, but isn't shipped today.
- **No annotations / struct tags.** `@JsonProperty("user_name")`, Go struct tags `json:"user_name,omitempty"`, etc. don't apply — there's nothing for them to attach to without struct-mapping in the first place.
- **No streaming parse.** The whole document is buffered into the arena before the tree is walkable. For multi-gigabyte JSON, use a different tool. Documents into the tens of MB are fine.
- **No JSON5 / comments / trailing commas.** Strict RFC 8259 only.
- **No pretty-print on stringify.** Compact output only. Wrap with a separate prettier if you need one.
- **No JSON Schema validation.** Validate by hand or build it on top.
- **No arbitrary-precision numbers.** Numbers are `int` or `double`; the parser auto-falls-through to `strtod` for correctly-rounded IEEE-754 on edge cases (16+ significant digits, huge exponents) but there's no `BigDecimal` / `decimal.Decimal` equivalent for financial precision.
- **Hard-coded depth limit of 256.** DoS protection against deeply nested JSON bombs; not configurable. Rare to hit in practice.

### Other structured-data formats

Beyond JSON, the stdlib has **no built-in support** for:

- **YAML** — no parser. The runtime is single-language, so configuration files for Aether projects use TOML (read by the build tool internally — not a user-facing stdlib module) or hand-rolled formats.
- **XML** — no parser. The Servirtium climate-API replay tests parse XML by hand from the WorldBank API responses (substring-extract `<double>...</double>` values from a known-shape body), not via a real DOM/SAX surface.
- **TOML** — there's a parser at `tools/apkg/toml_parser.c` used internally by the `ae` CLI to read `aether.toml` project files. It's not exposed as `std.toml`. If a project needs TOML, copying that parser or shelling out to a host-language tool are the options today.
- **INI** — no parser. Trivial to implement on top of `string.split` if needed.
- **Java-style `.properties`** — no parser. Same shape as INI without sections; same advice.
- **CSV** — no parser. `string.split(line, ",")` covers the no-quoting / no-embedded-commas case; anything more needs a real CSV parser, which isn't shipped.
- **Protocol Buffers / MessagePack / CBOR / Avro / Thrift** — no codecs. Same reflection-gap reasoning as struct ↔ JSON: without struct introspection there's no automatic encode/decode, and a hand-written codec on top of `tcp.write` / `tcp.read` / `aether_string_data` is what you'd build.

This isn't a hidden roadmap — these are absent because no downstream user has driven the need yet. If you're starting a project that needs YAML config, expect to write a parser, ship a contrib module, or shell out. The structured-data thinking in the stdlib is currently JSON-shaped and HTTP-adjacent; broader format coverage is open territory.

---

## Cryptography Library

Hash digests + Base64 codec. Built on OpenSSL's EVP API. When OpenSSL isn't linked, every wrapper returns `("", "openssl unavailable")` rather than crashing.

```aether
import std.cryptography

main() {
    digest, _ = cryptography.sha256_hex("abc", 3)
    b64,    _ = cryptography.base64_encode("\x01\x02\x03", 3)
    raw, n, _ = cryptography.base64_decode(b64)
}
```

### Hash Functions

- `cryptography.sha1_hex(data, length)` → `(string, string)` - 40-char lowercase hex digest. Legacy interop (Git, Subversion); prefer SHA-256 for new work.
- `cryptography.sha256_hex(data, length)` → `(string, string)` - 64-char lowercase hex digest.
- `cryptography.hash_hex(algo, data, length)` → `(string, string)` - Algorithm-by-name dispatcher. `algo` is `"sha1"`, `"sha256"`, or any name `EVP_get_digestbyname()` recognizes (`"sha384"`, `"sha512"`, `"sha3-256"`, ...). Returns `("", "unknown algorithm")` for unrecognized names.
- `cryptography.hash_supported(algo)` → `int` - `1` if this build can compute `algo`, `0` otherwise. Always succeeds. Use to validate user-supplied algorithm names before calling `hash_hex`.

`length` is explicit so binary payloads with embedded NULs survive. `data` may be either a plain string literal or an AetherString from `fs.read_binary` — the runtime unwraps automatically.

### Base64 (RFC 4648 §4 standard alphabet)

- `cryptography.base64_encode(data, length)` → `(string, string)` - Encode `length` bytes, **unpadded** output.
- `cryptography.base64_encode_padded(data, length)` → `(string, string)` - Encode `length` bytes, **with `=` padding** to a multiple of 4. For wire formats (auth headers, JSON-encoded blobs) that require padded output.
- `cryptography.base64_decode(b64)` → `(string, int, string)` - Decode. Returns `(bytes, byte_count, "")` on success — `bytes` is an AetherString preserving embedded NULs. Accepts both padded and unpadded input.

### What's not in `std.cryptography`

HMAC, key derivation, symmetric ciphers, signing, certificate handling, streaming digests, URL-safe Base64 (RFC 4648 §5), MD5, and constant-time comparison are all out of scope. See [stdlib-reference.md](stdlib-reference.md) §"What `std.cryptography` doesn't do" for the rationale.

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

### HTTP Client Builder (`std.http.client`)

Builder-shaped requests with full response access. The `http.get` / `http.post` / `http.put` / `http.delete` one-liners above are good for "no auth, JSON in, 200 means good" calls; reach for `std.http.client` when you need custom request headers, response-header capture, status discrimination, per-request timeouts, or methods other than the four common verbs (PROPFIND, PATCH, custom RPC verbs all work). Method is an arbitrary string. Non-2xx is not an error — the caller checks `response_status`.

```aether
import std.http.client

main() {
    req = client.request("GET", "https://api.example.com/users/42")
    client.set_header(req, "Authorization", "Bearer abc123")
    client.set_timeout(req, 30)
    resp, err = client.send_request(req)
    client.request_free(req)
    if err != "" { return }
    status = client.response_status(resp)
    body   = client.response_body(resp)
    client.response_free(resp)
}
```

**Builder + send:**
- `client.request(method, url)` → `ptr` - Build a request handle
- `client.set_header(req, name, value)` → `string` - Append a request header
- `client.set_body(req, body, length, content_type)` → `string` - Set request body (length explicit for binary safety)
- `client.set_timeout(req, seconds)` → `string` - Per-request timeout (`0` = block forever)
- `client.send_request(req)` → `(ptr, string)` - Fire it; `(resp, "")` on success, `(null, err)` on transport failure
- `client.request_free(req)` - Free the request handle

**Response accessors:**
- `client.response_status(resp)` → `int`
- `client.response_body(resp)` → `string` (binary-safe AetherString)
- `client.response_header(resp, name)` → `string` (case-insensitive)
- `client.response_headers(resp)` → `string` (raw header block)
- `client.response_error(resp)` → `string`
- `client.response_free(resp)` - Free the response

**Sugar wrappers** (pure Aether on top of the builder):
- `client.get_with_headers(url, header_pairs)` → `(string, int, string)`
- `client.post_with_status(url, body, content_type)` → `(string, int, string)`
- `client.post_json(url, value)` → `(ptr, string)` - Marshal value via `std.json`, set Content-Type + Accept
- `client.response_body_json(resp)` → `(ptr, string)` - `response_body` + `json.parse` round-trip

See `std/http/README.md` for design rationale and `tests/integration/test_http_client_v2.ae` for ten worked examples (header round-trip, status discrimination, binary body, timeout, transport failure, JSON sugar, malformed-JSON parse failure).

### HTTP Record/Replay (`std.http.server.vcr`)

Aether's implementation of [Servirtium](https://servirtium.dev) — cross-language record/replay HTTP testing. Tapes are markdown, interoperable with Java/Kotlin/Python/Go implementations of the same framework.

```aether
import std.http.server.vcr
import std.http
extern http_server_start_raw(server: ptr) -> int

message StartVCR { raw: ptr }
actor VCRActor { state s = 0
    receive { StartVCR(raw) -> { s = raw; http_server_start_raw(raw) } } }

main() {
    raw = vcr.load("tests/tapes/my.tape", 18099)
    a = spawn(VCRActor())
    a ! StartVCR { raw: raw }
    sleep(500)
    body, err = http.get("http://127.0.0.1:18099/things/42")
    vcr.eject(raw)
}
```

**Replay:** `vcr.load(tape_path, port)` / `vcr.eject(server)` / `vcr.tape_length()`.

**Record:** `vcr.record(method, path, status, content_type, body)` / `vcr.record_full(...)` / `vcr.flush(tape_path)` / `vcr.flush_or_check(tape_path)` (re-record byte-diff with `.actual` sibling on mismatch).

**Secret scrubbing** (applied at flush time; in-memory capture stays untouched): `vcr.redact(field, pattern, replacement)` / `vcr.clear_redactions()` with `vcr.FIELD_PATH` and `vcr.FIELD_RESPONSE_BODY` selectors.

**Per-interaction notes:** `vcr.note(title, body)` — record-only `[Note]` markdown block attached to the next interaction.

**Strict request matching:** `vcr.last_error()` / `vcr.clear_last_error()` — tearDown-readable mismatch diagnostics.

**Static content:** `vcr.static_content(mount_path, fs_dir)` / `vcr.clear_static_content()` — bypass-the-tape mounts for Selenium/Cypress assets.

**Markdown format options:** `vcr.emphasize_http_verbs()` / `vcr.indent_code_blocks()` / `vcr.clear_format_options()` — alternative emit forms; playback tolerates either.

Full surface, design notes, and the runnable test suite live in `std/http/README.md`.

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
delivering substantially higher HTTP throughput than a blocking
keep-alive worker. `await_io` is the Aether-language surface over
that same machinery — rerun the HTTP benchmark on your target host
to get a current figure for your environment.

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
