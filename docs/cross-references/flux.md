<!-- Source: GitHub issue #335 — Cross-reference: Flux vs. Aether comparison (full menu of features to consider/skip) -->
<!-- Lifted from issue body so the comparison lives next to the code, discoverable for future contributors. -->

# Flux vs. Aether — A Comparison for the Aether Maintainer

Audience: Nic (Aether co-maintainer) and any future LLM contributor working on
Aether (`~/scm/aether`). The point of this document is **not** to argue that
Aether should become Flux. It is to give Aether enough detail about Flux's
design — especially the parts that are genuinely novel — that Aether can
choose, deliberately, what to absorb, what to ignore, and how to express what
it does absorb in Aether's idiom.

References:

- Flux source of truth: `~/scm/flux_ae/Flux/`
  - `docs/Specs/language_specification.md` (~2.6k lines, the canonical spec)
  - `docs/keyword_reference.md`, `docs/operator_reference.md`
  - `docs/Compiler/compiler_architecture.md`, `docs/Compiler/what_works.md`
  - `examples/*.fx`, `src/stdlib/`, `src/compiler/`
- Aether self-notes: `~/scm/aether/LLM.md` (this is the working memory; pair
  with `CHANGELOG.md` `[current]` and `docs/`)

---

## 1. Elevator pitch — same continent, different countries

| | **Flux** | **Aether** |
|---|---|---|
| One-liner | C performance + Python readability with first-class data manipulation | Go ergonomics + Rust capability discipline + Erlang actors, compiles via C |
| License | Proprietary (© Karac Von Thweatt, all rights reserved) | (project owner: Paul; not stated in `LLM.md`) |
| Backend | LLVM IR → Clang → linker | Emit C → cc → linker (no LLVM in the path) |
| Bootstrap host | Python 3.12+ (compiler is Python) | C (compiler `aetherc.c`, single source-of-truth binary) |
| Target audience | Embedded, drivers, network protocols, file formats, game engines | Systems language for tools/services; embedded DSL host; FFI surface for Python/Ruby/Java/C |
| Default safety stance | "High-trust" — programmer is responsible | Capability-empty by default for `--emit=lib`; opt-in via `--with=` |
| Memory model | Manual, stack-default, opt-in `~`-tracked ownership; no GC, no borrow checker | Manual at C runtime layer; ref-counted + arena strings; RAII-via-codegen; no borrow checker |
| Concurrency | Not built-in; OS threads via FFI | Actors first-class (`receive`/`send`, declared message types) |
| Sandbox | None at language level | Three-layer: `--emit=lib` + `--with=`, `hide`/`seal except`, LD_PRELOAD `libaether_sandbox.so` |
| Embedded-DSL story | Hot-patching examples, `~$` codify, but no isolation primitives | `contrib.host.<lang>.run_sandboxed(perms, code)` for Lua/Python/Perl/Ruby/Tcl/JS |
| Generics | Templates without SFINAE | Not a notable feature in `LLM.md` (Aether has tuples, `(value, err)`, `*StringSeq`, but no template prose) |
| Strings | C-pointer-y; `$ident` stringify; f-strings; binary-aware via `data` types | Length-aware, refcounted `AetherString*`; `*StringSeq` for runtime sequences; `string[]` lowers to `const char**` |

**Read this:** the two languages overlap on goal (C-ABI compatible systems
language with better ergonomics), but their *hill to die on* is different.

- Flux dies on the hill of **first-class binary/bit manipulation**:
  endianness as type, alignment as type, `data{N}` arbitrary-width integers,
  bit slicing, `from`-cast, packed structs, hot-patching. Everything else
  serves that.
- Aether dies on the hill of **capability discipline + actor concurrency +
  ABI stability for FFI**: `--emit=lib`/`--with=`, sandbox layers, declared
  message types, stable `aether_<name>` exports.

When evaluating whether Aether should adopt a Flux feature, ask: *does it
pull weight on Aether's hill, or only on Flux's?*

---

## 2. Compilation pipeline

### Flux

```
.fx source
  → fpreprocess.py   (#import, #ifdef, #def, #dir, #warn, #stop)
  → flexer.py        (tokens)
  → fparser.py       (recursive descent + operator precedence climbing → AST)
  → fast.py / ftypesys.py  (type resolution, validation)
  → fc.py / fcodegen.py    (LLVM IR generation)
  → build/<prog>/<prog>.ll
  → clang -c → .o → linker → binary
```

Output modes via `fxc.py`:

- Native exe (default)
- Static library (`--library`, `.a`/`.lib`)
- DOS COM (`-dos -com`, flat 0x0100, ≤64 KB)
- DOS EXE (`-dos`, MZ header)
- 512-byte boot sector (16-bit real mode, `0x55 0xAA` signature)

Compiler is **Python**. Single-file driver: `python fxc.py file.fx`. Imports
resolved by the preprocessor against (relative path → `src/stdlib` →
`#dir`-added paths).

Name mangling: `<qualified>__<name>__<paramcount>__<param_types>__ret_<ret>`,
with `!!` to opt out (FFI).

### Aether

```
.ae source
  → aetherc           (single C binary; lexer + parser + sema + emit-C)
  → generated .c
  → cc → .o → linker → binary
```

