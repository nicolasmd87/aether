# std.host.js — Embedded JavaScript (Duktape)

## Prerequisites

```bash
# Debian/Ubuntu
sudo apt install duktape-dev

# Verify
ls /usr/include/duktape.h
```

## Build flags

```toml
# aether.toml
[build]
cflags = "-DAETHER_HAS_JS"
link_flags = "-lduktape"
```

## Usage

```aether
import std.host.js
js.run_sandboxed(perms, "print('hello')")
```

## Notes

Duktape implements ES5.1. No ambient capabilities — all functions
(env, readFile, fileExists) are native bindings with sandbox checks.
No LD_PRELOAD needed. Purest containment model.
