# Aether as a Configuration Language — v2: Namespaces and Generated Bindings

A design for the next layer above `--emit=lib`: a host application
embedding Aether scripts gets a typed, idiomatic SDK in its own
language (Java, Python, Go, Ruby, ...) generated from a single
manifest written in Aether itself. The host developer never writes
JNI, never writes SWIG `.i` files, never registers callback function
pointers by hand.

> **Status (2026-04-18):** design proposal. The transport layer
> (`--emit=lib`, `aether_config.h`, opaque `AetherValue*`) exists
> locally on branch `emit-shared-lib-v1-draft` but is **not shipped**
> as a standalone PR — it folds into v2. There is intentionally no
> "v1 API" to maintain backward compatibility with.

## Goals and non-goals

**Goals.**

1. The Aether-side surface a *script* author writes is unchanged from
   what they write today — top-level functions, trailing-block DSLs,
   `notify()` for events back to the host. No mention of the host
   language. No FFI ceremony.

2. The host-side surface a *Java/Python/Go/...* developer reads is a
   normal typed library in their language. No `dlopen`, no
   `MemorySegment`, no `ctypes.CDLL`, no manual callback registration.
   Methods, events, fields, named like the Aether functions they wrap.

3. The generated SDK is **discoverable at runtime**. A standard
   `aether_describe()` entry point returns the namespace's manifest —
   typed, machine-readable, embedded in the `.so` — so version
   checks, IDE tooling, and reflective frameworks all work without
   shipping a sidecar JSON.

4. The callback contract follows the Hohpe **claim check** pattern:
   the script emits thin notifications (`notify("OrderRejected", id)`)
   and the host calls back into the script through the same typed
   downcall API to fetch detail. No bidirectional struct marshalling
   in v2.

**Non-goals.**

1. **No Swagger / OpenAPI emission.** The manifest itself, surfaced
   via `aether_describe()`, is the canonical typed contract. Anyone
   wanting to derive Swagger from it can; we don't.

2. **No live closure handles held by the host.** Closures still don't
   cross the boundary. Host registers per-event callbacks (the claim
   check listener) but does not pass anonymous functions into the script.

3. **No "rich struct" boundary marshalling.** Inputs limited to
   primitives, strings, maps, lists, and arrow-typed functions over
   primitives. Bigger payloads go through a `map` and the script walks it.

4. **No hot reload in v2.** A separate concern; if needed, build on
   top.

## The shape, in one example

A **trading** namespace with three scripts that share a single manifest.

### Directory layout

```
trading/
    manifest.ae          # the namespace definition (one per directory)
    placeTrade.ae        # contributes place_trade()
    killTrade.ae         # contributes kill_trade()
    getTicker.ae         # contributes get_ticker(), get_ticker_history()
```

The convention: every `.ae` file in the directory contributes its
top-level public functions to the namespace declared by `manifest.ae`.
A script that lives elsewhere can opt in by `import`ing the manifest
explicitly (the **escape hatch**, described later).

### `trading/manifest.ae`

```aether
import std.host

namespace("trading") {

    // Inputs the host supplies before any script runs. The compiler
    // generates a setter on the host-side SDK for each one.
    input order: map
    input catalog_has: fn(string) -> bool
    input compute_score: fn(map) -> int
    input max_order: int

    // Events scripts can emit. The compiler generates an event-handler
    // registration on the host-side SDK for each one. Carries are
    // restricted to int64 in v2 (the claim-check ID).
    event "OrderPlaced"   carries int64
    event "OrderRejected" carries int64
    event "TradeKilled"   carries int64
    event "UnknownTicker" carries int64

    // Per-target binding descriptors. The generator uses these to
    // emit one host SDK per language.
    bindings {
        java   { package "com.example.trading"; class "Trading" }
        python { module "trading_rules" }
        go     { package "trading" }
    }
}
```

The manifest is itself an Aether file. It uses the existing trailing-block
builder DSL — `namespace()`, `input`, `event`, `bindings`, `java`, `python`,
`go` are all `_ctx`-injected functions in a new `std.host` module. No new
syntax; the manifest participates in the same lexer/parser/typechecker
as every other `.ae` file.

