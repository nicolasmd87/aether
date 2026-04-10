#!/bin/sh
# Test: --lib flag allows resolving modules from a custom library directory.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

pass=0
fail=0

# Test 1: --lib .mylib should find greeter module and run
cd "$SCRIPT_DIR"
if "$ROOT/build/aetherc" --lib .mylib custom_lib_dir.ae /tmp/ae_libdir_test.c 2>/dev/null; then
    if "$ROOT/build/ae" run custom_lib_dir.ae --lib .mylib >/tmp/ae_libdir_out.txt 2>&1; then
        echo "  [PASS] --lib resolves modules from custom dir"
        pass=$((pass + 1))
    else
        echo "  [FAIL] --lib compiled but runtime failed"
        fail=$((fail + 1))
    fi
else
    echo "  [FAIL] --lib failed to compile with custom lib dir"
    fail=$((fail + 1))
fi

# Test 2: without --lib, should fail (module not in lib/)
if "$ROOT/build/aetherc" custom_lib_dir.ae /tmp/ae_libdir_noflag.c 2>/dev/null; then
    echo "  [FAIL] should not compile without --lib flag"
    fail=$((fail + 1))
else
    echo "  [PASS] correctly fails without --lib flag"
    pass=$((pass + 1))
fi

# Test 3: AETHER_LIB_DIR env var should work like --lib
if AETHER_LIB_DIR=.mylib "$ROOT/build/ae" run custom_lib_dir.ae >/tmp/ae_libdir_env.txt 2>&1; then
    echo "  [PASS] AETHER_LIB_DIR env var resolves modules"
    pass=$((pass + 1))
else
    echo "  [FAIL] AETHER_LIB_DIR env var did not resolve modules"
    fail=$((fail + 1))
fi

# Test 4: --lib should override AETHER_LIB_DIR
if AETHER_LIB_DIR=nonexistent "$ROOT/build/ae" run custom_lib_dir.ae --lib .mylib >/tmp/ae_libdir_override.txt 2>&1; then
    echo "  [PASS] --lib overrides AETHER_LIB_DIR"
    pass=$((pass + 1))
else
    echo "  [FAIL] --lib did not override AETHER_LIB_DIR"
    fail=$((fail + 1))
fi

# Cleanup
rm -f /tmp/ae_libdir_test.c /tmp/ae_libdir_noflag.c /tmp/ae_libdir_out.txt /tmp/ae_libdir_env.txt /tmp/ae_libdir_override.txt

echo ""
echo "Custom lib dir tests: $pass passed, $fail failed"
if [ "$fail" -gt 0 ]; then exit 1; fi
