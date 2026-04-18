# Embedding Aether as a Shared Library (`--emit=lib`)

`aetherc --emit=lib` (or `ae build --emit=lib`) compiles a `.ae` file into
a dynamic library (`.so` on Linux, `.dylib` on macOS) with ABI-stable
entry points that any FFI-capable language can call: C, Java (Panama),
Python (ctypes or SWIG), Go (cgo), Ruby, Rust (`extern "C"`), and so on.

This document covers the v1 contract. For the broader design rationale,
see [aether-embedded-in-host-applications.md](aether-embedded-in-host-applications.md).

## Quick example

```aether
// config.ae
import std.map

build_config(env: string, port: int) {
    m = map_new()
    map_put(m, "env", env)
    map_put(m, "port", port)
    return m
}
```

Build:

```sh
ae build --emit=lib config.ae
# produces libconfig.so (or libconfig.dylib on macOS)
```

Consume from C:

```c
#include "aether_config.h"
#include <dlfcn.h>

typedef AetherValue* (*build_fn)(const char*, int32_t);

int main(void) {
    void* h = dlopen("./libconfig.so", RTLD_NOW);
    build_fn build = dlsym(h, "aether_build_config");
    AetherValue* cfg = build("prod", 8080);

    const char* env  = aether_config_get_string(cfg, "env");
    int32_t     port = aether_config_get_int(cfg, "port", 0);
    printf("%s on %d\n", env, port);   // prod on 8080

    aether_config_free(cfg);
    dlclose(h);
}
```

## The flag matrix

| Flag | Effect |
|---|---|
| `--emit=exe` *(default)* | Current behaviour — produces an executable. |
| `--emit=lib` | No `main()` in the output; every top-level Aether function gets an `aether_<name>()` C-ABI alias; built with `-fPIC -shared`. |
| `--emit=both` | Accepted by `aetherc` (emits both symbols into the `.c` file) but **not yet wired up in `ae build`** — run `ae build --emit=exe` and `ae build --emit=lib` separately when you need both artifacts from one source. |

## Naming

Every exported Aether function becomes `aether_<name>` in the library:

| Aether declaration | C symbol |
|---|---|
| `sum(a: int, b: int) { ... }` | `aether_sum` |
| `greet(name: string) { ... }` | `aether_greet` |
| `build_config(env, port)` | `aether_build_config` |

