# contrib.host.tinygo — In-Process Go via TinyGo c-shared

Run TinyGo-compiled Go code inside the Aether process — no
subprocess, no IPC, no marshalling. Just `dlopen` the `.so` /
`.dll` that `tinygo build -buildmode=c-shared` produces and call
its exported functions directly.

This is the in-process counterpart of [`contrib/host/go`](../go/),
which spawns the standard `go` toolchain as a sandboxed
subprocess. Pick the host that matches your containment story:

| | `contrib/host/go` | `contrib/host/tinygo` |
|---|---|---|
| Toolchain | Standard `go` | `tinygo` |
| Process model | Subprocess (LD_PRELOAD sandbox) | In-process (`dlopen`) |
| Latency per call | Process spawn (~ms) | Direct function call (~ns) |
| Memory model | Separate heap | Shared heap (TinyGo + Aether) |
| Goroutines | Full Go runtime | TinyGo's reduced runtime |
| Sandbox enforcement | Per-call (libc grants) | Per-process (one-time grant) |

## Prerequisites

```bash
# macOS
brew install tinygo

# Debian/Ubuntu — see https://tinygo.org/getting-started/install/linux/
wget https://github.com/tinygo-org/tinygo/releases/download/v0.34.0/tinygo_0.34.0_amd64.deb
sudo dpkg -i tinygo_0.34.0_amd64.deb

# Verify
tinygo version
```

## Build the Go side

Mark each function you want to call from Aether with a `//export`
comment. The function name on the Aether side matches the
exported C symbol exactly (case-sensitive, no name mangling).

```go
// greet.go
package main

import "C"  // required by -buildmode=c-shared

//export Answer
func Answer() int32 { return 42 }

//export Add
func Add(a, b int32) int32 { return a + b }

//export Greet
func Greet(name *C.char) *C.char {
    msg := "hello, " + C.GoString(name)
    return C.CString(msg)  // Go-allocated, leaked unless freed (see notes)
}

func main() {}  // c-shared still requires a main() — empty body is fine
```

Build:

```bash
tinygo build -buildmode=c-shared -o libgreet.so greet.go     # Linux
tinygo build -buildmode=c-shared -o libgreet.dylib greet.go  # macOS
tinygo build -buildmode=c-shared -o libgreet.dll greet.go    # Windows
```

## Call from Aether

```aether
import contrib.host.tinygo

main() {
    handle, err = tinygo.load("./libgreet.so")
    if handle == null {
        println("load failed: ${err}")
        return
    }

    answer = tinygo.call_int_void(handle, "Answer")
    println("Answer = ${answer}")          // -> Answer = 42

    total = tinygo.call_int_int_int(handle, "Add", 2, 40)
    println("Add(2, 40) = ${total}")       // -> Add(2, 40) = 42

    msg = tinygo.call_str_str(handle, "Greet", "world")
    println(msg)                           // -> hello, world

    tinygo.unload(handle)
}
```

## Calling-convention surface

v1 ships pre-defined wrapper signatures for the most common
shapes:

| Aether call | Matches TinyGo c-shared signature |
|---|---|
| `tinygo.call_int_void(h, "F")` | `int F(void)` |
| `tinygo.call_int_int(h, "F", a)` | `int F(int)` |
| `tinygo.call_int_int_int(h, "F", a, b)` | `int F(int, int)` |
| `tinygo.call_void_int(h, "F", a)` | `void F(int)` |
| `tinygo.call_str_str(h, "F", s)` | `const char* F(const char*)` |

Adding a new shape is a one-line C extension in
[`aether_host_tinygo.c`](aether_host_tinygo.c) plus a matching
`extern` + wrapper in [`module.ae`](module.ae). Patches welcome.

Fully-dynamic dispatch (libffi) is intentionally out of scope for
v1: libffi is a system dependency 95% of users do not need, and
covering 80% of real call sites with five fixed shapes keeps the
contrib module dependency-free.

## Memory ownership

TinyGo's `C.CString(...)` allocates with `malloc` on the C heap
and is **not garbage-collected** by the Go runtime. Without an
explicit `C.free`, every call leaks. v1 of this module accepts
the leak for short-lived demos; for long-running programs, expose
a `Free(p *C.char)` from the Go side and call it from Aether.

Pointers returned from TinyGo are valid until the next call into
the library on the same handle, or until `tinygo.unload(handle)`.
Copy via `string_concat(s, "")` if you need to outlive that
window.

## Limitations

- **Single Go runtime per process.** `dlopen` of two distinct
  TinyGo c-shared libraries in the same process can collide on
  runtime state. Stick to one library per Aether process.
- **No goroutines that outlive the Aether call.** TinyGo's
  scheduler runs cooperatively inside the call; spawning a
  goroutine that blocks on I/O and returns to the Aether caller
  before the goroutine completes is undefined behaviour.
- **Standard `go` toolchain produces a different ABI.** Building
  with `go build -buildmode=c-shared` (not `tinygo`) embeds the
  full Go runtime, which conflicts with Aether's actor scheduler.
  Use `contrib/host/go` (subprocess) for full Go.
- **Symbol export is `//export Name`.** TinyGo does not export
  package-level `Name` automatically — you must annotate.

## See also

- [`contrib/host/go/`](../go/) — full Go via subprocess + LD_PRELOAD
  sandbox.
- [`std/dl/module.ae`](../../../std/dl/module.ae) — the
  cross-platform `dlopen` shim this host sits on top of.
- [TinyGo c-shared docs](https://tinygo.org/docs/guides/cgo/)