### `trading/placeTrade.ae`

```aether
place_trade() {
    if order.amount < 0 || order.amount > max_order {
        notify("OrderRejected", order.id)
        return
    }
    if !catalog_has(order.ticker) {
        notify("UnknownTicker", order.id)
        return
    }
    score = compute_score(order)
    if score > 80 {
        notify("OrderRejected", order.id)
        return
    }
    notify("OrderPlaced", order.id)
}
```

What's worth noticing about the script:

- It mentions no host language. Nothing about Java, JVM, JNI.
- `order`, `catalog_has`, `max_order`, `compute_score` are referenced
  as if they're ambient — they're declared `input` in the manifest, and
  the namespace runtime supplies them through the `_ctx`-style mechanism
  that already powers the builder DSL.
- `notify(event, id)` is the only host-callback primitive. It's the
  claim check — the host gets the ID, and if it wants the trade detail
  it calls `get_trade()` (or whatever the script exposes) over the
  normal downcall path.

### `trading/killTrade.ae` (sketch)

```aether
kill_trade(trade_id: int64) {
    notify("TradeKilled", trade_id)
}
```

### `trading/getTicker.ae` (sketch)

```aether
get_ticker(symbol: string) {
    return symbol  // stub for the example
}

get_ticker_history(symbol: string, days: int) {
    h = list.new()
    list.add(h, symbol)
    return h
}
```

### What the Java developer writes against the generated SDK

```java
import com.example.trading.Trading;

public class TradingDemo {
    public static void main(String[] args) {
        Trading rules = new Trading();

        // Wire up the host-supplied inputs.
        rules.setMaxOrder(100_000);
        rules.setCatalogHas(ticker -> catalog.contains(ticker));
        rules.setComputeScore(orderMap -> fraudService.score(orderMap));

        // Subscribe to events using the claim-check pattern: thin
        // notification with an ID, host looks up details if it wants.
        rules.onOrderRejected(id -> {
            Trade t = tradeService.getIfAuthorized(id);
            if (t != null) alertService.flag(t);
        });
        rules.onUnknownTicker(id -> alertService.unknownTicker(id));
        rules.onOrderPlaced(id   -> tradeService.persist(id));
        rules.onTradeKilled(id   -> tradeService.markKilled(id));

        // Set the input map and dispatch.
        Map<String,Object> order = Map.of(
            "id",     2322223222L,
            "ticker", "ACME",
            "amount", 50_000);
        rules.setOrder(order);
        rules.placeTrade();

        // Direct downcalls — typed, no callback machinery.
        String s = rules.getTicker("ACME");
        List<String> hist = rules.getTickerHistory("ACME", 30);

        rules.killTrade(2322223222L);

        // Discovery — the runtime contract, embedded in the .so.
        Trading.Manifest m = Trading.describe();
        System.out.printf("Trading SDK: %d functions, %d events%n",
            m.functions().size(), m.events().size());
    }
}
```

What the Java developer didn't have to write:

- A SWIG `.i` file
- A JNI / Panama `MethodHandle`
- A `MemorySegment` allocation, layout, or arena
- Any explicit `dlopen` call
- Any function pointer registration

What's there is a typed Java class with `set*` for inputs, `on*` for
events, and methods named after the Aether functions.

## How it composes with what's there today

The transport layer that already exists (in `emit-shared-lib-v1-draft`):

- `aetherc --emit=lib` produces a `.so`/`.dylib`.
- `aether_<name>(...)` exports for every top-level Aether function.
- `runtime/aether_config.h` accessors for walking returned maps/lists.
- Capability-empty default (no `std.net|http|tcp|fs|os` in lib mode).

Layered on top by v2:

| New piece | Purpose |
|---|---|
| `std.host` Aether module | DSL: `namespace()`, `input`, `event`, `bindings`, `java { }`, etc. — the manifest grammar |
| `notify(event: string, id: int64)` extern | Claim check; the only host-callback primitive |
| Manifest interpreter | Reads `<dir>/manifest.ae`, runs it under a special builder context, returns an in-memory `NamespaceDescription` |
| Embedded discovery struct | Manifest description serialized into a static struct in the `.so`; `aether_describe()` returns it |
| Generator framework | Templates that turn the `NamespaceDescription` into a Java SDK / Python module / Go package |
| New compile entrypoint | `aetherc --emit=namespace <dir>` (or `ae build --namespace <dir>`) — orchestrates the manifest interp + transport-layer compile + binding generation |