The internal `sum`, `greet`, etc. symbols are **also present** in the
library for now (v1 doesn't mark them `static`). Callers should use the
`aether_<name>` entry points for ABI stability — the un-prefixed names
are an implementation detail and may be hidden in a future version.

## Type mapping

Parameter and return types on exported functions are mapped to a fixed
public ABI:

| Aether type | Public C type | Notes |
|---|---|---|
| `int` | `int32_t` | Fixed width for cross-language clarity |
| `long` | `int64_t` | The Aether source keyword for 64-bit integers is `long`; the lexer token `TOKEN_INT64` backs both `long` and `int64_t` internally |
| `float` | `float` | IEEE 754 binary32 |
| `bool` | `int32_t` | `0` = false, `1` = true |
| `string` | `const char*` | Borrowed pointer; copy in the host if the lifetime matters |
| `ptr` / `list` / `map` | `AetherValue*` | Opaque handle; walk with `aether_config_*` accessors |

Functions whose parameters or returns use types outside this table
(tuples, structs, closures, actor refs) compile but **don't get an
`aether_<name>` alias**. `aetherc` prints a warning naming the skipped
function. You can still use those types internally; they just can't
cross the FFI boundary in v1.

## The `AetherValue*` accessor API

Composite Aether values — maps, lists, generic `ptr`s — come back to the
host as `AetherValue*` handles. The host walks them with the functions
declared in `runtime/aether_config.h`:

```c
// Map accessors
const char*  aether_config_get_string(AetherValue* root, const char* key);
int32_t      aether_config_get_int   (AetherValue* root, const char* key, int32_t default_value);
int64_t      aether_config_get_int64 (AetherValue* root, const char* key, int64_t default_value);
float        aether_config_get_float (AetherValue* root, const char* key, float  default_value);
int32_t      aether_config_get_bool  (AetherValue* root, const char* key, int32_t default_value);
AetherValue* aether_config_get_map   (AetherValue* root, const char* key);
AetherValue* aether_config_get_list  (AetherValue* root, const char* key);
int32_t      aether_config_has       (AetherValue* root, const char* key);

// List accessors
int32_t      aether_config_list_size       (AetherValue* list);
AetherValue* aether_config_list_get        (AetherValue* list, int32_t index);
const char*  aether_config_list_get_string (AetherValue* list, int32_t index);
int32_t      aether_config_list_get_int    (AetherValue* list, int32_t index, int32_t default_value);
int64_t      aether_config_list_get_int64  (AetherValue* list, int32_t index, int64_t default_value);
float        aether_config_list_get_float  (AetherValue* list, int32_t index, float   default_value);
int32_t      aether_config_list_get_bool   (AetherValue* list, int32_t index, int32_t default_value);

// Lifetime
void aether_config_free(AetherValue* root);
```

### Ownership

| Return value | Ownership |
|---|---|
| Root `AetherValue*` from an `aether_<name>()` call | **Owned** — call `aether_config_free(root)` |
| Nested map/list handle from `_get_map` / `_get_list` / `_list_get` | **Borrowed** — valid until the root is freed; do NOT free individually |
| `const char*` from `_get_string` / `_list_get_string` | **Borrowed** — points into the tree; copy if you need it past `aether_config_free` |

### Behaviour of missing keys / out-of-range indices

- Typed getters return the `default_value` the caller supplied.
- `_get_string` / `_get_map` / `_get_list` / `_list_get_string` return `NULL`.
- `_list_size(NULL)` returns `0`; `_has(NULL, k)` returns `0`.

### Type safety caveat

Aether maps and lists are **untyped** internally — values are stored as
opaque `void*` with no runtime tag. The accessors reinterpret the stored
value as the requested type without checking. If an Aether script stored
an `int` at `"port"` and the host asks for a string, the host gets
garbage. Document your script's output shape like any other FFI contract.

## Capability-empty default

`--emit=lib` rejects `.ae` files that import:

- `std.net`, `std.http`, `std.tcp` — networking
- `std.fs` — filesystem
- `std.os` — process / environment / shell

The rationale: a library embedded in a host process shouldn't have
ambient network, filesystem, or process-spawning access. Those are
capabilities the **host** grants; the script should only compute and
return data. The host mediates I/O through whatever its own runtime
provides.

`std.map`, `std.list`, `std.string`, `std.json`, `std.math`, and the
other capability-free standard modules are allowed.

A future version may add `--emit=lib --with=net,fs` for opt-in access.
For now, there's no escape hatch — if you need I/O in the library, the
design is wrong.

## Using SWIG to generate Java/Python/Ruby/Go bindings

The repo ships a minimal SWIG interface at `runtime/aether_config.i`.
It wraps `aether_config.h` and marks `aether_config_free` as the
destructor so target-language GC integrates cleanly.

```sh
# Python
swig -python -o aether_config_wrap.c runtime/aether_config.i
gcc -fPIC -shared $(python3-config --includes) aether_config_wrap.c \
    libconfig.so $(python3-config --ldflags) -o _aether_config.so

# Java
swig -java -package com.example.aether runtime/aether_config.i

# Ruby, Go, C#, ... same pattern with -ruby, -go, -csharp.
```

The generated bindings wrap `AetherValue*` as a proxy class per target
language — Python gets `AetherValue`, Java gets a `class AetherValue`,
etc. Users see idiomatic API calls; the opaque pointer is hidden.

See `tests/integration/emit_lib_swig/` for a worked Python round-trip.

## What's out of scope for v1

1. **Callbacks held live** — a host can't pass a closure into Aether and
   have it retained. If you want this (the ARexx / rules-engine model),
   track the "Shape B" design note in
   [aether-embedded-in-host-applications.md](aether-embedded-in-host-applications.md).
2. **`--emit=both` for `ae build`** — use two invocations.
3. **`--with=` opt-in stdlib** — capability-empty is strict for v1.
4. **Wall-clock timeout / allocation budget** — if the embedded script
   loops forever it hangs the host.
5. **Deep-recursive `aether_config_free`** — the v1 free only releases
   the root map/list; nested containers leak unless the caller walks
   the tree. In practice scripts build one tree and it's all released
   when the host is done.
6. **Typed returns beyond `void*`** — functions returning `map`/`list`
   come back as `AetherValue*` with no schema. Host knows the shape.

## Working tests

The integration suite under `tests/integration/` covers:

| Test | What it proves |
|---|---|
| `emit_lib/` | Primitive round-trip through `dlopen` |
| `emit_lib_composite/` | Nested map + list round-trip via accessors |
| `emit_lib_lists/` | List-of-ints, list-of-strings, empty list, out-of-range |
| `emit_lib_primitives/` | `long`, `bool`, `float` across the boundary |
| `emit_lib_unsupported/` | Unsupported param types warn + skip stub |
| `emit_lib_banned/` | All five capability-heavy imports rejected |
| `emit_lib_dual_build/` | Same source → exe AND lib via separate invocations |
| `emit_lib_swig/` | SWIG Python round-trip (skips if `swig` missing) |

Run them with the standard `make test-ae` or individually:

```sh
tests/integration/emit_lib_composite/test_emit_lib_composite.sh
```
