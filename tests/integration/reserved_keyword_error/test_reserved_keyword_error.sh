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
    kw="$3"          # the reserved word the error must name
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

    if ! grep -qi "reserved" "$log" || ! grep -q "$kw" "$log"; then
        echo "  [FAIL] $label: error message doesn't mention 'reserved' + '$kw'"
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

    # The `help:` line for the reserved-keyword error must match the
    # error. Previously E0100 fell back to the generic syntax-error
    # hint ("check for missing parentheses, braces, or keywords"),
    # which pointed users at a parse problem that wasn't there.
    # Extract the first error block (up to the first blank line) and
    # check its help line talks about renaming, not parentheses.
    first_block="$(awk '/^error/{flag=1} flag; /^$/{if(flag) exit}' "$log")"
    if echo "$first_block" | grep -q "missing parentheses"; then
        echo "  [FAIL] $label: reserved-keyword error still carries the generic 'missing parentheses' hint"
        echo "$first_block" | sed 's/^/          /'
        fail=1
        rm -rf "$tmpdir"
        return
    fi
    if ! echo "$first_block" | grep -qi "help:.*rename"; then
        echo "  [FAIL] $label: reserved-keyword error's help: line should suggest renaming"
        echo "$first_block" | sed 's/^/          /'
        fail=1
        rm -rf "$tmpdir"
        return
    fi

    echo "  [PASS] $label"
    rm -rf "$tmpdir"
}

check_case "$SCRIPT_DIR/message_as_param.ae"  "reserved_keyword_error: extern parameter name (message)" "message"
check_case "$SCRIPT_DIR/message_as_local.ae"  "reserved_keyword_error: local variable name (message)"   "message"
check_case "$SCRIPT_DIR/state_as_param.ae"    "reserved_keyword_error: extern parameter name (state)"   "state"

exit $fail