Output modes via `aetherc`:

- `--emit=exe` (default; produces `main()` binary)
- `--emit=lib` (produces `.so`/`.dylib` with `aether_<name>` exports;
  capability-empty)
- `--with=fs[,net,os]` to opt in to capabilities under `--emit=lib`

Compiler is **C** (`compiler/aetherc.c`). Build via `make`; binary at
`build/aetherc` is SHA-pinned per commit.

### Reimplementation guidance for Aether

If Aether ever wants Flux's bit-precise types or `from`-cast (sec. 4 below),
the natural insertion point is **the type-resolution stage in `aetherc.c`**,
not codegen — both features are about how a value is *typed*, with codegen
falling out as integer ops + memcpy. Aether emits C, so:

- `data{N}` → C `uintN_t` for power-of-2; for arbitrary N, an `unsigned`
  with explicit bitfield mask on read/write, or `uintleast{ceil(N/8)*8}_t`
  with a width-tracking type tag.
- Endianness-as-type → emit `__builtin_bswap{16,32,64}` at assignment between
  mismatched-endian types. Cheaper than Flux's LLVM approach because C
  already exposes the intrinsics.
- `from`-cast → a memcpy with `static_assert(sizeof(struct X) <=
  byte_array_len)`. One-liner in C.

Flux's preprocessor is its weakest point (a separate Python pass with its own
mini-language: `#import`, `#dir`, `#ifdef`, `#def`, `#warn`, `#stop`). Aether
already has module imports as a first-class language construct (`std.fs`,
`std.string`, ...). Don't regress to text-substitution. If Aether ever needs
conditional compilation, do it as a typed, first-class construct (`@if(arch
== "x86_64") { ... }`), not a `#`-prefixed shadow language.

---

## 3. Lexical surface

Both use C-family syntax with braces. Differences worth knowing:

