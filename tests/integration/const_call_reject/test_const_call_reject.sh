#!/bin/sh
# Tests that `const X = some_function_call()` is rejected at
# typecheck time. Per Nico's design call: const inlines its RHS at
# every use site, so `const G = make_thing()` would silently re-call
# the function on every reference. Reject these at compile time with
# a helpful diagnostic that points the user at std.config /
# std.actors for process-global state. Section 2 of
# nuther-ask-of-aether-team.md.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

pass=0
fail=0

expect_reject() {
    label="$1"
    file="$2"
    out=$(AETHER_HOME="" "$ROOT/build/ae" build "$file" -o /tmp/ae_const_reject_out 2>&1)
    if echo "$out" | grep -q "const initializer must be a compile-time constant expression"; then
        echo "  [PASS] $label"
        pass=$((pass + 1))
    else
        echo "  [FAIL] $label — expected reject; got:"
        echo "$out" | head -8 | sed 's/^/    /'
        fail=$((fail + 1))
    fi
}

expect_accept() {
    label="$1"
    file="$2"
    if AETHER_HOME="" "$ROOT/build/ae" build "$file" -o /tmp/ae_const_ok_out 2>/dev/null; then
        echo "  [PASS] $label"
        pass=$((pass + 1))
    else
        echo "  [FAIL] $label — expected accept"
        fail=$((fail + 1))
    fi
}

# Case 1: top-level const = function call (the headline trap).
cat > /tmp/ae_const_bad1.ae << 'EOF'
extern malloc(size: int) -> ptr
const G_BUF = malloc(64)
main() { println("ok") }
EOF
expect_reject "top-level const = function call" /tmp/ae_const_bad1.ae

# Case 2: top-level const = stdlib call.
cat > /tmp/ae_const_bad2.ae << 'EOF'
import std.string
const NAME = string.from_int(42)
main() { println("ok") }
EOF
expect_reject "top-level const = std-namespaced call" /tmp/ae_const_bad2.ae

# Case 3: function-local const = call.
cat > /tmp/ae_const_bad3.ae << 'EOF'
extern malloc(size: int) -> ptr
main() {
    const LOCAL = malloc(64)
    println("ok")
}
EOF
expect_reject "function-local const = call" /tmp/ae_const_bad3.ae

# Case 4: legit literal const compiles.
cat > /tmp/ae_const_ok1.ae << 'EOF'
const PI = 3.14
const MAX = 100
const NAME = "alice"
main() { println("ok") }
EOF
expect_accept "literal const RHS still compiles" /tmp/ae_const_ok1.ae

# Case 5: arithmetic on literals compiles.
cat > /tmp/ae_const_ok2.ae << 'EOF'
const MAX = 100
const HALF = MAX / 2
const DOUBLE = MAX * 2
main() { println("ok") }
EOF
expect_accept "arithmetic on const RHS still compiles" /tmp/ae_const_ok2.ae

rm -f /tmp/ae_const_bad*.ae /tmp/ae_const_ok*.ae \
      /tmp/ae_const_reject_out /tmp/ae_const_ok_out

echo ""
echo "const_call_reject: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
