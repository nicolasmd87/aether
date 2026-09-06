#!/bin/sh
# A hand-computed byte count fed to `malloc(<literal>) as *T` is sized to a
# struct's CURRENT layout, so any layout change silently under-allocates it and
# the overflow only surfaces as heap corruption at runtime. The v0.643.0 inline
# heap-string trackers (#1879) did exactly this to a downstream repo's 158
# hand-sized sites. The compiler cannot know a pure-Aether struct's sizeof (C
# does), so it cannot check the number — but it CAN flag the pattern and nudge
# to `malloc(sizeof(T))`, which is layout-exact.
#
# This asserts the warning fires on the literal-size shape and — just as
# importantly — stays SILENT on sizeof(T) and on a raw (uncast) buffer. A lint
# that cries wolf on correct code is worse than none.
#
# Pruned from the generic .ae runner (these sources exist to have their
# diagnostics inspected, not to be run).
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AETHERC="$ROOT/build/aetherc"
if [ ! -x "$AETHERC" ]; then echo "  [SKIP] malloc_literal_struct_cast_warn: $AETHERC not built"; exit 0; fi
TMPDIR="$(mktemp -d)"; trap 'rm -rf "$TMPDIR"' EXIT

expect_warning() {
    name="$1"; want="$2"
    "$AETHERC" "$SCRIPT_DIR/$name.ae" "$TMPDIR/$name.c" >"$TMPDIR/$name.log" 2>&1
    if ! grep -qiE "$want" "$TMPDIR/$name.log"; then
        echo "  [FAIL] malloc_literal_struct_cast_warn: $name.ae did not warn as expected"
        echo "         expected a diagnostic matching: $want"
        sed 's/^/    /' "$TMPDIR/$name.log" | head -8
        exit 1
    fi
    # It must warn exactly once, not once per type-inference pass.
    count=$(grep -c "sizes the allocation by a literal" "$TMPDIR/$name.log")
    if [ "$count" != "1" ]; then
        echo "  [FAIL] malloc_literal_struct_cast_warn: $name.ae warned $count times, expected exactly 1"
        exit 1
    fi
}

expect_no_malloc_warning() {
    name="$1"
    "$AETHERC" "$SCRIPT_DIR/$name.ae" "$TMPDIR/$name.c" >"$TMPDIR/$name.log" 2>&1
    if grep -qi "sizes the allocation by a literal" "$TMPDIR/$name.log"; then
        echo "  [FAIL] malloc_literal_struct_cast_warn: $name.ae warned, but it is correct code"
        sed 's/^/    /' "$TMPDIR/$name.log" | head -8
        exit 1
    fi
}

expect_warning warn_literal_size 'malloc\(16\) as \*Node.*sizeof\(Node\)'
expect_no_malloc_warning ok_sizeof
expect_no_malloc_warning ok_raw_buffer

echo "  [PASS] malloc_literal_struct_cast_warn: literal-sized struct cast warns once; sizeof(T) and raw buffers stay silent"
exit 0
