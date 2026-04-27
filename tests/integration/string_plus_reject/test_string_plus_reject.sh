#!/bin/sh
# Tests that `string + string` is rejected at typecheck time with a
# helpful diagnostic, rather than emitting invalid C. Closes #276.
#
# Pre-fix the codegen would emit `(const char*) + (const char*)` which
# the C compiler rejects with a pointer-arithmetic error pointing at
# generated C — useless to the .ae author.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

pass=0
fail=0

# Case 1: literal + literal
cat > /tmp/ae_strplus_bad1.ae << 'EOF'
main() {
    s = "hello" + " world"
    println(s)
}
EOF
output=$(AETHER_HOME="" "$ROOT/build/ae" build /tmp/ae_strplus_bad1.ae -o /tmp/ae_strplus_bad1_out 2>&1)
if echo "$output" | grep -q "'+' is not defined for strings"; then
    echo "  [PASS] literal + literal rejected with helpful diagnostic"
    pass=$((pass + 1))
else
    echo "  [FAIL] expected the helpful diagnostic; got:"
    echo "$output" | head -10 | sed 's/^/    /'
    fail=$((fail + 1))
fi

# Case 2: variable + literal — same path
cat > /tmp/ae_strplus_bad2.ae << 'EOF'
main() {
    a = "abc"
    s = a + "def"
    println(s)
}
EOF
output=$(AETHER_HOME="" "$ROOT/build/ae" build /tmp/ae_strplus_bad2.ae -o /tmp/ae_strplus_bad2_out 2>&1)
if echo "$output" | grep -q "'+' is not defined for strings"; then
    echo "  [PASS] var + literal rejected with helpful diagnostic"
    pass=$((pass + 1))
else
    echo "  [FAIL] expected the helpful diagnostic for var + literal"
    echo "$output" | head -10 | sed 's/^/    /'
    fail=$((fail + 1))
fi

# Case 3: int + int still works (regression sentry — we only block
# the string-string case).
cat > /tmp/ae_strplus_ok.ae << 'EOF'
main() {
    n = 1 + 2
    println(n)
}
EOF
if AETHER_HOME="" "$ROOT/build/ae" build /tmp/ae_strplus_ok.ae -o /tmp/ae_strplus_ok_out 2>/dev/null; then
    if /tmp/ae_strplus_ok_out 2>&1 | grep -q "^3$"; then
        echo "  [PASS] int + int still works"
        pass=$((pass + 1))
    else
        echo "  [FAIL] int + int compiled but produced wrong value"
        fail=$((fail + 1))
    fi
else
    echo "  [FAIL] int + int should still compile"
    fail=$((fail + 1))
fi

# Cleanup
rm -f /tmp/ae_strplus_bad1.ae /tmp/ae_strplus_bad2.ae /tmp/ae_strplus_ok.ae \
      /tmp/ae_strplus_bad1_out /tmp/ae_strplus_bad2_out /tmp/ae_strplus_ok_out

echo ""
echo "string_plus_reject: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
