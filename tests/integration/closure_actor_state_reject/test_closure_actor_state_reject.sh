#!/bin/sh
# L4: compile-time rejection of closures that mutate actor state fields.
#
# The proper fix — threading `self` through the closure env so state
# mutations route to `self->field` — is larger language work. In the
# meantime, users would silently get wrong answers (the closure writes
# to an unscoped stack local that reads as 0). This test confirms the
# compiler rejects the pattern at compile time with a clear error
# instead.
#
# The error must mention the offending field name AND the actor name
# so the user can find the site quickly, AND point at the workaround
# (README_closure_actor_state_limitation.md).

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AETHERC="$ROOT/build/aetherc"

SRC="$SCRIPT_DIR/mutates_state.ae"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

# The compiler MUST fail. A successful compile means the bug is back.
if "$AETHERC" "$SRC" "$TMPDIR/out.c" >"$TMPDIR/err.log" 2>&1; then
    echo "  [FAIL] aetherc accepted a closure that mutates actor state — L4 regressed"
    exit 1
fi

# The error should reference the field name.
if ! grep -q "count" "$TMPDIR/err.log"; then
    echo "  [FAIL] error message doesn't mention the offending field name 'count'"
    cat "$TMPDIR/err.log"
    exit 1
fi

# The error should mention actor state or point at the workaround doc.
if ! grep -qE "actor state|state field|README_closure_actor_state" "$TMPDIR/err.log"; then
    echo "  [FAIL] error message doesn't explain actor-state context or point at workaround"
    cat "$TMPDIR/err.log"
    exit 1
fi

echo "  [PASS] closure_actor_state_reject: closure mutating actor state is rejected"
