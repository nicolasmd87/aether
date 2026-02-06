# Aether Standard Library Reference

Complete reference for Aether's standard library modules.

> **Note:** The standard library is under active development. Some APIs documented here may be partially implemented or planned for future releases. The runtime provides C implementations in `std/` that can be linked via the `ae` tool.

## Collections (`std.collections`)

### HashMap

High-performance hash map using Robin Hood hashing for better cache locality.

```aether
import std.collections.HashMap

map = HashMap.new()
map.insert("key", "value")
value = map.get("key")
map.remove("key")
size = map.size()
```

**Methods:**
- `new()`: Create new HashMap
- `insert(key, value)`: Insert or update key-value pair
- `get(key)`: Get value by key (returns null if not found)
- `remove(key)`: Remove key-value pair
- `contains(key)`: Check if key exists
- `size()`: Get number of elements
- `is_empty()`: Check if map is empty
- `clear()`: Remove all elements

**Performance:** O(1) average case for insert, get, remove

### Set

Set implementation with set operations.

```aether
import std.collections.Set

set = Set.new()
set.add("item")
set.remove("item")
has = set.contains("item")

a = Set.from(["a", "b", "c"])
b = Set.from(["b", "c", "d"])
union = a.union(b)         // {a, b, c, d}
intersection = a.intersection(b)  // {b, c}
difference = a.difference(b)      // {a}
```

**Methods:**
- `new()`: Create new Set
- `add(element)`: Add element
- `remove(element)`: Remove element
- `contains(element)`: Check membership
- `union(other)`: Set union
- `intersection(other)`: Set intersection
- `difference(other)`: Set difference
- `is_subset(other)`: Check if subset
- `is_superset(other)`: Check if superset

### Vector

Dynamic array with amortized O(1) append.

```aether
import std.collections.Vector

vec = Vector.new()
vec.push(1)
vec.push(2)
vec.push(3)

item = vec.get(0)
vec.set(0, 10)
vec.remove(1)

size = vec.size()
vec.reverse()
vec.sort()
```

**Methods:**
- `new()`: Create new Vector
- `push(element)`: Append element
- `pop()`: Remove and return last element
- `get(index)`: Get element at index
- `set(index, element)`: Set element at index
- `insert(index, element)`: Insert at index
- `remove(index)`: Remove at index
- `size()`: Get number of elements
- `reverse()`: Reverse order
- `sort()`: Sort elements

### PriorityQueue

Binary heap implementation for priority-based task scheduling.

```aether
import std.collections.PriorityQueue

pq = PriorityQueue.new_min()  // Min-heap
pq = PriorityQueue.new_max()  // Max-heap

pq.insert(task)
highest = pq.peek()
task = pq.extract()
```

**Methods:**
- `new_min()`: Create min-heap (smallest first)
- `new_max()`: Create max-heap (largest first)
- `insert(element)`: Insert with O(log n)
- `extract()`: Remove and return min/max with O(log n)
- `peek()`: View min/max without removing O(1)
- `size()`: Get number of elements
- `is_empty()`: Check if empty

## Logging (`std.log`)

Structured logging with levels and async writing.

```aether
import std.log as Log

Log.init("app.log")
Log.set_level(Log.INFO)

Log.debug("Debug message")
Log.info("Info message")
Log.warn("Warning message")
Log.error("Error message")
```

**Log Levels:**
- `DEBUG`: Detailed diagnostic information
- `INFO`: General informational messages
- `WARN`: Warning messages
- `ERROR`: Error messages

## Strings (`std.string`)

Immutable, reference-counted strings with comprehensive operations.

### String Creation and Operations

```aether
import std.string

// Create strings
s = string.new("Hello")
s2 = string.concat(s, " World")

// String operations
len = string.length(s)
upper = string.to_upper(s)
lower = string.to_lower(s)
trimmed = string.trim(s)

// Searching
contains = string.contains(s, "ell")
index = string.index_of(s, "l")
starts = string.starts_with(s, "He")
ends = string.ends_with(s, "lo")

// Substrings and splitting
sub = string.substring(s, 0, 3)  // "Hel"
parts = string.split(s, ",")
```

