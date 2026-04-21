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
3. Dot-style calls: `string.new()`, `list.free()`, etc. (compiler translates to C-level `string_new`, `list_free`)

### Module Orchestration Phase

Before type checking, the compiler runs a module orchestration phase
(added between parsing and type checking in the compilation pipeline):

1. **Scan**: Walk the main program AST for `import` statements
2. **Resolve**: Map each import to a file path (stdlib, lib/, src/ paths)
3. **Parse**: Lex and parse each module file into an AST
4. **Cache**: Store the parsed AST in `global_module_registry`
5. **Recurse**: Process each module's own imports (transitive dependencies)
6. **Cycle Check**: Build a dependency graph and detect circular imports via DFS

Both the type checker and code generator then use `module_find()` to retrieve
cached ASTs — each module file is read and parsed exactly once.

### Available Modules

| Import | Namespace | Functions |
|--------|-----------|-----------|
| `import std.string` | `string` | `string.new()`, `string.length()`, `string.release()` |
| `import std.file` | `file` | `file.open()`, `file.read_all()`, `file.write()`, `file.exists()`, `file.close()`, `file.delete()`, `file.size()` |
| `import std.dir` | `dir` | `dir.exists()`, `dir.create()`, `dir.delete()`, `dir.list()` |
| `import std.path` | `path` | `path.join()`, `path.dirname()`, `path.basename()`, `path.extension()`, `path.is_absolute()` |
| `import std.fs` | `file`, `dir`, `path` | Combined module — re-exports `file.*`, `dir.*`, and `path.*` operations |
| `import std.json` | `json` | `json.parse()`, `json.create_object()`, `json.free()` |
| `import std.http` | `http` | `http.get()`, `http.server_create()`, `http.server_start()` |
| `import std.tcp` | `tcp` | `tcp.connect()`, `tcp.send()`, `tcp.listen()` |
| `import std.net` | `tcp`, `http` | Combined module — re-exports `tcp.*` and `http.*` operations |
| `import std.list` | `list` | `list.new()`, `list.add()`, `list.get()`, `list.set()`, `list.remove()` |
| `import std.map` | `map` | `map.new()`, `map.put()`, `map.get()`, `map.has()`, `map.remove()` |
| `import std.collections` | `list`, `map` | Combined module — re-exports `list.*` and `map.*` operations |
| `import std.math` | `math` | `math.abs_int()`, `math.sqrt()`, `math.sin()`, `math.random_int()` |
| `import std.log` | `log` | `log.init()`, `log.write()`, `log.shutdown()` |
| `import std.io` | `io` | `io.print()`, `io.read_file()`, `io.getenv()` |
| `import std.os` | `os` | `os.system()`, `os.exec()`, `os.getenv()` |

---

## Export Visibility

Use `export` to control which symbols are part of a module's public API:

```aether
// lib/geometry/module.ae

export const PI = 3

export distance(x1, y1, x2, y2) {
    dx = x1 - x2
    dy = y1 - y2
    return sqrt_approx(dx * dx + dy * dy)
}

// Private helper — not accessible from outside
sqrt_approx(n) {
    return n  // placeholder
}
```

```aether
import geometry

main() {
    d = geometry.distance(0, 0, 3, 4)  // OK — exported
    println(geometry.PI)                 // OK — exported
    // geometry.sqrt_approx(25)          // Error: not exported
}
```

**Rules:**
- `export` works with functions (`export func_name(...)`), constants (`export const NAME = value`), and `fn`-keyword functions (`export fn func_name(...)`)
- If a module has **any** `export` declarations, only exported symbols are accessible from importers. Non-exported symbols are private.
- If a module has **no** `export` declarations, all symbols are public (backwards compatible)
- Private functions can still be used internally by exported functions — they are merged into the program but not accessible via `module.name()`

## Future

Work planned on top of the current module system:

