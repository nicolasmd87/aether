#!/bin/sh
# Regression: contrib.sqlite open + DDL + INSERT + SELECT + close
# against an in-memory database. See probe.ae for the test matrix.
#
# SQLite isn't auto-detected by the Aether toolchain. We probe for
# libsqlite3 via pkg-config (with a gcc-link fallback for systems
# without pkg-config metadata) and SKIP the test when it's absent.
# This matches CONTRIBUTING.md §"Coding for portability" pattern #2.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

TMPDIR="$(mktemp -d)"; trap 'rm -rf "$TMPDIR"' EXIT

# -- Capability probe -----------------------------------------------------
probe_sqlite_available() {
    if pkg-config --exists sqlite3 2>/dev/null; then
        return 0
    fi
    # Last-ditch: try a direct compile in case pkg-config metadata is absent.
    probe_c="$TMPDIR/probe_sqlite.c"
    cat > "$probe_c" <<'EOF'
#include <sqlite3.h>
int main(void) { sqlite3* db; return sqlite3_open(0, &db); }
EOF
    gcc "$probe_c" -lsqlite3 -o "$TMPDIR/probe_sqlite" >/dev/null 2>&1
}

if ! probe_sqlite_available; then
    echo "  [PASS] sqlite_roundtrip: SKIP (libsqlite3 not installed)"
    exit 0
fi

# -- Build ---------------------------------------------------------------
# Stage a workspace with its own aether.toml so the -lsqlite3 link flag
# flows into `ae build`'s gcc invocation via get_link_flags(). The
# workspace symlinks contrib/ so `import contrib.sqlite` resolves.
WORK="$TMPDIR/work"
mkdir -p "$WORK"
ln -s "$ROOT/contrib" "$WORK/contrib"
cp "$SCRIPT_DIR/probe.ae" "$WORK/probe.ae"

cat > "$WORK/aether.toml" <<EOF
[project]
name = "sqlite_probe"
version = "0.0.0"

[[bin]]
name = "probe"
path = "probe.ae"
extra_sources = ["contrib/sqlite/aether_sqlite.c"]

[build]
link_flags = "-lsqlite3"
EOF

if ! ( cd "$WORK" && "$ROOT/build/ae" build "probe.ae" -o "$TMPDIR/probe" \
        >"$TMPDIR/build.log" 2>&1 ); then
    echo "  [FAIL] sqlite_roundtrip: build failed"
    sed 's/^/    /' "$TMPDIR/build.log" | head -20
    exit 1
fi

# `ae build` is known to exit 0 even when the gcc link fails, so verify
# the binary was actually produced before trying to run it.
if [ ! -x "$TMPDIR/probe" ]; then
    echo "  [FAIL] sqlite_roundtrip: build produced no binary"
    sed 's/^/    /' "$TMPDIR/build.log" | head -20
    exit 1
fi

# -- Run -----------------------------------------------------------------
if ! "$TMPDIR/probe" >"$TMPDIR/run.log" 2>&1; then
    echo "  [FAIL] sqlite_roundtrip: probe exited non-zero"
    sed 's/^/    /' "$TMPDIR/run.log" | head -30
    exit 1
fi

if ! grep -q "All contrib.sqlite tests passed" "$TMPDIR/run.log"; then
    echo "  [FAIL] sqlite_roundtrip: didn't reach final PASS line"
    sed 's/^/    /' "$TMPDIR/run.log" | head -30
    exit 1
fi

echo "  [PASS] sqlite_roundtrip: 7 cases"