### String Parsing

Convert strings to numbers with error handling.

```aether
// Parse integers
input = "42"
value = 0
if (string.to_int(input, value)) {
    print("Parsed: ")
    print(value)
}

// Parse floats
pi_str = "3.14159"
pi = 0.0
if (string.to_float(pi_str, pi)) {
    print("Pi: ")
    print(pi)
}
```

**Parsing Functions:**
- `to_int(str, out)`: Parse integer, returns 1 on success, 0 on failure
- `to_long(str, out)`: Parse long integer
- `to_float(str, out)`: Parse float
- `to_double(str, out)`: Parse double

### String Formatting

Printf-style string formatting.

```aether
// Format strings (C API - available from runtime)
formatted = string.format("User %s has %d points", name, score)
message = string.format("Value: %.2f", 3.14159)
```

**Format Specifiers:**
- `%s`: String
- `%d`: Integer
- `%f`: Float/double
- `%x`: Hexadecimal
- `%%`: Literal percent sign

## File System (`std.fs`)

Cross-platform file system operations.

```aether
import std.fs as FS

// File operations
exists = FS.file_exists("path/to/file.txt")
content = FS.read_file("file.txt")
FS.write_file("file.txt", "content")
FS.delete_file("file.txt")

// Directory operations
FS.create_dir("new_folder")
files = FS.list_dir("folder")
FS.delete_dir("folder")
```

**Functions:**
- `file_exists(path)`: Check if file exists
- `read_file(path)`: Read file contents as string
- `write_file(path, content)`: Write string to file
- `delete_file(path)`: Delete file
- `create_dir(path)`: Create directory
- `list_dir(path)`: List directory contents
- `delete_dir(path)`: Delete directory

## Environment (`std.io`)

Access environment variables and command-line arguments.

### Environment Variables

```aether
import std.io as IO

// Read environment variable
home = IO.getenv("HOME")
if (home) {
    print("Home: ")
    print(home)
}

// Set environment variable
IO.setenv("MY_VAR", "my_value")

// Unset environment variable
IO.unsetenv("MY_VAR")
```

**Functions:**
- `getenv(name)`: Get environment variable value (returns null if not set)
- `setenv(name, value)`: Set environment variable
- `unsetenv(name)`: Remove environment variable

### Command-Line Arguments

Access program arguments passed at runtime.

```aether
// Available as globals in main()
// aether_argc: Number of arguments
// aether_argv: Array of argument strings

main() {
    count = args_count()
    print("Argument count: ")
    print(count)
    print("\n")

    for (i = 0; i < count; i = i + 1) {
        arg = args_get(i)
        print("Arg ")
        print(i)
        print(": ")
        print(arg)
        print("\n")
    }
}
```

**Functions:**
- `args_count()`: Get number of command-line arguments
- `args_get(index)`: Get argument at index (0 is program name)

## Networking (`std.net`)

Network utilities for building servers and clients.

```aether
import std.net as Net

// Server
server = Net.server_create(8080)
conn = Net.server_accept(server)
data = Net.receive(conn, 1024)
Net.send(conn, "HTTP/1.1 200 OK\r\n\r\nHello")
Net.close(conn)

// Client
socket = Net.connect("localhost", 8080)
Net.send(socket, "GET / HTTP/1.1\r\n\r\n")
response = Net.receive(socket, 4096)
Net.close(socket)
```

## Best Practices

1. **Choose the right collection:**
   - HashMap for key-value lookups
   - Set for unique elements and set operations
   - Vector for sequential data
   - PriorityQueue for scheduling

2. **Use logging for production:**
   - Set appropriate log levels
   - Log to files for persistence
   - Use structured logging

3. **Error handling:**
   - Check return values (especially for I/O)
   - Use pattern matching for error cases
   - Log errors appropriately

4. **Performance:**
   - Preallocate collections when size is known
   - Use batch operations when possible
   - Leverage pattern matching for cleaner code

## See Also

- [Getting Started](getting-started.md)
- [Tutorial](tutorial.md)
- [Architecture](architecture.md)

