#!/bin/sh
# A heap-string struct field assigned from a heap-tracked local that holds a
# BORROWED value at runtime must move the source's runtime ownership into the
# field tracker, not hard-code it to 1. Hard-coding 1 made the destructor free
# a value the program never owned (a literal -> free of rodata), aborting at
# teardown. This is the closure/FFI-callback shape a downstream HTML sanitizer
# hit: a hook returns a literal, threaded through `-> string` helpers into a
# heap-boxed attr field, double-freed on free.
#
# Running to completion is half the assertion (a double free / rodata free
# aborts with SIGSEGV or SIGABRT). The emitted ownership move is checked
# directly too, so this means something on CI platforms with no leak checker.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AE="$ROOT/build/ae"
AETHERC="$ROOT/build/aetherc"
[ -x "$AE" ] || { echo "  [SKIP] heap_field_borrowed_var_no_double_free: build/ae missing"; exit 0; }

TMP="$(mktemp -d)"
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT

# 1. Generated C moves the source var's runtime tracker into the field, rather
#    than writing a literal 1. The borrowed store must NOT read `= 1`.
if ! "$AETHERC" "$SCRIPT_DIR/prog.ae" "$TMP/out.c" > "$TMP/gen.log" 2>&1; then
    echo "  [FAIL] heap_field_borrowed_var_no_double_free: codegen failed"
    sed 's/^/        /' "$TMP/gen.log" | head -8
    exit 1
fi
if ! grep -q "_heap_value = _heap_borrowed" "$TMP/out.c"; then
    echo "  [FAIL] heap_field_borrowed_var_no_double_free: borrowed store did not move the runtime tracker"
    echo "         expected '..._heap_value = _heap_borrowed; _heap_borrowed = 0;'"
    grep -n "value = borrowed\|_heap_value" "$TMP/out.c" | sed 's/^/        /' | head -6
    exit 1
fi
if grep -q "value = borrowed; ap->_heap_value = 1" "$TMP/out.c"; then
    echo "  [FAIL] heap_field_borrowed_var_no_double_free: borrowed store still hard-codes _heap_value = 1"
    exit 1
fi

# 2. It runs to completion — the actual bug was a teardown abort. Build a real
#    binary and run it; a double free / rodata free crashes here.
if ! "$AE" build "$SCRIPT_DIR/prog.ae" -o "$TMP/prog" > "$TMP/build.log" 2>&1; then
    echo "  [FAIL] heap_field_borrowed_var_no_double_free: build failed"
    sed 's/^/        /' "$TMP/build.log" | head -8
    exit 1
fi
if ! "$TMP/prog" > "$TMP/run.out" 2>&1; then
    echo "  [FAIL] heap_field_borrowed_var_no_double_free: program aborted at runtime (rc $?)"
    sed 's/^/        /' "$TMP/run.out" | head -8
    exit 1
fi
if ! grep -q "^done$" "$TMP/run.out"; then
    echo "  [FAIL] heap_field_borrowed_var_no_double_free: did not reach 'done'"
    sed 's/^/        /' "$TMP/run.out" | head -8
    exit 1
fi

echo "  [PASS] heap_field_borrowed_var_no_double_free: borrowed field-store moves runtime ownership; no teardown double-free"
exit 0
