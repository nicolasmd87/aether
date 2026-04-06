# std.host.python — Embedded CPython

## Prerequisites

```bash
# Debian/Ubuntu
sudo apt install python3-dev

# Verify
python3-config --includes
```

## Build flags

```toml
# aether.toml
[build]
cflags = "-DAETHER_HAS_PYTHON $(python3-config --includes)"
link_flags = "$(python3-config --ldflags --embed)"
```

## Usage

```aether
import std.host.python
python.run_sandboxed(perms, "print('hello')")
```