The transport layer is not visible to either the script author or the
host developer in v2. They see the namespace SDK; the transport layer
is plumbing underneath.

## The manifest grammar in detail

`std.host` defines the manifest builder. Every form below is a function
call (or builder block) — pure Aether, no new lexer or parser work.

```aether
import std.host

namespace(name: string) {
    // name becomes the runtime namespace identifier.
    // Used for the discovery struct, generator output naming, etc.

    input <name>: <type>
        // Declares a host-supplied value or function the namespace
        // makes ambient inside its script files.
        //
        // Allowed types in v2:
        //   primitives: int, long, float, bool, string
        //   composites: map, list
        //   functions:  fn(<primitive-or-map>...) -> <primitive-or-map>
        //
        // Each input becomes a `set<Name>(...)` method on the host SDK.

    event "<EventName>" carries <type>
        // Declares an event the script may emit.
        // v2: `carries` is restricted to `int64` (the claim-check ID).
        // Each event becomes an `on<EventName>(handler)` method
        // on the host SDK.

    bindings {
        java   { package "..."; class "..." }
        python { module  "..." }
        go     { package "..." }
        // ... one builder per supported target language
    }
}
```

The DSL choices are conservative: every form is already expressible
through Aether's existing trailing-block + `_ctx` mechanisms (same
shape as TinyWeb's `path()`/`end_point()`). No grammar extension.

## Namespace membership

**Convention (default).** Every `.ae` file in the same directory as
`manifest.ae` contributes its top-level functions to the namespace.

```
trading/
    manifest.ae      # declares namespace("trading")
    placeTrade.ae    # place_trade() automatically in namespace
    killTrade.ae     # kill_trade() automatically in namespace
    getTicker.ae     # get_ticker(), get_ticker_history() automatically in namespace
```

**Escape hatch (explicit `import`).** A script that lives elsewhere can
opt into a namespace by importing its manifest:

```aether
// some/other/path/extra_rules.ae
import trading.manifest    // explicit join — picks up inputs / events

extra_check() {
    if order.amount > 1_000_000 {
        notify("OrderRejected", order.id)
    }
}
```

Importing the manifest pulls in its `input` and `event` declarations
into the script's scope, exactly as if the file lived in the namespace
directory. It also tells the generator: "include my exported functions
in the namespace SDK."

**Opt-out for sibling files (annotation).** A `.ae` file in the namespace
directory that does **not** want to be part of the namespace marks itself:

```aether
// trading/internal_helpers.ae
@private_to_file

shared_constant_x() { return 42 }
```

`@private_to_file` (or whatever annotation we settle on) keeps the
file's functions out of the generated SDK.

## The claim-check callback model

Aether-side primitive in `std.host`:

```aether
extern notify(event: string, id: int64) -> int    // 0 = no listener, 1 = delivered
```

C-side dispatch table (sketch):

```c
// runtime/aether_host.c
typedef void (*aether_event_handler_t)(int64_t id);

static struct {
    const char* name;
    aether_event_handler_t handler;
} g_event_table[64];
static int g_event_count = 0;

void aether_event_register(const char* name, aether_event_handler_t h) {
    g_event_table[g_event_count++] = (struct ...){ name, h };
}

int notify(const char* event, int64_t id) {
    for (int i = 0; i < g_event_count; i++) {
        if (strcmp(g_event_table[i].name, event) == 0) {
            g_event_table[i].handler(id);
            return 1;
        }
    }
    return 0;
}
```

The host SDK's `on<EventName>(handler)` calls `aether_event_register`
under the covers. SWIG's function-pointer typemaps handle the per-language
trampoline (Consumer<Long>, callable[[int], None], etc.) — and **that's
the only place SWIG appears**. The script and the host developer don't
see SWIG syntax.

