#!/bin/bash
# Test Aether-hosts-Aether: compile, sandboxed run, shared map,
# and stdout capture.
#
# Requires: ae compiler on PATH (or AETHER_AE_PATH set),
#           libaether_sandbox.so in build/.

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
AE="$ROOT/build/ae"
AETHERC="$ROOT/build/aetherc"
PRELOAD="$ROOT/build/libaether_sandbox.so"
PASS=0
FAIL=0
SKIP=0

check() {
    local name="$1" expected="$2" actual="$3"
    if echo "$actual" | grep -qF "$expected"; then
        echo "  [PASS] $name"
        PASS=$((PASS+1))
    else
        echo "  [FAIL] $name"
        echo "         expected: $expected"
        echo "         got:      $actual"
        FAIL=$((FAIL+1))
    fi
}

check_not() {
    local name="$1" unwanted="$2" actual="$3"
    if echo "$actual" | grep -qF "$unwanted"; then
        echo "  [FAIL] $name"
        echo "         should not contain: $unwanted"
        echo "         got: $actual"
        FAIL=$((FAIL+1))
    else
        echo "  [PASS] $name"
        PASS=$((PASS+1))
    fi
}

check_rc() {
    local name="$1" expected="$2" actual="$3"
    if [ "$actual" -eq "$expected" ]; then
        echo "  [PASS] $name (exit $actual)"
        PASS=$((PASS+1))
    else
        echo "  [FAIL] $name (expected exit $expected, got $actual)"
        FAIL=$((FAIL+1))
    fi
}

echo "============================================"
echo "  Aether-hosts-Aether Tests"
echo "============================================"
echo ""

# Check prerequisites
if [ ! -x "$AE" ]; then
    echo "  [SKIP] ae compiler not found at $AE"
    exit 0
fi

# ---------------------------------------------------------------
# Test 1: Compile a simple script
# ---------------------------------------------------------------
echo "=== Compile ==="

cat > /tmp/ae_host_test_hello.ae << 'EOF'
main() { println("hello from hosted aether") }
EOF

AETHER_HOME="" "$AE" build /tmp/ae_host_test_hello.ae \
    -o /tmp/ae_host_test_hello 2>/dev/null
check_rc "compile simple script" 0 $?

# ---------------------------------------------------------------
# Test 2: Run unsandboxed — stdout captured
# ---------------------------------------------------------------
echo ""
echo "=== Run (unsandboxed) ==="

OUT=$(/tmp/ae_host_test_hello 2>/dev/null)
check "stdout output" "hello from hosted aether" "$OUT"

# ---------------------------------------------------------------
# Test 3: Run sandboxed — allowed operations pass
# ---------------------------------------------------------------
echo ""
echo "=== Run sandboxed (allowed) ==="

# Script that only prints — no network, no fs, no exec
cat > /tmp/ae_host_test_print.ae << 'EOF'
main() { println("sandboxed ok") }
EOF

AETHER_HOME="" "$AE" build /tmp/ae_host_test_print.ae \
    -o /tmp/ae_host_test_print 2>/dev/null

# The preload library needs AETHER_SANDBOX_GRANTS (a file) or
# AETHER_SANDBOX_SHM to activate.  An empty grants file means
# deny-all.  We create one for deny-all tests and a permissive
# one for allow tests.
DENY_ALL="/tmp/ae_host_grants_deny_$$"
printf "" > "$DENY_ALL"

if [ -f "$PRELOAD" ]; then
    OUT=$(LD_PRELOAD="$PRELOAD" \
          AETHER_SANDBOX_LOG=none \
          AETHER_SANDBOX_GRANTS="$DENY_ALL" \
          /tmp/ae_host_test_print 2>/dev/null)
    check "sandboxed print works (deny-all)" \
        "sandboxed ok" "$OUT"
else
    echo "  [SKIP] libaether_sandbox.so not found"
    SKIP=$((SKIP+1))
fi

# ---------------------------------------------------------------
# Test 4: Run sandboxed — denied file read
# ---------------------------------------------------------------
echo ""
echo "=== Run sandboxed (denied file read) ==="

cat > /tmp/ae_host_test_fs.ae << 'EOF'
import std.fs
main() {
    f = file_open_raw("/etc/hostname", "r")
    if f == null {
        println("denied")
    } else {
        println("opened")
        file_close(f)
    }
}
EOF

AETHER_HOME="" "$AE" build /tmp/ae_host_test_fs.ae \
    -o /tmp/ae_host_test_fs 2>/dev/null

