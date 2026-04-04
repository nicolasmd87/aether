# std.host.lua — Embedded Lua

## Prerequisites

```bash
# Debian/Ubuntu
sudo apt install liblua5.4-dev   # or liblua5.3-dev

# Verify
pkg-config --cflags lua5.4       # or lua5.3
```

## Build flags

```toml
# aether.toml
[build]
cflags = "-DAETHER_HAS_LUA $(pkg-config --cflags lua5.4)"
link_flags = "$(pkg-config --libs lua5.4)"
```

## Usage

```aether
import std.host.lua
lua.run_sandboxed(perms, "print('hello')")
```
