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
library with external linkage. Callers should use the `aether_<name>`
entry points for ABI stability — the un-prefixed names are an
implementation detail and may be hidden in a future version. See
[Symbol visibility matrix](#symbol-visibility-matrix) below for the
full picture across emit modes.

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

Two private-helper conventions also opt out of the `aether_<name>`
surface:

- **Trailing-underscore**: a function whose name ends in `_`
  (e.g. `record_start_`, `helper_`, `parse_line_`) is treated as
  file-local. It's emitted with C `static` storage and gets no
  `aether_*` alias. This lets two `.ae` files in the same project
  declare their own `record_start_` without colliding at link time
  and without leaking either into the public ABI. The convention is
  enforced by codegen, so adding the `_` is enough — no annotation.
- **Tuple-returning helpers**: `helper(n: int) -> { return n, n+1 }`
  returns a `_tuple_int_int` struct that doesn't fit the public ABI
  table. The function itself is fine; the alias is skipped. Wrap
  with a single-value-returning function if it needs to be exposed
  across the library boundary.

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

### Type contract

Aether maps and lists are **untyped** internally — values are stored as
opaque `void*` with no runtime tag. The accessors reinterpret the stored
value as the requested type; they do not verify it. That makes the
script-to-host interface a straight FFI contract: document what the
script writes at each key, and the host must read it back with the
matching accessor. If the script stored an `int` at `"port"` and the
host asks for a string, the host gets garbage — treat this exactly the
way you would treat any other cross-language value exchange.

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

### Opting in: `--with=<capabilities>`

Projects that **are** the host — code that compiles `.ae` and
handwritten C into one binary, rather than embedding Aether as an
untrusted user script — opt into specific capability categories:

```sh
ae build --emit=lib --with=fs           file.ae   # std.fs
ae build --emit=lib --with=net          file.ae   # std.net, std.http, std.tcp
ae build --emit=lib --with=os           file.ae   # std.os
ae build --emit=lib --with=fs,os        file.ae   # multiple, comma-separated
ae build --emit=lib --with=first-party  file.ae   # alias for fs,net,os
ae build --emit=lib --with=all          file.ae   # alias for fs,net,os
```

The gate stays default-deny: a build without `--with=fs` still rejects
`import std.fs`. Unknown capability names are a hard error (typos
shouldn't silently leave a gate closed). The categories mirror the
banned-import groupings above — three buckets, chosen coarsely enough
that opting in is an auditable event in a project's build invocation.

`--with=first-party` and `--with=all` both expand to `fs,net,os`.
The two names are equivalent; pick whichever expresses intent better
in your project. `first-party` reads as "this Aether code is
trusted-as-first-party, give it everything"; `all` reads as a literal
shorthand for the full set. Using either one is appropriate for tools
like build systems and SDK generators where the `.ae` files ship with
the toolchain itself; it is **not** appropriate for plugin / user-
script scenarios — see "When NOT to use this" below.

**When to use this.** Systems code where the `.ae` files are
first-party and version-controlled the same way as the `.c` files.
The Aether-side `file_open_raw` is no different from the C side
calling `fopen` — there's no privilege boundary to police.

**When NOT to use this.** Anywhere the library could run untrusted
Aether (user scripts, a plugin loader, a DSL evaluator). Leave the
default in place; the host mediates I/O on the script's behalf.

## Symbol visibility matrix

What gets `static` storage class versus external linkage in the
generated `.c` file, by function origin × emit mode. This determines
which symbols collide when multiple `.c` files compiled from
different `.ae` sources are linked into one binary.

| Function origin                     | `--emit=exe` | `--emit=lib`                            |
|-------------------------------------|:------------:|:---------------------------------------:|
| Local (defined in this `.ae`)       | external     | external + `aether_<name>` alias        |
| Local with trailing `_` (`helper_`) | **`static`** | **`static`** (no `aether_<name>` alias) |
| Local with tuple return type        | external     | external (no `aether_<name>` alias)     |
| Imported Aether wrapper             | **`static`** | **`static`** (no `aether_<name>` alias) |
| `extern` declaration (any module)   | declaration only — refers to external symbol from `libaether.a` or another TU |
| Stdlib C externs (`std/*/aether_*.c`) | linked from `libaether.a`; no per-TU duplication |

Two consequences worth highlighting:

**Imported Aether wrappers are private to each TU.** When module A
and module B both `import std.string`, both generated `.c` files
contain `static const char* string_copy(...)` — a private copy each.
Linking the two `.o` files together produces no duplicate-symbol
errors, even on linkers that don't support
`-Wl,--allow-multiple-definition` (notably macOS ld64). This is
deliberate; see [`compiler/aether_module.c:1126`](../compiler/aether_module.c)
where `clone->is_imported = 1` is set, and
[`compiler/codegen/codegen_func.c:297`](../compiler/codegen/codegen_func.c)
where the `static` keyword gets emitted.

**Locally-defined functions retain external linkage.** If two `.ae`
files both define a function with the same name (e.g. both define
`helper`), they DO collide at link time. `--emit=lib` adds an
`aether_<name>` alias on top, but the un-aliased local symbol is
still external. This is the symmetric opposite of the imported case
and is intentional — local functions are the unit of inter-TU
linking; the user program structures around their names.

**Implication for downstream tools.** Multi-TU binaries that import
the same SDK module across many `.ae` files do not need
`-Wl,--allow-multiple-definition`. The static-marking on imported
wrappers handles deduplication at the source level. Tools building
this shape can drop the link flag and gain macOS compatibility for
free — `ld64` (Apple's linker) silently rejects
`--allow-multiple-definition`, and the static-marking removes the
need for it.

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

## Per-call resource caps (#343)

Hosts loading untrusted Aether scripts bound the script's
resource use via two TLS-backed knobs in `include/libaether.h`:

```c
#include <libaether.h>

aether_set_memory_cap(64 * 1024 * 1024);   // 64 MiB ceiling
aether_set_call_deadline(50);              // 50 ms wall-clock

call_into_aether();                        // returns inside both bounds

if (aether_deadline_tripped()) {
    /* The deadline fired mid-call. The Aether call returned
     * early via the codegen tripwire. Any partial state is
     * the host's to interpret. */
}
```

**Memory cap.** Process-wide. The cap-aware allocators (`std/string`,
`std/collections`, `runtime/memory/{arena,pool}`) check + account
against a single `_Atomic uint64_t` counter. The counter tracks
**current usage**, not high-water-mark — long-running guests that
allocate-and-free in a loop don't trip the cap on cumulative churn.
Allocations past the cap return NULL through the existing
error-string convention; stdlib functions surface "out of memory:
cap exceeded" up the call chain rather than aborting. Pass `0` to
disable.

**Wall-clock deadline.** Per-thread monotonic (`CLOCK_MONOTONIC`).
The codegen emits `if (aether_caps_deadline_tripped()) {
__aether_abort_call(); break; }` at every `for` / `while` loop head
under `--emit=lib` — not under `--emit=exe` (zero overhead on
non-sandboxed builds). On trip the sticky flag flips; subsequent
loop heads in the same call exit too, so any depth of nested loops
unwinds in O(N) tripwire-bounded breaks. Pass `0` to disable; pass
a fresh ms value to clear the sticky flag and re-arm.

The trip is observable via `aether_deadline_tripped()` — the host
checks after the call to know whether the result was complete or
truncated. Retrying a tripped deadline is the host's policy:
re-arm + call again with a more generous budget, or surface
`Deadline` to the user.

## What's out of scope for v1

1. **Callbacks held live** — a host can't pass a closure into Aether and
   have it retained. If you want this (the ARexx / rules-engine model),
   track the "Shape B" design note in
   [aether-embedded-in-host-applications.md](aether-embedded-in-host-applications.md).
2. **`--emit=both` for `ae build`** — use two invocations.
3. **Per-function capability grants** — `--with=` flags are coarse
   (fs / net / os). Fine-grained gates like "allow file_open but not
   dir_delete" don't match any concrete threat model for the
   default-deny shape, and every additional flag is API surface.
4. **Deep-recursive `aether_config_free`** — the v1 free only releases
   the root map/list; nested containers leak unless the caller walks
   the tree. In practice scripts build one tree and it's all released
   when the host is done.
5. **Typed returns beyond `void*`** — functions returning `map`/`list`
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
| `emit_lib_with_capability/` | `--with=fs,net,os` opt-ins; `--with=first-party` and `--with=all` aliases |

Run them with the standard `make test-ae` or individually:

```sh
tests/integration/emit_lib_composite/test_emit_lib_composite.sh
```
