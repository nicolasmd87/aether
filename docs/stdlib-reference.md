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

    list.add(mylist, item1);
    list.add(mylist, item2);

    item = list.get(mylist, 0);
    size = list.size(mylist);

    list.remove(mylist, 0);
    list.clear(mylist);
    list.free(mylist);
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

    key = string.new("name");
    val = string.new("Aether");

    map.put(mymap, key, val);
    result = map.get(mymap, key);
    exists = map.has(mymap, key);

    map.remove(mymap, key);
    size = map.size(mymap);

    map.clear(mymap);
    map.free(mymap);

    string.release(key);
    string.release(val);
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

    // Memory management
    string.release(s);
    string.release(s2);
}
```

**Functions:**
- `string.new(cstr)` - Create from C string
- `string.empty()` - Create empty string
- `string.length(str)` - Get length
- `string.concat(a, b)` - Concatenate strings
- `string.char_at(str, index)` - Get character
- `string.equals(a, b)` - Check equality
- `string.compare(a, b)` - Compare (-1, 0, 1)
- `string.starts_with(str, prefix)` - Check prefix
- `string.ends_with(str, suffix)` - Check suffix
- `string.contains(str, sub)` - Search for substring
- `string.index_of(str, sub)` - Find position
- `string.substring(str, start, end)` - Extract substring
- `string.to_upper(str)` - Convert to uppercase
- `string.to_lower(str)` - Convert to lowercase
- `string.trim(str)` - Remove whitespace
- `string.retain(str)` - Increment reference count
- `string.release(str)` - Decrement and free if zero

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

### Directories (`std.dir`)

```aether
import std.dir

main() {
    // Check and create
    if (dir.exists("output") == 0) {
        dir.create("output");
    }

    // List contents
    list = dir.list(".");
    // Process list...
    dir.list_free(list);

    // Delete
    dir.delete("temp_dir");
}
```

**Functions:**
- `dir.exists(path)` - Check if directory exists
- `dir.create(path)` - Create directory
- `dir.delete(path)` - Delete empty directory
- `dir.list(path)` - List directory contents
- `dir.list_free(list)` - Free directory listing

### Paths (`std.path`)

```aether
import std.path

main() {
    joined = path.join("dir", "file.txt");
    dirname = path.dirname("/a/b/file.txt");  // "/a/b"
    basename = path.basename("/a/b/file.txt"); // "file.txt"
    ext = path.extension("file.txt");          // "txt"
    is_abs = path.is_absolute("/usr/bin");     // 1
}
```

**Functions:**
- `path.join(a, b)` - Join path components
- `path.dirname(path)` - Get directory name
- `path.basename(path)` - Get file name
- `path.extension(path)` - Get file extension
- `path.is_absolute(path)` - Check if absolute path

---

## JSON (`std.json`)

JSON parsing and creation.

```aether
import std.json

main() {
    // Create values
    num = json.create_number(42.5);
    str = json.create_string("hello");
    bool_val = json.create_bool(1);
    null_val = json.create_null();

    // Arrays
    arr = json.create_array();
    json.array_add(arr, json.create_number(1.0));
    json.array_add(arr, json.create_number(2.0));
    size = json.array_size(arr);
    item = json.array_get(arr, 0);

    // Objects
    obj = json.create_object();
    json.object_set(obj, "key", json.create_string("value"));
    val = json.object_get(obj, "key");

    // Type checking
    type = json.type(num);  // JSON_NUMBER = 2
    is_null = json.is_null(null_val);

    // Get values
    n = json.get_number(num);
    b = json.get_bool(bool_val);
    s = json.get_string(str);

    // Cleanup
    json.free(num);
    json.free(arr);
    json.free(obj);
}
```

**JSON Type Constants:**
- `JSON_NULL` = 0
- `JSON_BOOL` = 1
- `JSON_NUMBER` = 2
- `JSON_STRING` = 3
- `JSON_ARRAY` = 4
- `JSON_OBJECT` = 5

---

## Networking

### HTTP (`std.http`)

```aether
import std.http

main() {
    // HTTP Client
    response = http.get("http://example.com");
    if (response != 0) {
        // Process response
        http.response_free(response);
    }

    // HTTP Server
    server = http.server_create(8080);
    http.server_bind(server, "127.0.0.1", 8080);
    // Register handlers...
    http.server_start(server);  // Blocking
    http.server_free(server);
}
```

**Client Functions:**
- `http.get(url)` - HTTP GET request
- `http.post(url, body, content_type)` - HTTP POST
- `http.put(url, body, content_type)` - HTTP PUT
- `http.delete(url)` - HTTP DELETE
- `http.response_free(response)` - Free response

**Server Functions:**
- `http.server_create(port)` - Create server
- `http.server_bind(server, host, port)` - Bind to address
- `http.server_start(server)` - Start serving
- `http.server_stop(server)` - Stop server
- `http.server_free(server)` - Free server

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
    log.init("app.log", 0);  // 0 = LOG_DEBUG

    log.debug("Debug message");
    log.info("Info message");
    log.warn("Warning message");
    log.error("Error message");

    log.shutdown();
}
```

**Log Levels:**
- `LOG_DEBUG` = 0
- `LOG_INFO` = 1
- `LOG_WARN` = 2
- `LOG_ERROR` = 3

**Functions:**
- `log.init(filename, level)` - Initialize logging
- `log.shutdown()` - Shutdown logging
- `log.set_level(level)` - Set minimum level
- `log.debug(msg)` - Debug message
- `log.info(msg)` - Info message
- `log.warn(msg)` - Warning message
- `log.error(msg)` - Error message

---

## Math (`std.math`)

Mathematical functions.

```aether
import std.math

main() {
    // Basic operations
    a = math.abs(-5);       // 5
    min = math.min(3, 7);   // 3
    max = math.max(3, 7);   // 7

    // Trigonometry
    s = math.sin(0.5);
    c = math.cos(0.5);
    t = math.tan(0.5);

    // Power and roots
    sq = math.sqrt(16.0);   // 4.0
    p = math.pow(2.0, 3.0); // 8.0

    // Rounding
    fl = math.floor(3.7);   // 3.0
    ce = math.ceil(3.2);    // 4.0
    ro = math.round(3.5);   // 4.0

    // Random
    math.random_seed(12345);
    r = math.random_int(1, 100);
    f = math.random_float();
}
```

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
