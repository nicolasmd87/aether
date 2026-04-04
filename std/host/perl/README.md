# std.host.perl — Embedded Perl

## Prerequisites

Perl dev files typically ship with the perl package itself.

```bash
# Debian/Ubuntu — usually already installed
sudo apt install perl

# Verify
perl -MExtUtils::Embed -e ccopts
perl -MExtUtils::Embed -e ldopts
```

## Build flags

```toml
# aether.toml
[build]
cflags = "-DAETHER_HAS_PERL $(perl -MExtUtils::Embed -e ccopts)"
link_flags = "$(perl -MExtUtils::Embed -e ldopts)"
```

## Usage

```aether
import std.host.perl
aether_perl.run_sandboxed(perms, "print \"hello\\n\"")
```

## Notes

Function names are prefixed `aether_perl_` to avoid collision with
Perl's own `perl_run`/`perl_init` symbols.
