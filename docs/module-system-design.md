# Aether Module System Design

## Status: ✅ Implemented in v0.4.0

This document outlines the design and implementation of Aether's module system.

## Overview

The module system provides:
- ✅ Code organization into reusable modules
- ✅ Namespace management
- ✅ Import/export syntax
- ✅ Standard library as modules (std.collections, std.log, std.net, etc.)
- ✅ Module resolution with automatic file loading
- ✅ Circular import detection
- ⚠️ Third-party package support (via apkg package manager - in progress)

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

## Implementation Plan

### Phase 1: Basic Modules (v0.2.0)

- [x] Design syntax and semantics
- [ ] Add `module` and `import` keywords to lexer
- [ ] Parse module and import statements
- [ ] Implement module resolution
- [ ] Add export/public visibility
- [ ] Update type checker for module scope
- [ ] Update code generator for modules
- [ ] Reorganize stdlib as modules

### Phase 2: Advanced Features (v0.3.0)

- [ ] Module caching for faster compilation
- [ ] Circular dependency detection
- [ ] Selective imports
- [ ] Module aliases
- [ ] Re-exports

### Phase 3: Package Management (v0.4.0)

- [ ] Package manifest (aether.json)
- [ ] Package repository
- [ ] Version management
- [ ] Dependency resolution
- [ ] `aether get` command

## Compiler Changes

### Lexer

Add tokens:
- `TOKEN_MODULE`
- `TOKEN_IMPORT`
- `TOKEN_EXPORT`
- `TOKEN_AS`

### Parser

New AST nodes:
- `AST_MODULE_DECLARATION`
- `AST_IMPORT_STATEMENT`
- `AST_EXPORT_STATEMENT`

### Type Checker

- Module-level symbol tables
- Cross-module type checking
- Export/visibility validation
- Import resolution

### Code Generator

- Module-prefixed C names
- Header file generation
- Link-time dependencies

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

## Backward Compatibility

- Single-file programs continue to work without modules
- Gradual migration path
- Standard library provides both old and new APIs during transition

## Testing Strategy

1. **Unit tests:** Module resolution, import parsing
2. **Integration tests:** Multi-file programs
3. **Regression tests:** Ensure single-file programs still work
4. **Stdlib tests:** All standard library modules

## Documentation Updates

- Language reference chapter on modules
- Tutorial on organizing code with modules
- Migration guide from single-file to modules
- Standard library API docs per module

## Timeline

- **v0.2.0 (Q2 2025):** Basic module system
- **v0.3.0 (Q3 2025):** Advanced features
- **v0.4.0 (Q4 2025):** Package management

## References

Similar module systems in other languages:
- **OCaml/SML:** ML-style modules with signatures and functors
- **Rust:** Explicit `mod` and `use`, cargo packages  
- **Go:** Package-based, implicit from directory
- **Erlang:** Module attributes with export lists

Aether's design draws from ML-family module systems while incorporating Rust's explicit exports and Erlang's pragmatic approach.

