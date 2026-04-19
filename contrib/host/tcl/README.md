# contrib.host.tcl — Embedded Tcl

## Prerequisites

```bash
# Debian/Ubuntu
sudo apt install tcl-dev

# macOS
brew install tcl-tk

# Verify
pkg-config --cflags tcl    # or tcl8.6, tcl9.0
```

## Build flags

```toml
# aether.toml
[build]
cflags = "-DAETHER_HAS_TCL $(pkg-config --cflags tcl)"
link_flags = "$(pkg-config --libs tcl)"
```

## Usage

```aether
import contrib.host.tcl
tcl.run_sandboxed(perms, "puts \"hello\"")
```
