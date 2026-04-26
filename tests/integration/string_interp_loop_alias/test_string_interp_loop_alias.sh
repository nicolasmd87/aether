#!/bin/sh
# Issue #250 regression: string interpolation in accumulator loops
# where the LHS appears on the RHS — `out = "${out}…"`.
#
# Earlier, the codegen recycled the LHS's backing buffer between
# iterations, so iteration N+1 saw freed-then-reallocated bytes
# before the new content was written. Symptom: garbage prefix,
# correct suffix (e.g. "!d^?U### Request headers...").
#
# This test runs the program and compares stdout against a known-good
# expected output. A regression that produces garbage bytes shows up
# immediately as a diff. Test is portable: works on every host where
# `ae run` works (no ASAN required).

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AE="$ROOT/build/ae"

SRC="$SCRIPT_DIR/loop_self_aliasing.ae"
EXPECTED="$SCRIPT_DIR/expected_output.txt"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

ACTUAL="$TMPDIR/actual.txt"
if ! AETHER_HOME="$ROOT" "$AE" run "$SRC" >"$ACTUAL" 2>"$TMPDIR/err.log"; then
    echo "  [FAIL] ae run exited non-zero"
    cat "$TMPDIR/err.log"
    exit 1
fi

# Filter out compiler warnings that may appear on stderr but shouldn't
# affect stdout content. We compare *stdout only* (already separated above).
if ! diff -u "$EXPECTED" "$ACTUAL" >"$TMPDIR/diff.log"; then
    echo "  [FAIL] string-interp accumulator loops produced wrong output"
    echo "  --- expected vs actual diff ---"
    cat "$TMPDIR/diff.log"
    exit 1
fi

echo "  [PASS] string_interp_loop_alias: 5 accumulator-loop shapes round-trip clean"
