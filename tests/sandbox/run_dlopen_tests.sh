#!/bin/bash
# Test that dlopen/syscall escape attempts are blocked by the sandbox
# No set -e: tests may return nonzero when sandbox blocks operations

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PRELOAD="$ROOT/build/libaether_sandbox.so"
GRANTS="$ROOT/tests/sandbox/dlopen-test-grants.txt"

if [ ! -f "$PRELOAD" ]; then
    echo "Build libaether_sandbox.so first: make"
    exit 1
fi

echo "============================================"
echo "  Sandbox Escape Prevention Tests"
echo "  Testing: dlopen, syscall, FFI blocking"
echo "============================================"
echo ""

run_sandboxed() {
    local name="$1"; shift
    echo "--- $name ---"
    AETHER_SANDBOX_GRANTS="$GRANTS" \
    LD_PRELOAD="$PRELOAD" \
    "$@" 2>/dev/null
    echo ""
}

echo "=== Python (ctypes / dlopen) ==="
if command -v python3 >/dev/null 2>&1; then
    run_sandboxed "Python" python3 "$ROOT/tests/sandbox/test_dlopen_python.py"
else echo "  [SKIP] python3 not installed"; fi

echo "=== Lua (require / os.execute) ==="
LUA_CMD=""
for v in lua5.4 lua5.3 lua; do command -v $v >/dev/null 2>&1 && LUA_CMD=$v && break; done
if [ -n "$LUA_CMD" ]; then
    run_sandboxed "Lua" $LUA_CMD "$ROOT/tests/sandbox/test_dlopen_lua.lua"
else echo "  [SKIP] lua not installed"; fi

echo "=== Ruby (Fiddle / dlopen) ==="
if command -v ruby >/dev/null 2>&1; then
    run_sandboxed "Ruby" ruby "$ROOT/tests/sandbox/test_dlopen_ruby.rb"
else echo "  [SKIP] ruby not installed"; fi

echo "=== Perl (syscall / DynaLoader) ==="
if command -v perl >/dev/null 2>&1; then
    run_sandboxed "Perl" perl "$ROOT/tests/sandbox/test_dlopen_perl.pl"
else echo "  [SKIP] perl not installed"; fi

echo "============================================"
echo "  All escape attempts should show BLOCKED"
echo "============================================"