| Aspect | Flux | Aether |
|---|---|---|
| Statement terminator | `;` everywhere — including after `{}` blocks (`if (...) { ... };`) | `;` at end of statements, no trailing `;` after blocks |
| Comments | (not stated; assume `//` and `/* */`) | (not stated in `LLM.md`) |
| Function declaration | `def name(int x) -> int { ... };` | `func_name(x: int) -> int { ... }` (Go-ish) |
| Param syntax | `int x` (type before name, C-style) | `x: int` (name before type, Go/Rust-style) |
| String interpolation | `f"Value: {x}"` and `i"Computed: {}" : {x*2;}` | (uses string interpolation per `LLM.md`; syntax not specified) |
| Range | `1..10` (inclusive) | (not specified) |
| Trailing closure | (not a thing in Flux) | `f(x) { ... }` attaches to call line; `f(x)\n{ ... }` warns (#286) |

The `;`-after-`}` rule in Flux is unusual and worth flagging — every block
terminator carries a semicolon. Aether should **not** adopt this; it's a
constant friction point for Flux users in the spec.

---

## 4. The type system — Flux's main contribution

This is where Flux earns its "first-class data manipulation" claim. Aether
should read this section twice.

### 4.1 Primitives

| | Flux | Aether |
|---|---|---|
| 8-bit | `byte` (unsigned, configurable via `__BYTE_WIDTH__`), `char` (8-bit, character literal) | (not enumerated in `LLM.md`) |
| 32-bit | `int` (signed), `uint` (unsigned), `float` | `int` exists (used in extern signatures) |
| 64-bit | `long` (signed), `ulong`, `double`, `float` (yes, `float` is 64-bit too — alias for `double`) | `long` is 64-bit on **every** target including MSVC; backed by C `long long` |
| Bool | `bool` (true/false; integers coerce in context, floats coerce iff exactly 1.0/0.0) | (not specified) |
| Void | `void` is a type, a null-pointer literal, AND a falsy value — overloaded | (not specified) |

Flux's `float == double` (both 64-bit) is unusual and worth not copying. C
programmers will trip on it.

### 4.2 The `data{N:A:E}` construct — the headline feature

```flux
unsigned data{32}     as u32;       // 32 bit, unsigned, default alignment, default endian
signed   data{13}     as weirdInt;  // 13 bit signed, two's complement
unsigned data{16::0}  as little16;  // 16 bit, default align, LITTLE-endian (0)
unsigned data{16::1}  as big16;     // 16 bit, BIG-endian (1; the default)
unsigned data{7:8}    as aligned7;  // 7 bit value, byte-aligned (8-bit boundary)
unsigned data{32:16:0} as packed32; // 32 bit, 16-bit aligned, little-endian
```

Three orthogonal axes:

1. **Width** — any positive integer (3, 13, 17, 256). Decays to integer at
   runtime.
2. **Alignment** (optional) — bit boundary. Defaults to "no requirement."
3. **Endianness** (optional) — `0`=little, `1`=big. **Default is big-endian.**

Casting between `data` types is bit reinterpretation. Assignment between
mismatched-endian types triggers automatic byte-swap. Internal arithmetic is
single-model (big-endian); swapping happens at type boundaries only.

`endianof(T)` and `alignof(T)` are first-class queries.

#### Why this matters for Aether

Aether already has `*StringSeq` for runtime sequences and `(value, err)` for
returns. It does not have an answer to "how do I parse a 24-bit big-endian
length field followed by a 4-bit type tag." Right now, an Aether user writing
the equivalent of svn-aether's wire protocol parser must hand-roll shift-and-
mask. Flux's `data{N:A:E}` collapses that to a struct declaration.

If Aether wants this, the minimum viable design:

- Add `data{N}` as a builtin type constructor in `compiler/aetherc.c` type
  table. Width N is a const expression; alignment and endianness are optional
  trailing modifiers.
- At struct layout time, sum `data{N}` widths in declaration order, packing
  tightly. Members spanning byte boundaries are bitfield reads.
- At assignment, if LHS endianness ≠ RHS endianness, emit a `__builtin_bswap`
  call.
- `from` operator (sec. 4.6): emit a memcpy + size-check.

Do **not** copy Flux's `as` keyword for aliasing — Aether already has type
imports. Inline aliasing is fine: `type u32 = data{32}` keeps Aether's
existing aesthetic.

### 4.3 Composite types

| | Flux | Aether |
|---|---|---|
| Struct | Data-only, packed, no methods. Composable via `:`: `struct C : A, B { ... } : D` | (not specified — Aether has structs implicitly via `*StringSeq` etc., declared in C) |
| Object | Has methods, `__init`/`__exit`/`__expr`, traits, static dispatch | Aether is not OO per `LLM.md`; it's actor + procedural |
| Union | Standard C-style overlap | (not specified) |
| Tagged union | `union U { ... } EnumTag;` with `._` discriminant access | Aether's `(value, err)` covers the common case; full tagged unions not in `LLM.md` |
| Enum | C-style named integer constants (currently `int`-typed) | (not specified) |
| Trait | Pure interface contract; static dispatch only; `Trait object MyObj { ... }` | Actor message types are declared, not duck-typed — the closest analogue |
| Tuple | (not native) | Yes — language has tuples |

**Recommendation for Aether:** don't grow objects-with-methods. Aether's
identity is "actor model + procedural + capability discipline." Adding
inheritance-free objects would dilute that without obvious payoff. Traits
(static dispatch interfaces) might earn their keep if used to express
capability bounds (a function takes "anything implementing `Reader`"), but
that's an actor-system question, not a Flux-port question.

### 4.4 Type aliases

Flux: `unsigned data{32} as u32;` — keyword `as` makes new names.

Aether: not a notable construct in `LLM.md`. If added, prefer
`type Name = ...` (Go/Rust standard) over `as` (which collides with C++'s
casting connotation).

### 4.5 Pointers

Flux:
- `int* p = @x;` — `@` is address-of.
- `int v = *p;` — `*` is dereference.
- `int* p = (@)0x4000_0000;` — `(@)` casts integer to pointer (memory-mapped I/O).
- `void` = null pointer literal: `int* p = (int*)void;`.
- Pointer arithmetic: yes (C-like).
- Implicit pointer ↔ integer round-trip: yes.

Aether: pointers exist (extern signatures use them), but `LLM.md` warns the
borrow boundary: "If it smells like `ptr`, assume borrowed; if it smells like
`string` from a builtin, assume ref-counted." Aether's pointer story is
*intentionally* less rich than Flux's because Aether's bet is RAII-via-codegen,
not unrestricted pointer arithmetic.

**Don't import Flux's `(@)` operator into Aether.** It is a hill Flux dies
on (memory-mapped I/O for embedded/driver work). Aether's audience is
services and tools, not ring 0.

### 4.6 `from` — struct-from-bytes cast

```flux
byte[7] bytes = [0x01, 0x00, 0x20, 0x5F, 0x12, 0x34, 0x56];
Packet pkt from bytes;   // zero-cost reinterpret
```

Where `Packet` is:

```flux
struct Packet
{
    data{8}  type;      // 1 byte
    data{16} length;    // 2 bytes
    data{32} timestamp; // 4 bytes
};
```

This is the keystone of Flux's binary-data ergonomics. Combined with the
endianness-as-type rule, parsing a wire-format packet is *one statement*.

Aether's analogue, if added: `let pkt = Packet from bytes` with an implicit
size check. The codegen is `memcpy(&pkt, bytes.data, sizeof(Packet))`. The
hard part is the type system, not the codegen — `data{N}` has to land first.

### 4.7 Bit slicing

```flux
byte x = 55;
x[0``7] = x[7``0];     // reverse 8 bits; double-backtick is the slice op
u2  subset = value[2``5];   // extract bits 2..5
```

Slices into integer-typed values. Range is *inclusive* and uses the
double-backtick operator (`` `` ``).

This is genuinely useful for protocol work. It's also a syntax choice that
will fight the lexer (backticks already mean other things in many tooling
contexts: shells, Markdown, Slack). If Aether adopts the *semantics*,
consider a different surface, e.g. `value[2..=5]` or `bits(value, 2, 5)`.

### 4.8 Templates / generics

Flux: parametric, no SFINAE, no specialization.

```flux
def foo<T, U>(T a, U b) -> U { ... };
struct Vec<T> { T x, y, z; };
operator<T>(T a, T b)[+] -> T { ... };
```

Single template definition generates all instances; no specialization
override. Type inference at call site.

Aether: `LLM.md` does not describe a generics system. Aether currently uses
typed message-passing and `*StringSeq` for the polymorphism it needs. If
Aether ever adds generics, **stop at non-specialized parametric polymorphism**
— Flux's "no SFINAE, no noise" is a deliberate ceiling and a good one.

### 4.9 Auto / inference

Flux: `auto x = 5;` infers smallest-fitting type. Spec specifically warns
against using it ("Not recommended").

Aether: not stated. Probably let-binding inference (`let x = 5` deduces
`int`) without a separate `auto` keyword.

### 4.10 Endianness as type — engineering details

This is the Flux feature most likely to repay the implementation cost in
Aether. Pin down:

- **Default endianness:** Flux's default is big-endian (network order). For
  Aether, *target-native* would be more consistent with C (the lower runtime
  layer). Pick one and document loudly.
- **Mixed-endian arithmetic:** Flux performs arithmetic in big-endian
  internally. The conversion happens at *assignment*, not at the operator.
  This is the correct rule — doing it at operators forces a swap on every
  read.
- **Marshaling on FFI:** Aether's whole game is FFI-friendly. If a
  `--emit=lib` export takes `data{32::0}` (little-endian), what does the C
  caller see? Probably: a plain `uint32_t` (native), with the swap implicit
  inside the Aether function body. Document this in `docs/emit-lib.md` if
  added.

---

## 5. Memory model

### Flux

- **Stack default**, heap explicit: `int x = 5;` is on the stack; `heap int
  x = 5;` calls `fmalloc()` and `x` becomes a pointer.
- **Zero-init by default**, opt out with `noinit`.
- **Allocation keywords:** `stack`, `heap`, `local`, `global`, `register`,
  `singinit` (single-init across function re-entries — used in recursion).
- **Custom allocator:** `fmalloc`/`ffree`. Casting heap pointer to `(void)x`
  frees it.
- **Optional ownership:** prefix `~` ties a value to a scope/owner. Tied
  values must be moved/freed before scope exit. No global enforcement, no
  borrow checker. Escape hatch: `int* raw = @(~owned);` drops tracking.

### Aether

- Stack/heap distinction not loud in `LLM.md`, but:
  - Strings are ref-counted (`AetherString*`) or arena-owned.
  - `*StringSeq` is structurally shared and refcount-aware.
  - `extern fs_foo_raw() -> string` returns are borrowed from TLS or arena —
    valid only until the next same-kind call.
- RAII-via-codegen: drop-on-scope-exit elsewhere.
- No `~`-style explicit ownership. The split-accessor pattern
  (`fs_try_read_binary` → `fs_get_read_binary` → `fs_release_read_binary`)
  carries ownership in the convention, not the type.

### Reimplementation guidance

Flux's `~` is the most Aether-compatible piece of Flux's memory model: it's
opt-in, simple, and doesn't pretend to be a borrow checker. If Aether ever
wants to upgrade beyond "drop on scope exit," `~` is a cheaper bet than
adding a borrow checker.

But: Flux's full keyword zoo (`stack`, `heap`, `local`, `global`,
`register`, `singinit`, `noinit`) is too much. Aether has been correct to
keep allocation implicit. Only `noinit` (skip zero-init for hot paths) and
maybe `heap` (force heap for an otherwise-small struct) are worth a keyword.

