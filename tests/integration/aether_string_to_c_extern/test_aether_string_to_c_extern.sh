#!/bin/sh
# Tests that AetherString* values typed `string` are unwrapped to
# their payload bytes before being passed to a naive C extern
# declared `const char*`. Closes #297.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AE="$ROOT/build/ae"

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

cd "$SCRIPT_DIR" || exit 1

if ! "$AE" build probe.ae -o "$tmpdir/probe" >"$tmpdir/build.log" 2>&1; then
    echo "  [FAIL] aether_string_to_c_extern: ae build failed"
    sed 's/^/    /' "$tmpdir/build.log" | head -20
    exit 1
fi

if ! "$tmpdir/probe" > "$tmpdir/run.out" 2>&1 || ! grep -q "^OK$" "$tmpdir/run.out"; then
    echo "  [FAIL] aether_string_to_c_extern: binary ran but did not print OK"
    sed 's/^/    /' "$tmpdir/run.out" | head -10
    exit 1
fi

echo "  [PASS] aether_string_to_c_extern: TYPE_STRING args unwrapped at C extern call sites (#297)"
exit 0