- `import math.geometry as geo` — import aliases (parser accepts them; wiring through the typechecker and symbol table is next).
- Exporting structs and actors from modules.
- Re-exports (module A re-exporting module B's symbols).
- Transitive dependency resolution.

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
│   └── utils/
│       └── module.ae # import utils → lib/utils/module.ae
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
# Add with: ae add github.com/user/repo
# github.com/user/mylib = "v1.0.0"

[dev-dependencies]
# Test dependencies

[build]
target = "native"
optimizations = "aggressive"

[[bin]]
name = "mypackage"
path = "src/main.ae"
```

### Creating Local Modules

Aether supports local modules within your project:

**Project Structure:**
```
myapp/
├── aether.toml
├── src/
│   └── main.ae              # Main entry point
├── lib/
│   └── utils/
│       └── module.ae        # Pure Aether module
└── tests/
```

**lib/utils/module.ae** — define your module:
```aether
export const MULTIPLIER = 2

export double_value(x) {
    return multiply(x, MULTIPLIER)
}

export triple_value(x) {
    return multiply(x, 3)
}

// Private helper
multiply(a, b) {
    return a * b
}
```

**src/main.ae** — use the module:
```aether
import utils

main() {
    println(utils.double_value(5))   // 10
    println(utils.triple_value(5))   // 15
    println(utils.MULTIPLIER)        // 2
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

7. `~/.aether/packages/<name>/src/module.ae` - Installed package (via `ae add`)
8. `~/.aether/packages/<name>/module.ae` - Installed package (flat layout)

Packages installed with `ae add` are searched after local paths. The package name is the first component of the import path.

### Namespace Convention

Function names must be prefixed with the namespace:
- Import: `import utils`
- Call: `utils.double_value(x)`
- C function: `utils_double_value()`

The compiler converts `namespace.function()` to `namespace_function()`.

### Pure Aether Modules

You can write reusable modules in pure Aether — no C backing file required:

**lib/mymath/module.ae:**
```aether
export const PI_APPROX = 3

export double_it(x) {
    return x * 2
}

export add(a, b) {
    return a + b
}

// Intra-module calls work — functions can call each other
export double_and_add(x, y) {
    return add(double_it(x), y)
}
```

**src/main.ae:**
```aether
import mymath

main() {
    println(mymath.double_it(5))    // 10
    println(mymath.add(3, 4))       // 7
    println(mymath.double_and_add(5, 3))  // 13
    println(mymath.PI_APPROX)       // 3
}
```

**How it works:**

After module orchestration, the compiler clones each module's function and constant AST nodes into the main program with namespace-prefixed names (`double_it` → `mymath_double_it`). Intra-module calls, constant references, and constant-to-constant references (e.g., `const DOUBLE_BASE = BASE * 2`) are renamed automatically. Function parameters and local variables correctly shadow module constants — `check(SCALE) { return SCALE }` returns the parameter, not the module constant `SCALE`. This makes the entire downstream pipeline (type inference, type checking, codegen) work without modification — merged functions are just regular top-level functions.

**What's supported:**
- Functions (with type inference from call sites)
- Constants (`const NAME = value`), including constants referencing other constants
- Intra-module calls (functions calling other functions and referencing constants in the same module)
- Export visibility (`export` keyword controls public API)
- Multiple module imports in the same program
- Mixing pure modules with stdlib imports
- Parameter/local variable shadowing of module constants

**On the module-system roadmap (language-level):**
- Actors from modules (dispatch tables currently assume main-program scope)
- Message definitions from modules
- Re-exports (module A re-exporting module B's functions)
- Module-level mutable state

### Roadmap

1. **Done**: Package initialization (`ae init`)
2. **Done**: Local package building (`ae build`)
3. **Done**: Local nested package imports
4. **Done**: Pure Aether module implementations
5. **Done**: Export visibility enforcement
6. **Planned**: Package dependencies
7. **Planned**: Package registry/publishing

## Notes

- Single-file programs continue to work without packages
- The `std` package is built-in and always available