Why claim check and not richer callbacks:

- **Marshalling.** `notify(name, id)` crosses the boundary as
  `(const char*, int64_t)` — two primitives. No struct marshalling.
- **Auth and freshness on the host side.** The host's `getIfAuthorized(id)`
  decides whether the listener is allowed to see this trade, and gets
  current state. The script doesn't know or need to know.
- **Decoupled lifecycles.** Add a new listener without touching the script.
- **Folklore.** Hohpe's claim check pattern, EAI book, decades of
  message-queue experience converging on this. We're not inventing.

When you genuinely need richer host → script communication, the
script's typed downcall functions (`get_ticker(symbol)`, `kill_trade(id)`)
already give you that — the host calls them directly, no events involved.

## The discovery method

The manifest description is serialized into a static struct embedded in
the produced `.so`. A standard entry point exposes it:

```c
// emitted into every namespace .so
typedef struct AetherInputDecl {
    const char* name;
    const char* type_signature;   // "int", "fn(string)->bool", "map", ...
} AetherInputDecl;

typedef struct AetherEventDecl {
    const char* name;
    const char* carries_type;     // "int64"
} AetherEventDecl;

typedef struct AetherFunctionDecl {
    const char* name;
    const char* signature;        // "(symbol: string) -> string"
} AetherFunctionDecl;

typedef struct AetherNamespaceManifest {
    const char* namespace;
    const char* schema_version;   // semver of the manifest format
    int input_count;     const AetherInputDecl*    inputs;
    int event_count;     const AetherEventDecl*    events;
    int function_count;  const AetherFunctionDecl* functions;
} AetherNamespaceManifest;

const AetherNamespaceManifest* aether_describe(void);
```

What this gets us:

- **Runtime contract verification.** When the host loads the `.so`, it
  can confirm "the loaded namespace is `trading` v3" before calling
  anything. Catches the "wrong `.so` deployed" class of bug at startup
  rather than at first call.

- **Reflective tooling.** IDE plugins, doc generators, contract testers
  walk the manifest. No need to ship a sidecar JSON or re-parse the
  Aether source.

- **Convertible to other contract formats.** A 50-line program walks
  the manifest and emits Swagger, gRPC `.proto`, GraphQL SDL, JSON
  Schema, or whatever's needed downstream. We don't ship those
  emissions ourselves; the manifest is canonical.

The discovery struct is also surfaced on each host SDK as a typed
convenience:

```java
Trading.Manifest m = Trading.describe();
m.events().forEach(e -> System.out.println(e.name() + " carries " + e.carries()));
```

## The generator framework

Conceptual flow when the user runs `ae build --namespace trading/`:

1. Find `trading/manifest.ae`.
2. Run it under a manifest-only typechecker that resolves the
   `namespace()` block into an in-memory `NamespaceDescription`.
3. For every other `.ae` in the directory (and any explicit
   `import trading.manifest`), typecheck its top-level functions
   against the manifest's `input` declarations — `order`, `max_order`,
   etc. resolve from the manifest.
4. Compile every `.ae` (manifest excluded) via the existing `--emit=lib`
   pipeline into one `.so`/`.dylib`.
5. Embed the serialized manifest as the static `AetherNamespaceManifest`
   struct; emit `aether_describe()`.
6. For each declared `bindings { java { ... } }` block, run the
   corresponding generator template:
   - **Java:** template the SDK class, the input setters, the event
     handlers (`Consumer<Long>` typed), the typed return DTOs from any
     functions returning maps/lists.
   - **Python:** template a module with the same shape.
   - **Go:** ditto, with cgo build constraints.
7. SWIG runs once per target with an internally-generated `.i` —
   the developer doesn't see it. SWIG's job is restricted to the
   mechanical bit: marshalling primitives, wrapping the opaque
   `AetherValue*` proxy class, generating function-pointer typemaps
   for `notify` listeners.

The generator is the new piece. It's a templating exercise on top of
the data structures the manifest interpreter already produces. Most
of the language-specific complexity is squeezed into the templates;
the framework is a few hundred lines.

## Test strategy