`fmalloc`/`ffree` as a separate allocator from C's `malloc`/`free` is a
correctness footgun — calling C `free` on a Flux heap pointer corrupts the
Flux heap. Don't replicate. Aether should pass through to system `malloc`.

---

## 6. Functions

### 6.1 Definition syntax

```flux
def add(int x, int y) -> int { return x + y; };

// Calling conventions are KEYWORDS that REPLACE `def`:
cdecl     foo(int x) -> int;
stdcall   foo(int x) -> int;
fastcall  foo(int x) -> int;   // the default (configurable)
thiscall  foo(int x) -> int;
vectorcall foo(int x) -> int;
```

Aether: `func_name(x: int) -> int { ... }` with no calling-convention zoo.
The C-emit backend means whatever calling convention the C compiler picks is
what you get. Don't add `stdcall`/`fastcall`/etc. — they pull weight only on
Windows-x86 32-bit, which is not Aether's audience.

### 6.2 Recurse arrow `<~` and `escape`

```flux
def factorial <~ int (int n, int acc)
{
    if (n <= 1) { escape acc; };  // only true exit
    return factorial(n - 1, acc * n);  // every `return` re-enters
};
```

`<~` declares a "strictly recursive" function. The stack frame never grows
(guaranteed TCO). Every `return` re-enters the same function. `escape` is
the only way out, returning a value up the original caller chain.

This is genuinely novel and useful for state machines and interpreters.
Aether's actor model already covers tail-call-shaped workloads (a `receive`
loop is exactly this), so the *use case* is partially covered. But for
non-actor pure-functional tail recursion, Aether has nothing equivalent.

