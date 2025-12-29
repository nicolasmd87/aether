# Aether Standard Library

Organized standard library for the Aether programming language.

## Structure

```
std/
├── string/          String operations and manipulation
├── io/              File I/O and console operations
├── math/            Mathematical functions
├── net/             HTTP client and TCP sockets
├── collections/     ArrayList and HashMap data structures
└── json/            JSON parsing and stringification
```

## Modules

### string
- String creation, concatenation, manipulation
- Reference counting for memory management
- Unicode support

### io
- Console I/O (print, print_line)
- File operations (read, write, append, delete, exists)
- File information

### math
- Basic operations (abs, min, max, pow, sqrt)
- Trigonometric functions (sin, cos, tan)
- Rounding (floor, ceil, round)
- Random number generation

### net
- HTTP client (GET, POST, PUT, DELETE)
- TCP sockets (client and server)
- Cross-platform networking

### collections
- ArrayList: Dynamic array with automatic resizing
- HashMap: Hash table with string keys

### json
- Full JSON parsing (null, bool, number, string, array, object)
- JSON stringification
- Type-safe accessors

## Usage

Include the main header:

```c
#include "std/aether_std.h"
```

Or include specific modules:

```c
#include "std/net/aether_http.h"
#include "std/json/aether_json.h"
```

## Testing

All standard library modules have comprehensive test coverage in `tests/test_runtime_*.c`.

