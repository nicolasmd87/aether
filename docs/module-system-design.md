# Aether Module System

This document describes Aether's module and namespace system.

## Terminology

| Term | Definition | Example |
|------|------------|---------|
| **Package** | A collection of related modules | `std` (standard library) |
| **Module** | A single importable unit with functions | `std.string`, `std.http` |
| **Namespace** | The prefix used when calling functions | `string`, `http` |

## Current Implementation

The standard library uses **namespace-style function calls**:

```aether
import std.string    // Import the string module
import std.file      // Import the file module

main() {
    // Namespace = module name after last dot
    s = string.new("hello")     // string namespace
    len = string.length(s)
    string.release(s)

    if (file.exists("data.txt") == 1) {   // file namespace
        size = file.size("data.txt")
    }
}
```

### How It Works

1. `import std.X` registers `X` as a namespace
2. `X.func()` is resolved to the C function `X_func()`
3. Both styles work: `string.new()` and `string_new()` (backwards compatible)

### Available Modules

| Import | Namespace | Functions |
|--------|-----------|-----------|
| `import std.string` | `string` | `string.new()`, `string.length()`, `string.release()` |
| `import std.file` | `file` | `file.exists()`, `file.open()`, `file.size()` |
| `import std.dir` | `dir` | `dir.exists()`, `dir.create()`, `dir.list()` |
| `import std.path` | `path` | `path.join()`, `path.dirname()`, `path.basename()` |
| `import std.json` | `json` | `json.parse()`, `json.create_object()`, `json.free()` |
| `import std.http` | `http` | `http.get()`, `http.server_create()`, `http.server_start()` |
| `import std.tcp` | `tcp` | `tcp.connect()`, `tcp.send()`, `tcp.listen()` |
| `import std.list` | `list` | `list.new()`, `list.add()`, `list.get()` |
| `import std.map` | `map` | `map.new()`, `map.put()`, `map.get()` |
| `import std.math` | `math` | `math.sqrt()`, `math.sin()`, `math.cos()` |
| `import std.log` | `log` | `log.init()`, `log.info()`, `log.error()` |

---

## Future: Full Module System

> **Design Document:** The features below describe the planned module system architecture. The `module` and `export` keywords are not yet fully implemented.

The full module system will provide:
- Code organization into reusable modules
- Namespace management
- Import/export syntax
- Third-party package support
- Circular import detection

## Syntax

### Module Declaration

```aether
// File: math/geometry.ae
module math.geometry

export struct Point {
    int x
    int y
}

export distance(p1, p2) {
    dx = p1.x - p2.x
    dy = p1.y - p2.y
    return sqrt(dx*dx + dy*dy)
}

// Private helper (not exported)
sqrt_approx(n) {
    // ... implementation
}
```

### Importing Modules

```aether
// Import entire module
import math.geometry

main() {
    p1 = geometry.Point{ x: 0, y: 0 }
    p2 = geometry.Point{ x: 3, y: 4 }
    d = geometry.distance(p1, p2)
}

// Import specific items
import math.geometry (Point, distance)

main() {
    p1 = Point{ x: 0, y: 0 }
    d = distance(p1, p2)
}

// Import with alias
import math.geometry as geo

main() {
    p = geo.Point{ x: 0, y: 0 }
}
```

## Module Resolution

### File Structure

```
project/
  main.ae
  math/
    geometry.ae
    algebra.ae
  utils/
    string.ae
    io.ae
```

### Resolution Rules

1. **Relative imports:** `import ./utils/string`
2. **Absolute imports:** `import std.io` (standard library)
3. **Package imports:** `import github.com/user/package` (future)

### Search Paths

1. Current directory
2. Project root
3. `AETHER_PATH` environment variable
4. Standard library location

## Standard Library Modules

```
std/
  core/      - Basic types and operations
  io/        - Input/output
  math/      - Mathematical functions
  string/    - String operations
  actors/    - Actor utilities
  collections/ - Data structures (future)
  net/       - Networking (future)
```

### Usage Example

