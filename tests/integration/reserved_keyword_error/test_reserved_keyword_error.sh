#!/bin/sh
# Regression: using a reserved keyword (`message`, `state`, `send`, ...)
# where an identifier is expected must fail to compile with an error
# that (a) names the offending keyword, and (b) suggests renaming.
# Previously the parser emitted "Expected IDENTIFIER, got <TOKEN_NAME>"
# which didn't mention the actual source text and made it hard to tell
# what was wrong without reading the grammar.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AETHERC="$ROOT/build/aetherc"

fail=0

check_case() {
    src="$1"
    label="$2"
    tmpdir="$(mktemp -d)"
    log="$tmpdir/cc.log"

    # aetherc reports parse errors on stderr but currently exits 0;
    # inspect the log rather than the return code. The acceptance
    # criteria are (1) an error IS reported, and (2) it names the
    # offending keyword and suggests renaming.
    "$AETHERC" "$src" "$tmpdir/out.c" >"$log" 2>&1

    if ! grep -q '^error' "$log"; then
        echo "  [FAIL] $label: aetherc reported no error for a reserved-keyword identifier"
        fail=1
        rm -rf "$tmpdir"
        return
    fi

    if ! grep -qi "reserved" "$log" || ! grep -q "message" "$log"; then
        echo "  [FAIL] $label: error message doesn't mention 'reserved' + 'message'"
        echo "        got:"
        sed 's/^/          /' "$log" | head -8
        fail=1
        rm -rf "$tmpdir"
        return
    fi

    if ! grep -qi "rename" "$log"; then
        echo "  [FAIL] $label: error message doesn't suggest renaming"
        sed 's/^/          /' "$log" | head -8
        fail=1
        rm -rf "$tmpdir"
        return
    fi

    echo "  [PASS] $label"
    rm -rf "$tmpdir"
}

check_case "$SCRIPT_DIR/message_as_param.ae"  "reserved_keyword_error: extern parameter name"
check_case "$SCRIPT_DIR/message_as_local.ae"  "reserved_keyword_error: local variable name"

exit $fail
