# std.host.ruby — Embedded CRuby

## Prerequisites

```bash
# Debian/Ubuntu
sudo apt install ruby-dev

# Verify
pkg-config --cflags ruby    # or ruby-3.1, ruby-3.2, etc.
```

## Build flags

```toml
# aether.toml
[build]
cflags = "-DAETHER_HAS_RUBY $(pkg-config --cflags ruby)"
link_flags = "$(pkg-config --libs ruby)"
```

## Usage

```aether
import std.host.ruby
ruby.run_sandboxed(perms, "puts 'hello'")
```