If Aether wants this: it's a single transform in the C-emit backend (`while
(1) { ... }` wrapping, with `return f(args)` rewritten as `args = (...);
continue;` and `escape v` as `return v;`). The hard part is detecting
non-tail uses of `return` and erroring at compile time.

### 6.3 Default params, variadic

Flux: variadic via `...`, accessed as `...[0]`, `...[1]`. Default params not
shown — likely deferred.

Aether: not in `LLM.md`. Probably already supported (variadic at least, for
`printf`-style).

### 6.4 Function contracts

```flux
contract Precondition  { assert(x > 0); };
contract Postcondition { assert(result > 0); };

def divide(int x, int y) -> int : Precondition
{
    return x / y;
} : Postcondition;
```

Pre and post are separate; both expand inline at compile time. **Status:
deferred (`compt`/contract not yet implemented per `what_works.md`).** So
this is Flux's intent, not Flux's reality.

Aether shouldn't chase this. `assert(...)` at function entry/exit covers
99% of the value with no language complexity.

### 6.5 `extern` and `export`

Flux:
```flux
extern { def !!malloc(ulong) -> void*; def free(void*) -> void; };
export { def !!my_lib_func() -> void; };
```

`!!` disables name mangling. `extern` and `export` are mutually exclusive.

Aether's analogue: `--emit=lib` exports `aether_<name>` symbols
automatically. ABI-stable mangling is a feature, not a bug — it's what makes
Python `ctypes` and Java Panama work. Don't add `!!`-style escape hatches;
they break the FFI contract Aether is selling.

### 6.6 Closures / lambdas

Flux: not present. Function pointers cover the role.

Aether: closures *are* present per `LLM.md` ("compiles closures to C"). This
is a real divergence. Closures + capability-restricted grant lists are
Aether's embedded-DSL value proposition. **Keep them; Flux has nothing to
contribute here.**

---

## 7. Control flow

| Construct | Flux | Aether |
|---|---|---|
| `if`/`elif`/`else` | yes; single-statement arms must be wrapped in `{}` | (standard) |
| Switch | `switch(x) { case (1) { ... }; default { ... }; };` — no fallthrough | (not specified; `match` is a reserved word per `LLM.md`) |
| While, do-while, for | C-style + `for (int x in 1..10)` for-in | (not specified) |
| Goto + labels | yes; `label j1:` ... `goto j1;` | (not in `LLM.md`) |
| `jump` to address | yes — `jump 0;`, `jump @func;` (any int = address) | absolutely not (and shouldn't) |
| `try`/`catch`/`throw` | yes — typed catches, `catch (auto e)` for catch-all | not in `LLM.md`; Aether uses `(value, err)` |
| `defer` | yes — LIFO at scope exit | not in `LLM.md`; covered by RAII-via-codegen |
| `assert` | yes; throws inside try/catch, stderr otherwise | (standard) |
| `noreturn` | yes; emits LLVM `unreachable` | (not specified) |
| Pattern matching | partial via switch on tagged union `._` | not specified, but `match` is reserved (so planned) |

Aether's `(value, err)` convention plus `match` as a reserved word suggests
Aether is heading toward Rust/Go-style error returns + pattern matching, not
exceptions. **Stay there.** Flux's `try`/`catch`/`throw` works but is a tax
on every function (stack-unwinding metadata, exception tables) that doesn't
pull weight for Aether's targets.

`jump` to address and `goto/label` are Flux's nod to bootloader/driver
authors. Aether is not in that market.

---

## 8. Objects, traits, and OO

Flux has objects with `__init`, `__exit`, `__expr`, `private`/`public`
blocks, traits as static-dispatch interfaces, no inheritance, composition
via struct prepend/append.

Aether per `LLM.md` is **not OO**. It's procedural + actors + closures. The
closest thing to a method is an actor's message handler.

**Recommendation:** do not bring objects into Aether. Aether's identity is
clearer without them. If there's pressure for "method-like" syntax, add
trait-style static-dispatch *protocols* on top of structs without
introducing a separate `object` declaration.

---

## 9. Operators — Flux's bestiary

Flux has the richest operator set of any production-targeted systems
language I've seen documented. Categories:

- **Arithmetic:** `+ - * / % ^` (`^` is **exponentiation**, not XOR — major
  footgun for C programmers).
- **Logical:** `& | ! !& !| ^^` (with keyword aliases `and or not xor`).
- **Bitwise (backtick-prefixed):** `` `! `& `| `!& `!| `^^ `^^! `` —
  sixteen distinct bitwise ops, all named.
- **Shift:** `<< >>`, with postfix no-arg form (`x<<;` shifts by 1).
- **Comparison:** `== != < <= > >=` and `is` (alias for `==`).
- **Special:** `?? ?= ~ @ (@) <- <~ -> $ ~$ {}* !! !? ..` — covered above
  or below.
- **Custom infix:** `operator (T L, T R) [+++] -> T { ... };` defines a new
  operator. Symbol or identifier (`[NOPOR]`). At least one operand must be a
  user type to overload built-ins.

`^` for exponentiation breaks 50 years of C convention. If Aether were to
adopt anything from this list, `**` for exponentiation (Python) is the
better choice.

`is` for `==` is a small ergonomic win for enum comparisons (`if (state is
ReadyState)`). Cheap to add. Risk: collides with type-checks in
Python/Ruby/JS ("X is instance of Y"). If Aether adds it, document the
semantic clearly.