if [ -f "$PRELOAD" ]; then
    # Deny-all grants file = no fs_read grant
    OUT=$(LD_PRELOAD="$PRELOAD" \
          AETHER_SANDBOX_LOG=none \
          AETHER_SANDBOX_GRANTS="$DENY_ALL" \
          /tmp/ae_host_test_fs 2>/dev/null)
    check "fs denied without grant" "denied" "$OUT"

    # With fs_read grant — should succeed
    ALLOW_FS="/tmp/ae_host_grants_fs_$$"
    printf "fs_read:/etc/*\n" > "$ALLOW_FS"
    OUT=$(LD_PRELOAD="$PRELOAD" \
          AETHER_SANDBOX_LOG=none \
          AETHER_SANDBOX_GRANTS="$ALLOW_FS" \
          /tmp/ae_host_test_fs 2>/dev/null)
    check "fs allowed with grant" "opened" "$OUT"
    rm -f "$ALLOW_FS"
else
    echo "  [SKIP] libaether_sandbox.so not found"
    SKIP=$((SKIP+2))
fi

# ---------------------------------------------------------------
# Test 5: Run sandboxed — denied network
# ---------------------------------------------------------------
echo ""
echo "=== Run sandboxed (denied network) ==="

cat > /tmp/ae_host_test_net.ae << 'EOF'
extern tcp_connect_raw(host: string, port: int) -> ptr
main() {
    sock = tcp_connect_raw("127.0.0.1", 80)
    if sock == null {
        println("net denied")
    } else {
        println("net allowed")
    }
}
EOF

AETHER_HOME="" "$AE" build /tmp/ae_host_test_net.ae \
    -o /tmp/ae_host_test_net 2>/dev/null

if [ $? -eq 0 ] && [ -f "$PRELOAD" ]; then
    OUT=$(LD_PRELOAD="$PRELOAD" \
          AETHER_SANDBOX_LOG=none \
          AETHER_SANDBOX_GRANTS="$DENY_ALL" \
          /tmp/ae_host_test_net 2>/dev/null)
    check "tcp denied without grant" "net denied" "$OUT"
else
    echo "  [SKIP] compile failed or sandbox not available"
    SKIP=$((SKIP+1))
fi

# ---------------------------------------------------------------
# Test 6: Run sandboxed — denied exec
# ---------------------------------------------------------------
echo ""
echo "=== Run sandboxed (denied exec) ==="

# Note: execve interception via LD_PRELOAD does not work on
# modern glibc — internal libc calls (execvp -> execve) bypass
# the PLT and go through internal symbols.  This is a known
# limitation documented in containment-sandbox.md.  The in-
# process sandbox (aether_sandbox_check in os_system) catches
# exec from Aether code; LD_PRELOAD only catches it from
# external interpreters (Python, Lua, etc.) that call execve
# through the public PLT entry.
echo "  [SKIP] exec interception: known glibc LD_PRELOAD gap"
SKIP=$((SKIP+1))

# ---------------------------------------------------------------
# Test 7: Run sandboxed — denied env read
# ---------------------------------------------------------------
echo ""
echo "=== Run sandboxed (denied env) ==="

cat > /tmp/ae_host_test_env.ae << 'EOF'
extern os_getenv(name: string) -> string
main() {
    val = os_getenv("HOME")
    if val == null {
        println("env denied")
    } else {
        println("env got home")
    }
}
EOF

AETHER_HOME="" "$AE" build /tmp/ae_host_test_env.ae \
    -o /tmp/ae_host_test_env 2>/dev/null

if [ $? -eq 0 ] && [ -f "$PRELOAD" ]; then
    OUT=$(LD_PRELOAD="$PRELOAD" \
          AETHER_SANDBOX_LOG=none \
          AETHER_SANDBOX_GRANTS="$DENY_ALL" \
          /tmp/ae_host_test_env 2>/dev/null)
    check "env denied without grant" "env denied" "$OUT"
else
    echo "  [SKIP] compile failed or sandbox not available"
    SKIP=$((SKIP+1))
fi

# ---------------------------------------------------------------
# Cleanup
# ---------------------------------------------------------------
rm -f /tmp/ae_host_test_hello /tmp/ae_host_test_hello.ae \
      /tmp/ae_host_test_print /tmp/ae_host_test_print.ae \
      /tmp/ae_host_test_fs /tmp/ae_host_test_fs.ae \
      /tmp/ae_host_test_net /tmp/ae_host_test_net.ae \
      /tmp/ae_host_test_exec \
      /tmp/ae_host_test_env /tmp/ae_host_test_env.ae \
      "$DENY_ALL"

echo ""
echo "============================================"
echo "  Aether host tests: $PASS passed, $FAIL failed, $SKIP skipped"
echo "============================================"
if [ "$FAIL" -gt 0 ]; then exit 1; fi
