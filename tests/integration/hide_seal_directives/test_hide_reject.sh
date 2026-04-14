#!/bin/sh
# Test that `hide` and `seal except` directives correctly REJECT code that
# tries to reach a name they've blocked. Each case writes a tiny .ae file
# that should fail to compile, then asserts the compiler exited non-zero.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

pass=0
fail=0

run_reject() {
    case_name="$1"
    src_file="$2"
    if AETHER_HOME="" "$ROOT/build/ae" build "$src_file" -o /tmp/ae_hide_out 2>/dev/null; then
        echo "  [FAIL] $case_name should have been rejected"
        fail=$((fail + 1))
    else
        echo "  [PASS] $case_name correctly rejected"
        pass=$((pass + 1))
    fi
}

run_accept() {
    case_name="$1"
    src_file="$2"
    if AETHER_HOME="" "$ROOT/build/ae" build "$src_file" -o /tmp/ae_hide_out 2>/dev/null; then
        echo "  [PASS] $case_name correctly accepted"
        pass=$((pass + 1))
    else
        echo "  [FAIL] $case_name should have been accepted"
        fail=$((fail + 1))
    fi
}

# Case 1: direct read of a hidden variable from a nested block
cat > /tmp/ae_hide_read.ae << 'EOF'
extern println(s: string)
main() {
    secret = "abc"
    {
        hide secret
        println(secret)   // E0304: secret is hidden
    }
}
EOF
run_reject "read of hidden variable" /tmp/ae_hide_read.ae

# Case 2: direct write to a hidden variable from a nested block
cat > /tmp/ae_hide_write.ae << 'EOF'
extern println(s: string)
main() {
    secret = "abc"
    {
        hide secret
        secret = "xyz"    // E0304: cannot declare 'secret', it is hidden
    }
}
EOF
run_reject "write to hidden variable" /tmp/ae_hide_write.ae

# Case 3: seal except whitelist excludes a name that the body uses
cat > /tmp/ae_seal_excluded.ae << 'EOF'
extern println(s: string)
main() {
    allowed = "ok"
    forbidden = "no"
    {
        seal except allowed, println
        println(forbidden)   // E0304: forbidden is sealed out
    }
}
EOF
run_reject "seal except excludes used name" /tmp/ae_seal_excluded.ae

# Case 4: hide propagates to nested blocks
cat > /tmp/ae_hide_nested.ae << 'EOF'
extern println(s: string)
main() {
    secret = "abc"
    {
        hide secret
        {
            println(secret)   // E0304: still hidden in deeper nest
        }
    }
}
EOF
run_reject "hide propagates to nested block" /tmp/ae_hide_nested.ae

# Case 5: positive control — visible name in seal whitelist works
cat > /tmp/ae_hide_ok.ae << 'EOF'
extern println(s: string)
main() {
    allowed = "ok"
    {
        seal except allowed, println
        println(allowed)
    }
}
EOF
run_accept "seal except whitelisted name" /tmp/ae_hide_ok.ae

# Case 6: positive control — calling a visible function that USES the hidden var
cat > /tmp/ae_hide_via_helper.ae << 'EOF'
extern println(s: string)
helper(s: string) { println(s) }
main() {
    secret = "abc"
    {
        hide secret
        // helper is visible (we didn't hide it). secret reaches helper
        // through helper's own lexical chain at the call site, not ours.
        helper("via helper")
    }
}
EOF
run_accept "calling visible helper while hiding the var" /tmp/ae_hide_via_helper.ae

# Cleanup
rm -f /tmp/ae_hide_*.ae /tmp/ae_seal_*.ae /tmp/ae_hide_out

echo ""
echo "Hide/seal rejection tests: $pass passed, $fail failed"
if [ "$fail" -gt 0 ]; then exit 1; fi