`<-` chain operator: `result <- validate() <- parse() <- read();` — pipes
right-to-left. Pipe operators are a net win when the pipeline is the
program; net loss when they obscure ordering. Aether's `(value, err)` style
already makes pipelines awkward (you have to handle the err each step), so
adding `<-` doesn't unlock anything until error returns get sugared.

`$ident` (stringify): compile-time identifier-to-string. Useful for
debug-print macros. Cheap to add to Aether if needed.

`~$` (codify): compile/exec a string at runtime. **Don't.** This is
incompatible with Aether's `--emit=lib`/sandbox story by construction —
runtime code-eval is exactly what capability discipline forbids.

**Custom infix operators** are a polarizing feature. Library authors love
them; readers of unfamiliar libraries hate them. Aether's "smart colleague
walks into the room" ethos argues against them. Skip.

---

## 10. Namespaces, modules, preprocessor

Flux: namespaces (`namespace foo { ... }`, reopenable), `using foo::bar;`,
`#import "other.fx";` for textual file inclusion.

Aether: first-class modules (`std.fs`, `std.string`, etc.), no
preprocessor, capability gate at module-import time.

Aether's design is strictly better here. Flux's preprocessor is a
holdover from C-compatible toolchain expectations. Don't regress.

---

## 11. FFI / interop

Flux:
- `extern { def !!malloc(ulong) -> void*; ... };` — declare C functions.
- `!!` to disable name mangling.
- C ABI is the default for `cdecl`-marked functions.
- Struct layout matches C (tight packing unless explicit alignment).
- Calling-convention keywords cover Windows ABIs (`stdcall`, `fastcall`,
  `vectorcall`, `thiscall`).

Aether:
- `--emit=lib` exports `aether_<name>` ABI-stable symbols.
- Auto-generated SDKs from `aether_describe()` output for ctypes/Panama/
  Fiddle.
- `extern fs_foo_raw() -> string` declares C-side functions; `_raw` suffix
  is convention.
- No calling-convention keywords (C compiler picks).

Aether's FFI story is *more* opinionated and *more* productive for the
"build a Python/Ruby/Java SDK" path. Flux's is more flexible if you're
already writing Win32. Different bets, both valid.

---

## 12. Sandbox / capabilities

This is the largest gap.

Flux: **none.** "High-trust" is the explicit design choice. Programmer is
responsible. No isolation primitives.

Aether: three layers (per `LLM.md`):

1. **Compile-time (`--emit=lib` + `--with=`):** stdlib import gate. `std.fs`
   etc. are *banned* under `--emit=lib` unless the build passes
   `--with=fs`.
2. **Lexical (`hide` / `seal except`):** denies enclosing names per block.
3. **Runtime (`libaether_sandbox.so`, LD_PRELOAD):** intercepts libc calls
   against a builder-DSL grant list.

Plus: `contrib.host.<lang>.run_sandboxed(perms, code)` for in-process
embedded interpreters (Lua/Python/Perl/Ruby/Tcl/JS). Same grant list gates
the hosted interpreter's libc calls.

This is Aether's *competitive moat*. Flux can't copy it without changing
identity. Aether should not copy *anything* that weakens it (e.g., do not
adopt `~$` codify; do not add a "low-level escape hatch" to `--emit=lib`).

---

## 13. Concurrency

Flux: nothing built-in. `mandelbrot_mt*.fx` and `web_server.fx` use FFI to
OS threading.

Aether: actors are core. `receive` + `send`, declared message types (not
duck-typed). Erlang-shaped, but with static typing on messages.

Total divergence. No reimplementation guidance — these are different
languages making different bets.

---

## 14. Standard library

Flux's `src/stdlib/`:

```
bigint, collections, console, cryptography (AES, SHA256, HMAC),
decimal, direct2d, dotenv, format, fourier, graphing, gltextanim,
io, json, linux (syscalls), math, net_windows/net_linux,
opengl, physics, random, raycasting, raytracing, regex,
sorting, crc32, detour (function hotpatching), rle, gzip,
fhf (Flux's own format), platform, runtime, builtins, fpm
```

Surface area: ~30+ modules. Heavy on graphics (Direct2D, OpenGL,
raycasting, raytracing) and crypto. This is "kitchen sink"-shaped.

Aether's stdlib (per `LLM.md` examples): `std.fs`, `std.net`, `std.os`,
`std.string`, `std.json`. Smaller surface, capability-gated.

**Recommendation:** don't grow Aether's stdlib to Flux's size. Each module
is a capability surface; each capability surface is a sandbox attack
surface. Smaller stdlib = easier to audit. `--with=` lists stay short.

---

## 15. Notable advanced features (Flux-only)

These are features Flux has that Aether does not — listed so Aether can
decide *deliberately* to adopt or skip.

### 15.1 Hot-patching (`hotpatch_*.fx` examples)

Compile a function on machine A, send the raw bytes over the network,
client jumps into the bytes. First-class code-as-data.

**Aether: skip.** This is the literal opposite of capability discipline. A
sandbox that allows arbitrary inbound machine code is not a sandbox.

