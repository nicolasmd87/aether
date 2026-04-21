#!/bin/sh
# Regression: `aether.toml` [build] cflags are applied on both `ae build`
# (release/optimize) AND `ae run` (non-optimize) paths. Previously only
# the release path picked them up, so users had to drop warnings or
# defines that their extern C shims relied on.
#
# The adjacent shim.c contains a #error that fires unless the cflags
# from aether.toml's [build] section define AE_CFLAGS_TEST_MARKER.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AE="$ROOT/build/ae"

fail=0
tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

# Build into tmpdir (keeps tests/ clean). `ae run` on the .ae file from
# the project dir so aether.toml is picked up.
cd "$SCRIPT_DIR" || exit 1

# Test 1: ae build picks up the cflags (this was already working).
build_log="$tmpdir/build.log"
if ! "$AE" build probe.ae -o "$tmpdir/probe_build" >"$build_log" 2>&1; then
    echo "  [FAIL] ae_run_cflags: ae build rejected the project; cflags were not applied"
    sed 's/^/    /' "$build_log" | head -10
    fail=1
else
    if ! "$tmpdir/probe_build" > "$tmpdir/build.out" 2>&1 || ! grep -q "^OK$" "$tmpdir/build.out"; then
        echo "  [FAIL] ae_run_cflags: ae build produced a binary but the probe returned wrong value"
        sed 's/^/    /' "$tmpdir/build.out" | head -5
        fail=1
    else
        echo "  [PASS] ae_run_cflags: ae build applies [build] cflags"
    fi
fi

# Test 2: ae run picks up the same cflags — this is the regression.
# `ae run` compiles and executes in one step; success means cflags reached
# the compile step AND the program returned 0 AND stdout contained "OK".
run_log="$tmpdir/run.log"
if ! "$AE" run probe.ae >"$run_log" 2>&1; then
    echo "  [FAIL] ae_run_cflags: ae run failed — cflags likely not applied to the ae-run path"
    sed 's/^/    /' "$run_log" | head -15
    fail=1
elif ! grep -q "^OK$" "$run_log"; then
    echo "  [FAIL] ae_run_cflags: ae run succeeded but probe did not print OK"
    sed 's/^/    /' "$run_log" | head -10
    fail=1
else
    echo "  [PASS] ae_run_cflags: ae run applies [build] cflags"
fi

exit $fail