```aether
import std.math (sqrt, pow, PI)
import std.io (print, read_line)

main() {
    print("Enter radius: ")
    r = 5.0  // In real version: read_line()
    
    area = PI * pow(r, 2)
    print("Area: ")
    print(area)
    print("\n")
}
```

## Example: Full Module

### Module: `std/io.ae`

```aether
module std.io

// Public exports
export print(text) {
    // Implementation
}

export read_line() {
    // Implementation
}

export File = struct {
    int handle
    int is_open
}

export open_file(path) {
    // Implementation
}

export close_file(file) {
    // Implementation
}

// Private helper
validate_path(path) {
    // Not exported
}
```

### Using the Module

```aether
import std.io (File, open_file, close_file, print)

main() {
    file = open_file("data.txt")
    
    if (file.is_open) {
        print("File opened!\n")
        close_file(file)
    }
}
```

---

## Creating Packages

Developers can create their own packages using the `ae` CLI tool.

### Quick Start

```bash
# Create a new package
ae init mypackage
cd mypackage

# Build and run
ae build
ae run
```

### Package Structure

```
mypackage/
├── aether.toml       # Package manifest
├── src/
│   └── main.ae       # Main entry point
├── lib/              # Library modules (optional)
│   └── utils.ae
├── tests/            # Test files (optional)
│   └── test_utils.ae
└── README.md
```

### Package Manifest (aether.toml)

Every package has an `aether.toml` file:

```toml
[package]
name = "mypackage"
version = "0.1.0"
authors = ["Your Name <you@example.com>"]
license = "MIT"
description = "My awesome Aether package"

[dependencies]
# Dependencies go here (future feature)
# http_client = "1.0"

[dev-dependencies]
# Test dependencies

[build]
target = "native"
optimizations = "aggressive"

[[bin]]
name = "mypackage"
path = "src/main.ae"
```

### Creating Nested Packages (Go-style)

Aether supports nested packages within your project, similar to Go's package structure.

**Project Structure:**
```
myapp/
├── aether.toml
├── src/
│   └── main.ae           # Main entry point
├── lib/
│   └── utils/
│       ├── module.ae     # Module definition (extern declarations)
│       └── utils.c       # C implementation
└── tests/
```

**lib/utils/module.ae** - Define the module's exports:
```aether
// Local utils module
// Import with: import utils

// Functions must be prefixed with namespace name
extern utils_double_value(x: int) -> int
extern utils_triple_value(x: int) -> int
```

**lib/utils/utils.c** - C implementation:
```c
int utils_double_value(int x) {
    return x * 2;
}

int utils_triple_value(int x) {
    return x * 3;
}
```

**src/main.ae** - Use the local module:
```aether
import utils

main() {
    x = 5;

    // Namespace-style calls: utils.function()
    doubled = utils.double_value(x);
    print("Doubled: ");
    print(doubled);
    print("\n");
}
```

### Module Resolution

The compiler searches for local modules in this order:

1. `lib/<module>/module.ae` - Library directory with module file
2. `lib/<module>.ae` - Single-file module in lib
3. `src/<module>/module.ae` - Source directory with module file
4. `src/<module>.ae` - Single-file module in src
5. `<module>/module.ae` - Project root
6. `<module>.ae` - Single file in project root

For nested modules like `import mypackage.utils`:
- Dots are converted to slashes: `mypackage/utils/module.ae`
- Same search paths are used with the converted path

### Namespace Convention

Function names must be prefixed with the namespace:
- Import: `import utils`
- Call: `utils.double_value(x)`
- C function: `utils_double_value()`

The compiler converts `namespace.function()` to `namespace_function()`.

### Current Limitations

- Package publishing/registry not yet implemented
- Remote package downloads not yet functional
- Pure Aether modules (no C implementation) not yet supported

### Roadmap

1. **Done**: Package initialization (`ae init`)
2. **Done**: Local package building (`ae build`)
3. **Done**: Local nested package imports
4. **In Progress**: Package dependencies
5. **Planned**: Package registry/publishing
6. **Planned**: Pure Aether module implementations

## Notes

- Single-file programs continue to work without packages
- The `std` package is built-in and always available