A namespace builds out into a lot of integration surface. The test
strategy mirrors what we did for v1, scaled up.

| Test | What it proves |
|---|---|
| `tests/integration/namespace_basic/` | manifest interpreter resolves a 1-script namespace correctly |
| `tests/integration/namespace_multifile/` | 3 sibling scripts contribute to one namespace; SDK exposes all functions |
| `tests/integration/namespace_escape_hatch/` | `import trading.manifest` from a non-sibling script joins the namespace |
| `tests/integration/namespace_private/` | `@private_to_file` keeps a sibling out of the SDK |
| `tests/integration/namespace_inputs/` | Each input type (primitive, map, fn) makes it through the boundary |
| `tests/integration/namespace_notify/` | `notify(event, id)` reaches a registered handler; missing handler returns 0 |
| `tests/integration/namespace_describe/` | `aether_describe()` round-trips the manifest faithfully |
| `tests/integration/namespace_java_smoke/` | Generated Java SDK compiles + a tiny Java program calls into it (skips if no JDK) |
| `tests/integration/namespace_python_smoke/` | Same for Python (skips if no python3-dev) |
| `tests/integration/namespace_trading_e2e/` | Full worked example: manifest + 3 scripts + Java host that exercises every input, every event, every function. Asserts the round-trip end-to-end |
| `tests/integration/namespace_capability_empty/` | A namespace script that imports `std.net` is rejected (inherits from v1) |
| `tests/integration/namespace_versioning/` | Discovery struct embeds a schema_version; mismatched host code can detect it |

Skip-on-missing-toolchain is the same pattern v1 uses — Java/Python
targets skip cleanly if their toolchain isn't installed, so CI works
in minimal environments.

## Worked example, end to end

This is the example I'd ship in the repo as `examples/embedded-java/trading/`.
Showing it inline so the design surface gets pressure-tested by an
actual use case.

### Files

```
examples/embedded-java/trading/
    aether/
        manifest.ae
        placeTrade.ae
        killTrade.ae
        getTicker.ae
    java/
        src/main/java/com/example/trading/TradingDemo.java
        pom.xml
    build.sh
    README.md
```

### `aether/manifest.ae`

```aether
import std.host

namespace("trading") {
    input order: map
    input catalog_has: fn(string) -> bool
    input compute_score: fn(map) -> int
    input max_order: int

    event "OrderPlaced"   carries int64
    event "OrderRejected" carries int64
    event "TradeKilled"   carries int64
    event "UnknownTicker" carries int64

    bindings {
        java { package "com.example.trading"; class "Trading" }
    }
}
```

### `aether/placeTrade.ae`

```aether
place_trade() {
    if order.amount < 0 || order.amount > max_order {
        notify("OrderRejected", order.id)
        return
    }
    if !catalog_has(order.ticker) {
        notify("UnknownTicker", order.id)
        return
    }
    score = compute_score(order)
    if score > 80 {
        notify("OrderRejected", order.id)
        return
    }
    notify("OrderPlaced", order.id)
}
```

### `aether/killTrade.ae`

```aether
kill_trade(trade_id: int64) {
    notify("TradeKilled", trade_id)
}
```

### `aether/getTicker.ae`

```aether
get_ticker(symbol: string) {
    return symbol
}

get_ticker_history(symbol: string, days: int) {
    h = list.new()
    list.add(h, symbol)
    return h
}
```

### `build.sh`

```sh
#!/bin/sh
ae build --namespace aether/ -o build/libtrading.so
# Side-effects also produced into build/:
#   build/libtrading.so          — the namespace .so
#   build/com/example/trading/Trading.java   — the generated Java SDK
cd java && mvn -q package
```

### `java/.../TradingDemo.java`

(Same as the worked Java example earlier in this doc.)

### What the test asserts

```sh
# tests/integration/namespace_trading_e2e/test.sh (sketch)
cd examples/embedded-java/trading
./build.sh
java -cp java/target/trading-1.0.jar:java/target/classes com.example.trading.TradingDemo > out.txt
grep -q "OrderPlaced for 2322223222" out.txt
grep -q "TradeKilled  for 2322223222" out.txt
grep -q "Discovered: 4 functions, 4 events" out.txt
```

