#!/bin/sh
# Tests the @c_callback FFI annotation (#235). The annotation marks an
# Aether function as having a stable, externally-visible C symbol so it
# can be passed across the linkage boundary as a function pointer to C
# externs that expect callbacks (HTTP route handlers, signal handlers,
# qsort comparators, libcurl callbacks, …).
#
# What this test asserts:
#  1. Bare `@c_callback` works — Aether name == C symbol, fn-pointer
#     dispatch via shim returns the expected value.
#  2. Explicit `@c_callback("c_name")` works — the C symbol named in
#     the annotation is what the linker resolves; Aether-side name
#     stays unchanged in user code.
#  3. Pointer equality across two reference sites — identifier-as-
#     value resolution emits the same C symbol every time.
#  4. Direct Aether-side call still works — annotation is additive,
#     the regular call path is unchanged.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AE="$ROOT/build/ae"

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

cd "$SCRIPT_DIR" || exit 1

build_log="$tmpdir/build.log"
if ! "$AE" build probe.ae -o "$tmpdir/probe" >"$build_log" 2>&1; then
    echo "  [FAIL] c_callback: ae build failed — annotation not honored"
    sed 's/^/    /' "$build_log" | head -20
    exit 1
fi

if ! "$tmpdir/probe" > "$tmpdir/run.out" 2>&1 || ! grep -q "^OK$" "$tmpdir/run.out"; then
    echo "  [FAIL] c_callback: binary ran but did not print OK"
    sed 's/^/    /' "$tmpdir/run.out" | head -10
    exit 1
fi

echo "  [PASS] c_callback: @c_callback exposes Aether functions as C function pointers"
exit 0
