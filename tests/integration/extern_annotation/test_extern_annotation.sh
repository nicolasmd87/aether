#!/bin/sh
# Tests the @extern("c_symbol") aether_name(...) FFI annotation (#234).
#
# What the test asserts:
#  1. The Aether-side name (ae_probe_compute) is what the program calls.
#  2. The C symbol that's actually linked is the annotated one (probe_v2)
#     — shim.c only defines probe_v2, not ae_probe_compute. If the call
#     site emits the Aether-side name unchanged, the link fails.
#  3. Multiple annotated externs in one program work (probe_v2 + probe_const).

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AE="$ROOT/build/ae"

fail=0
tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

cd "$SCRIPT_DIR" || exit 1

build_log="$tmpdir/build.log"
if ! "$AE" build probe.ae -o "$tmpdir/probe" >"$build_log" 2>&1; then
    echo "  [FAIL] extern_annotation: ae build failed — annotation not honored"
    sed 's/^/    /' "$build_log" | head -20
    exit 1
fi

if ! "$tmpdir/probe" > "$tmpdir/run.out" 2>&1 || ! grep -q "^OK$" "$tmpdir/run.out"; then
    echo "  [FAIL] extern_annotation: binary ran but did not print OK"
    sed 's/^/    /' "$tmpdir/run.out" | head -10
    exit 1
fi

echo "  [PASS] extern_annotation: @extern(\"c_symbol\") routes calls to the annotated C symbol"
exit $fail
