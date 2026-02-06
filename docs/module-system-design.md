# Aether Module System

> **Design Document:** This describes the planned module system architecture. Basic `import` statements work for including files, but `module`, `export`, and qualified imports are not yet implemented. See current import support in the [language reference](language-reference.md).

This document describes Aether's module system for code organization and namespace management.

## Overview

The module system provides:
- Code organization into reusable modules
- Namespace management
- Import/export syntax
- Standard library as modules (std.collections, std.log, std.net, etc.)
- Module resolution with automatic file loading
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

## Notes

- Single-file programs continue to work without modules
- Third-party package management via `ae add` (GitHub packages)
