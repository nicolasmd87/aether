#!/bin/sh
# Regression: contrib.sqlite v2 — prepared statements, parameter
# binding, column reads (incl. blob NUL preservation), reset,
# finalize, changes, errmsg. See probe.ae for the 8-case matrix.
#
# Same pkg-config probe + SKIP as the v1 test (sqlite_roundtrip),
# matching CONTRIBUTING.md §"Coding for portability" pattern #2.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

TMPDIR="$(mktemp -d)"; trap 'rm -rf "$TMPDIR"' EXIT

# -- Capability probe -----------------------------------------------------
probe_sqlite_available() {
    if pkg-config --exists sqlite3 2>/dev/null; then
        return 0
    fi
    probe_c="$TMPDIR/probe_sqlite.c"
    cat > "$probe_c" <<'EOF'
#include <sqlite3.h>
int main(void) { sqlite3* db; return sqlite3_open(0, &db); }
EOF
    gcc "$probe_c" -lsqlite3 -o "$TMPDIR/probe_sqlite" >/dev/null 2>&1
}

if ! probe_sqlite_available; then
    echo "  [PASS] sqlite_prepared: SKIP (libsqlite3 not installed)"
    exit 0
fi

# -- Build ---------------------------------------------------------------
WORK="$TMPDIR/work"
mkdir -p "$WORK"
ln -s "$ROOT/contrib" "$WORK/contrib"
cp "$SCRIPT_DIR/probe.ae" "$WORK/probe.ae"

cat > "$WORK/aether.toml" <<EOF
[project]
name = "sqlite_prepared_probe"
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
    echo "  [FAIL] sqlite_prepared: build failed"
    sed 's/^/    /' "$TMPDIR/build.log" | head -20
    exit 1
fi

if [ ! -x "$TMPDIR/probe" ]; then
    echo "  [FAIL] sqlite_prepared: build produced no binary"
    sed 's/^/    /' "$TMPDIR/build.log" | head -20
    exit 1
fi

# -- Run -----------------------------------------------------------------
if ! "$TMPDIR/probe" >"$TMPDIR/run.log" 2>&1; then
    echo "  [FAIL] sqlite_prepared: probe exited non-zero"
    sed 's/^/    /' "$TMPDIR/run.log" | head -30
    exit 1
fi

if ! grep -q "All contrib.sqlite v2 tests passed" "$TMPDIR/run.log"; then
    echo "  [FAIL] sqlite_prepared: didn't reach final PASS line"
    sed 's/^/    /' "$TMPDIR/run.log" | head -30
    exit 1
fi

echo "  [PASS] sqlite_prepared: 8 cases"