### 15.2 Bit slicing across struct boundaries

`x[2``5]` extracts bits 2–5 from any integer or struct member. Slices can
span fields because structs are packed.

**Aether: maybe.** If `data{N}` lands, slicing is a small extension and
useful. Use `..=` syntax instead of double-backtick.

### 15.3 `from`-cast (struct from bytes)

Already covered. **Aether: yes, if `data{N}` lands.**

### 15.4 `singinit` (function-scoped, single-initialized)

```flux
def counter() -> int { singinit int n = 0; return ++n; };
```

Like C `static` inside a function, but loud about being initialized once
even across recursion.

**Aether: maybe.** Useful for memoization. Easy to express as a
top-level let if Aether has package-private bindings.

### 15.5 Compile-time `compt` blocks

Spec-defined, not yet implemented:
```flux
compt {
    def test1() -> void { global def MY_MACRO 1; };
    if (!def(MY_MACRO)) { test1(); };
};
```

Closer to Zig's `comptime` than C++'s `constexpr`.

**Aether: skip until needed.** Compile-time evaluation is a feature with
enormous design surface (Zig spent years on theirs). Don't open it without
a concrete use case Aether actually has.

### 15.6 Custom calling conventions

Listed in sec. 6.1. **Skip.** C-emit means whatever C does.

### 15.7 DOS / bootloader output modes

`fxc.py -dos`, 512-byte boot sector. **Skip.** Not Aether's audience.

### 15.8 Stringify (`$ident`)

Compile-time identifier-to-string. Useful for debug macros. Easy to add to
Aether if a use case appears (`debug_print($x)` printing `"x = 42"`).

### 15.9 List comprehensions

```flux
int[10] squares = [x ^ 2 for (int x in 1..10)];
int[n]  evens = [x for (int x in 1..100) if (x % 2 == 0)];
```

Pythonic, nice. Aether could lift this verbatim if it adds array literals.

### 15.10 String interpolation: f-strings and i-strings

```flux
string s = f"Value: {x}\0";              // f-string: variable interp
string s = i"Computed: {}" : {x*2;};     // i-string: indexed expr eval
```

Aether already has string interpolation per `LLM.md`. Specifics not
listed. The i-string variant (deferred expression eval) is an extra; not
sure it earns the second syntax form.

---

## 16. Reserved keywords

Flux full list (deferred items marked):

```
alignof  and  as  asm  assert  auto
break  bool  byte
case  catch  cdecl  char  compt(deferred)  const  contract(deferred)  continue
data  def  default  deprecate  do  double
elif  else  enum  escape  export  extern
false  fastcall  float  for  from
global  goto
heap
if  in  int  is
jump  label  lext(deferred)  local  long
namespace  noinit  noreturn  not
object  operator  or
private  public
register  return
signed  singinit  sizeof  stack  stdcall  struct  super(deferred)  switch
this  thiscall  throw  trait  true  try  typeof
uint  ulong  union  unsigned  using
vectorcall  void  volatile
while  xor
```

~85 keywords. For comparison, C has ~32 (C89) to ~44 (C23); Go has 25.
Flux's count reflects its kitchen-sink surface and Windows-ABI ambitions.

Aether's keyword count from `LLM.md` is not enumerated, but the language
description ("Go's ergonomics") implies a smaller set. **Hold the line.**

---

## 17. What Aether should consider importing — ranked

By value-to-cost ratio for Aether specifically:

### Tier 1 — high value, design fits

1. **`data{N:A:E}` bit-precise types.** The single most useful Flux
   feature. Solves a real problem (binary protocol parsing) that Aether
   users will hit (svn-aether already does, with shift-and-mask). Cost:
   moderate (type table + struct-layout extension + bswap insertion at
   assignment).

2. **`from`-cast for struct-from-bytes.** Falls out trivially once
   `data{N}` exists. Kills the boilerplate of byte-by-byte parsing.

3. **`is` as `==` keyword alias.** Pure ergonomic win for enum/state
   comparisons. Cost: 5 lines in the parser.

### Tier 2 — useful, watch the cost

4. **Bit slicing (`x[a..=b]`).** Useful adjunct to `data{N}`. Pick a
   non-backtick syntax.

5. **`<~` recurse arrow / guaranteed TCO.** Fills a gap (non-actor tail
   recursion). Cost: one transform pass.

6. **`~` opt-in ownership.** Cheap upgrade over RAII-only. No borrow
   checker, no global analysis. Marks intent without enforcing it
   globally.

7. **Stringify (`$ident`).** Tiny, useful for debug macros.

### Tier 3 — skip

- Custom calling conventions (`stdcall`, `fastcall`, `thiscall`,
  `vectorcall`).
- DOS / bootloader output modes.
- Hot-patching (incompatible with sandbox).
- `~$` codify (incompatible with sandbox).
- Custom infix operators with arbitrary symbols.
- `^` for exponentiation (collides with C convention).
- `try`/`catch`/`throw` exceptions.
- Object orientation with `__init`/`__exit`/`__expr`.
- `jump` to address, `goto`/`label`.
- Preprocessor (`#import`, `#ifdef`, `#def`).
- Inheritance.
- The `auto` keyword (use let-binding inference instead).
- Calling Flux's stdlib's breadth a goal — keep Aether's small.

