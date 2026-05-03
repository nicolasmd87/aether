#!/bin/sh
# Issue #345 regression: std.cas put/has/get round-trip + digest
# stability + idempotent-put + negative-lookup behaviours.
#
# Runs the .ae driver under a clean AETHER_CAS root in /tmp so the
# test never touches the user's real ~/.aether/cas. The .ae driver
# self-reports PASS on the last line of stdout.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AE="$ROOT/build/ae"

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

# AETHER_CAS feeds straight into the Aether runtime's mkdir/stat
# calls, which on Windows-MinGW64 use the native libc — that libc
# doesn't translate MSYS virtual paths like /tmp/tmp.XYZ. Convert
# to a native (Win32-shaped, forward-slashed) path via cygpath when
# available; on POSIX cygpath isn't installed, the conditional
# falls through, and TMPDIR stays as-is.
NATIVE_TMPDIR="$TMPDIR"
if command -v cygpath >/dev/null 2>&1; then
    NATIVE_TMPDIR="$(cygpath -m "$TMPDIR")"
fi

ACTUAL="$TMPDIR/actual.log"
if ! AETHER_HOME="$ROOT" \
     AETHER_CAS="$NATIVE_TMPDIR/cas" \
     AETHER_CAS_TEST_DIR="$NATIVE_TMPDIR" \
     "$AE" run "$SCRIPT_DIR/cas_roundtrip.ae" > "$ACTUAL" 2>"$TMPDIR/err.log"; then
    echo "  [FAIL] ae run exited non-zero"
    cat "$TMPDIR/err.log" | head -10
    exit 1
fi

LAST=$(tail -1 "$ACTUAL")
case "$LAST" in
    PASS*) echo "  [PASS] cas round-trip (issue #345)" ;;
    *)
        echo "  [FAIL] cas driver did not print PASS"
        cat "$ACTUAL"
        exit 1
        ;;
esac

# Independent integrity check: the CAS root the driver populated
# should contain exactly one file, named with the well-known sha256
# digest of "hello cas".
EXPECTED_DIGEST="ba532ff613d7df59916589e39fc6dc9dc71b61bf7fd14a2c98c12c222a6cfd39"
# Check via TMPDIR (the bash-side path) — the Aether runtime wrote
# the entry at $NATIVE_TMPDIR/cas/<digest>; that's the same physical
# file viewable from the shell side as $TMPDIR/cas/<digest>.
if [ ! -f "$TMPDIR/cas/$EXPECTED_DIGEST" ]; then
    echo "  [FAIL] expected CAS entry not present at $TMPDIR/cas/$EXPECTED_DIGEST"
    ls -la "$TMPDIR/cas" 2>&1 | head -5
    exit 1
fi