Skips cleanly if `mvn` and `java` aren't installed.

## Implementation roadmap

Roughly six chunks, in dependency order. Each is ideally a separate PR
unless they're tiny.

| Chunk | Effort | What ships |
|---|---|---|
| 1. `notify(event, id)` extern + dispatch table | Small (~1 day) | The claim-check primitive, exposed through a new `std.host` module |
| 2. Manifest grammar (`namespace()`, `input`, `event`, `bindings`) as Aether DSL | Small-Medium (~2-3 days) | New `std.host` builder functions; manifest files parse and produce in-memory descriptions |
| 3. Namespace compile pipeline | Medium (~3-5 days) | `ae build --namespace <dir>`: walk dir, run manifest, typecheck siblings, link `--emit=lib`, embed discovery struct, emit `aether_describe()` |
| 4. Java generator template | Medium (~3-5 days) | `bindings { java { ... } }` produces a typed `Trading.java` with setters, event handlers, function methods, and the `Manifest` accessor. Internally uses SWIG for the function-pointer typemaps |
| 5. Python generator template | Small (~1-2 days) | Mirrors the Java path with Python idioms |
| 6. Worked example + e2e tests | Medium (~2-3 days) | `examples/embedded-java/trading/` plus the test directories above |

Order:

1. Land chunks 1 + 2 together (small) — proves the manifest + claim
   check work in isolation.
2. Land chunk 3 (the build pipeline) once the data structures it
   feeds on are stable.
3. Land chunk 4 (Java). Demands the most generator work; pays for
   itself by validating the design end to end.
4. Chunks 5 and 6 come after Java is solid.

Total: roughly **3 weeks of focused work**, distributed across 4-5 PRs.

## Open questions

A handful of decisions I'd like to settle when chunks 1-3 land but
which don't block the design:

1. **Manifest discovery strategy.** Hard-coded filename
   `manifest.ae`? Or look for any file containing a top-level
   `namespace(...)` call? First is simpler; second is more flexible.

2. **Multiple manifests in one directory.** Banned, allowed-with-
   warning, or allowed-and-each-defines-a-separate-namespace? I'd
   default to "banned" until there's a use case.

3. **What `aether_describe()` returns when the same `.so` is loaded
   twice in one process.** Probably fine since it's static state, but
   worth confirming before someone tries it.

4. **How the event handler gets called when the script is on a
   different thread than the host's main loop.** The runtime is
   single-threaded today and `notify` is synchronous, so v2 inherits
   that — events fire on whatever thread is currently running Aether
   code. Document and move on; revisit if multi-threading lands.

5. **Annotation syntax for `@private_to_file`.** Aether doesn't have
   annotations today. Alternatives: a magic file `.aether-private`,
   a leading underscore convention, or a manifest-level
   `exclude ["internal_helpers.ae"]`. The last is probably cleanest
   because it keeps the policy in the namespace owner's hands.

## Summary

v2 takes the typed transport layer from `--emit=lib` and adds:

- **A namespace-level manifest written in Aether**, defining inputs,
  events, and per-language bindings once for a directory of related
  scripts.
- **A claim-check callback model** (`notify(event, id)`) that sidesteps
  rich-struct boundary marshalling by deferring all detail fetching to
  the host's typed downcalls into the script.
- **A discovery method** (`aether_describe()`) embedded in every
  generated `.so`, exposing the manifest as a typed runtime contract.
- **A generator framework** that templates per-language SDKs from the
  manifest, hiding SWIG, JNI, ctypes, cgo, and all FFI plumbing from
  both the script author and the host developer.

The result: a Java developer consuming an Aether-built `trading.so`
sees a typed `com.example.trading.Trading` class with `setOrder`,
`onOrderPlaced`, `placeTrade`, `getTicker`, and `Trading.describe()`.
They write no glue. The script author writes plain Aether and a
manifest. Neither side knows about the boundary.

The transport layer (`--emit=lib`, `aether_config.h`, opaque
`AetherValue*`) folds in as the foundation. There is no v1 to maintain
backward compatibility against — v2 is the first shipped surface.
