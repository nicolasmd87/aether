# Aether Package Manifest (aether.toml)

## Overview

The `aether.toml` file is the package manifest for Aether projects. It defines package metadata, dependencies, build configuration, and other project settings.

## Format

The manifest uses TOML format for simplicity and readability.

## Sections

### [package]

Core package information (required).

```toml
[package]
name = "my-web-server"              # Package name (required, lowercase, hyphens)
version = "0.1.0"                    # Semantic version (required)
authors = ["Name <email@example.com>"]  # List of authors
license = "MIT"                      # SPDX license identifier
description = "High-performance web server using Aether actors"
repository = "https://github.com/user/repo"
homepage = "https://example.com"
documentation = "https://docs.example.com"
readme = "README.md"                 # Path to README
keywords = ["web", "server", "actors"]  # Search keywords (max 5)
categories = ["web-programming", "network-programming"]
```

### [dependencies]

Runtime dependencies (packages required for the program to run).

```toml
[dependencies]
aether-http = "1.0"                 # Specific version
aether-json = "0.5.2"               # Specific patch version  
aether-logger = "^0.3"              # Compatible with 0.3.x
aether-db = "~1.2.3"                # Compatible with 1.2.x
aether-utils = { version = "2.0", features = ["async"] }  # With features
local-lib = { path = "../local-lib" }  # Local dependency
git-dep = { git = "https://github.com/user/repo", tag = "v1.0.0" }  # Git dependency
```

#### Version Syntax

- `"1.0.0"` - Exact version
- `"^1.0.0"` - Compatible (>= 1.0.0, < 2.0.0)
- `"~1.2.3"` - Patch compatible (>= 1.2.3, < 1.3.0)
- `">= 1.0, < 2.0"` - Range
- `"*"` - Any version (not recommended)

### [dev-dependencies]

Development dependencies (only needed during development/testing).

```toml
[dev-dependencies]
aether-test = "0.2"                 # Test framework
aether-bench = "0.1"                # Benchmarking tools
aether-mock = "1.0"                 # Mocking library
```

### [build-dependencies]

Dependencies needed during build time only.

```toml
[build-dependencies]
aether-codegen = "0.5"              # Code generation tools
aether-proto = "1.2"                # Protocol buffer compiler
```

### [build]

Build configuration.

```toml
[build]
target = "native"                    # native, wasm, embedded
optimizations = "aggressive"         # none, basic, aggressive
debug-symbols = true                 # Include debug info
strip = false                        # Strip symbols from binary
static-linking = false               # Static vs dynamic linking
```

### [features]

Optional feature flags.

```toml
[features]
default = ["std", "logging"]        # Features enabled by default
std = []                             # Standard library
logging = ["dep:aether-logger"]     # Depends on aether-logger
async = ["std"]                      # Requires std feature
experimental = []                    # Experimental features
```

### [target]

Platform-specific configuration.

```toml
[target.linux]
dependencies = { linux-specific = "1.0" }

[target.windows]
dependencies = { windows-specific = "1.0" }

[target.macos]
dependencies = { macos-specific = "1.0" }

[target.'cfg(unix)']
dependencies = { unix-lib = "1.0" }
```

### [profile]

Build profiles (optimization levels).

```toml
[profile.dev]
optimizations = "none"
debug-symbols = true

[profile.release]
optimizations = "aggressive"
debug-symbols = false
strip = true

[profile.bench]
optimizations = "aggressive"
debug-symbols = true
```

### [workspace]

Multi-package workspace configuration.

```toml
[workspace]
members = [
    "packages/core",
    "packages/server",
    "packages/client"
]
exclude = ["temp", "examples"]

[workspace.dependencies]
shared-dep = "1.0"  # Shared across all members
```

### [scripts]

Custom scripts for common tasks.

```toml
[scripts]
test = "apkg test --verbose"
bench = "apkg bench --all"
docs = "apkg doc --open"
deploy = "./scripts/deploy.sh"
```

### [bin]

Binary targets.

```toml
[[bin]]
name = "server"                     # Binary name
path = "src/main.ae"                # Entry point
required-features = ["std"]         # Required features

[[bin]]
name = "cli"
path = "src/cli.ae"
```

### [lib]

Library configuration.

```toml
[lib]
name = "my_lib"                     # Library name
path = "src/lib.ae"                 # Library entry point
```

### [test]

Test configuration.

```toml
[[test]]
name = "integration"
path = "tests/integration.ae"
harness = true                      # Use built-in test harness
```

### [bench]

Benchmark configuration.

```toml
[[bench]]
name = "performance"
path = "benches/perf.ae"
harness = true
```

### [metadata]

Custom metadata (not used by package manager).

```toml
[metadata]
docs.rs = { features = ["full"] }
custom_field = "value"
```

## Complete Example

```toml
[package]
name = "aether-web-server"
version = "1.0.0"
authors = ["Developer <dev@example.com>"]
license = "MIT"
description = "High-performance HTTP server with actor-based request handling"
repository = "https://github.com/user/aether-web-server"
homepage = "https://aether-web.example.com"
readme = "README.md"
keywords = ["http", "server", "actors", "async"]
categories = ["web-programming", "network-programming"]

[dependencies]
aether-http = "^2.0"
aether-json = "1.5"
aether-logger = { version = "0.8", features = ["json", "async"] }
aether-collections = "~0.3.0"

[dev-dependencies]
aether-test = "0.5"
aether-mock = "0.2"

[build]
target = "native"
optimizations = "aggressive"
static-linking = true

[features]
default = ["logging", "tls"]
logging = ["dep:aether-logger"]
tls = ["dep:aether-tls"]
metrics = ["dep:aether-metrics"]

[profile.release]
optimizations = "aggressive"
debug-symbols = false
strip = true

[[bin]]
name = "aether-server"
path = "src/main.ae"
required-features = ["logging"]

[lib]
name = "aether_web_core"
path = "src/lib.ae"

[[test]]
name = "integration"
path = "tests/integration.ae"

[scripts]
start = "apkg run --release"
test-all = "apkg test && apkg bench"
```

## Validation Rules

1. **Package name**: lowercase, letters/numbers/hyphens only, must start with letter
2. **Version**: Must be valid semantic version (MAJOR.MINOR.PATCH)
3. **License**: Should be valid SPDX identifier
4. **Dependencies**: No circular dependencies allowed
5. **Features**: No circular feature dependencies

## Resolution Algorithm

1. Parse all `aether.toml` files
2. Build dependency graph
3. Resolve versions (newest compatible)
4. Check for conflicts
5. Download required packages
6. Build in topological order

## Lock File (aether.lock)

Generated automatically, contains exact resolved versions:

```toml
# This file is @generated by apkg.
# It is not intended for manual editing.

[[package]]
name = "aether-http"
version = "2.1.3"
source = "registry+https://packages.aetherlang.org"
checksum = "abc123..."
dependencies = ["aether-net"]

[[package]]
name = "aether-net"
version = "1.0.5"
source = "registry+https://packages.aetherlang.org"
checksum = "def456..."
dependencies = []
```

## Future Extensions

- **Patches**: Override dependencies of dependencies
- **Replaces**: Replace one package with another
- **Badges**: Display badges in documentation
- **Include/Exclude**: Control which files are published

