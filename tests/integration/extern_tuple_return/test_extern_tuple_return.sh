#!/bin/sh
# Tests extern declarations with tuple return types (#271). The shim
# defines three C functions returning struct-by-value with layouts
# matching the codegen-synthesised `_tuple_<T1>_<T2>` typedef:
#   - (int, int)
#   - (int, string)
#   - (ptr, int, string)
# The probe destructures each result at the call site and asserts the
# values flowed correctly across the FFI boundary.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AE="$ROOT/build/ae"

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

cd "$SCRIPT_DIR" || exit 1

build_log="$tmpdir/build.log"
if ! "$AE" build probe.ae -o "$tmpdir/probe" >"$build_log" 2>&1; then
    echo "  [FAIL] extern_tuple_return: ae build failed"
    sed 's/^/    /' "$build_log" | head -20
    exit 1
fi

if ! "$tmpdir/probe" > "$tmpdir/run.out" 2>&1 || ! grep -q "^OK$" "$tmpdir/run.out"; then
    echo "  [FAIL] extern_tuple_return: binary ran but did not print OK"
    sed 's/^/    /' "$tmpdir/run.out" | head -10
    exit 1
fi

echo "  [PASS] extern_tuple_return: tuple-return externs work for (int,int), (int,string), (ptr,int,string)"
exit 0