---

## 18. Reimplementation sketch — a minimum-viable Flux-data-features-in-Aether

If we wanted to absorb Tier 1 (`data{N}` + `from`-cast + `is`), here's the
shape:

### 18.1 Lexer (`compiler/aetherc.c`)

Add tokens: `DATA`, `FROM`, `IS`, `ALIGNOF`, `ENDIANOF`, `SIGNED`,
`UNSIGNED`. (`SIZEOF` likely already exists.)

### 18.2 Type representation

Extend the type node:

```c
struct Type {
    enum { TY_PRIM, TY_DATA, TY_STRUCT, /* ... */ } kind;
    union {
        struct {
            int    width_bits;   // 1..N
            int    align_bits;   // 0 = no requirement
            int    endian;       // 0=little, 1=big, -1=target-native
            bool   is_signed;
        } data;
        /* ... */
    };
};
```

### 18.3 Parser

```
data_type := ('signed' | 'unsigned')? 'data' '{' INT (':' INT (':' INT)?)? '}'
type_alias := 'type' IDENT '=' type
```

(Use `type` keyword for aliasing, not Flux's `as`.)

### 18.4 Struct layout

When emitting a struct containing `data{N}` members, sum widths in
declaration order. Members crossing byte boundaries need bitfield
read/write helpers in the emitted C. Power-of-2 widths can lower to
`uintN_t` directly.

### 18.5 Endianness conversion

At assignment `lhs = rhs`, if `lhs.type.endian != rhs.type.endian`:

```c
emit(rhs.expr);
if (width == 16) emit("__builtin_bswap16(...)");
if (width == 32) emit("__builtin_bswap32(...)");
if (width == 64) emit("__builtin_bswap64(...)");
// arbitrary widths: bit-by-bit reverse helper in the runtime
```

Arithmetic stays in target-native (or Aether's chosen canonical) endianness;
swap *only* at assignment to a typed location.

### 18.6 `from`-cast

```
from_cast := type IDENT 'from' expr
```

Lower to:

```c
T name;
static_assert(sizeof(T) <= sizeof_arr(expr));
memcpy(&name, (expr).data, sizeof(T));
```

### 18.7 `is` keyword

Parser: treat `is` as alternate spelling of `==`. Done.

### 18.8 Estimated LOC

In the spirit of overestimation: 800–1500 lines added to `aetherc.c` for
Tier 1, plus runtime helpers (~200 lines C) and 3–5 regression tests in
`tests/regression/`. Not a small project, but not a rewrite either.

---

## 19. Where the comparison ends

The thing to remember when reading Flux's spec is that it was designed by
one person, in one head, for problems Karac has encountered. Aether is
designed by you and Nic, for the embedded-DSL host problem and the
Python/Java/Ruby SDK problem. A feature can be technically excellent in
Flux and irrelevant to Aether. The reverse also holds — Aether's actor
syntax and capability layers don't need defending against Flux's
critique because Flux isn't trying to solve those problems.

Use this document as a menu, not a roadmap. The Tier 1 list (sec. 17) is
the only part where I think Aether is leaving value on the table.

— end —

---

## TL;DR for triage

The full doc above is a 950-line survey of Flux (/home/paul/scm/flux_ae) — a separate systems language with overlapping goals — written for the explicit purpose of letting Aether decide deliberately what to absorb, what to skip, and how to express anything we do absorb in Aether's idiom.

**The doc's own recommendation (Section 17):**

- **Tier 1 — high value, design fits**:
  1. `data{N:A:E}` bit-precise types (filed as separate issue — see cross-refs below)
  2. `from`-cast for struct-from-bytes (rides on Tier 1.1)
  3. `is` as `==` keyword alias (5-line parser change; happy to do this as a one-shot PR if you say go)
- **Tier 2 — useful, watch the cost**: bit slicing, `<~` recurse arrow / TCO, `~` opt-in ownership, `$ident` stringify
- **Tier 3 — explicit skip**: custom calling conventions, hot-patching, `~$` codify (incompatible with sandbox), exceptions, OO with `__init`/`__exit`, `jump` to address, preprocessor, etc.

The Tier 1 `data{N}` proposal — the most substantive feature — is filed as its own focused issue with implementation sketch. This umbrella issue exists to keep the framing ("Flux exists, here's what's worth absorbing or not") attached to the repository so future decisions about Flux-shaped features have a reference.

## What this issue is NOT asking

- Not a roadmap proposal. The doc explicitly says "use this as a menu, not a roadmap."
- Not asking for any single feature to ship. Each Tier 1 item should get its own issue / PR with proper review.
- Not litigating Tier 3. Those are correctly identified as incompatible with Aether's identity (sandbox, FFI stability, capability discipline).

## Cross-refs

- Tier 1.1 (`data{N:A:E}`): filed as separate issue, see issues list
- Tier 1.3 (`is` keyword alias): noted inline, await your call
- Authoring context: `~/scm/flux_ae/COMPARISON.md` is the source-of-truth (also pasted above for permanence). Flux's own spec lives at `~/scm/flux_ae/Flux/docs/Specs/language_specification.md`.
