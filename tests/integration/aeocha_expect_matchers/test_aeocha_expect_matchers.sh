#!/bin/sh
# Smoke test for the expect_* integration-shape matchers in
# contrib.aeocha (#aeocha-integration-helpers ask). Exercises both
# the process and HTTP halves end-to-end against real subprocesses
# and a VCR-loaded in-process server.
#
# Skips on Windows — os.run_capture is POSIX-only there, and the
# test depends on /bin/echo and /bin/sh.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AE="$ROOT/build/ae"

if [ "$OS" = "Windows_NT" ]; then
    echo "  [SKIP] aeocha_expect_matchers: Windows (os.run_capture is POSIX-only)"
    exit 0
fi

if [ ! -x "$AE" ]; then
    echo "  [SKIP] aeocha_expect_matchers: ae not built"
    exit 0
fi

if [ ! -x /bin/echo ] || [ ! -x /bin/sh ]; then
    echo "  [SKIP] aeocha_expect_matchers: /bin/echo or /bin/sh not present"
    exit 0
fi

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

# Run from repo root so the tape path inside probe.ae resolves.
# The probe.ae itself signals failure via aeocha.run_summary →
# exit(1), so the shell test just observes the exit code.
cd "$ROOT" || exit 1
if ! "$AE" run "$SCRIPT_DIR/probe.ae" >"$tmpdir/out.log" 2>&1; then
    echo "  [FAIL] aeocha_expect_matchers"
    tail -30 "$tmpdir/out.log"
    exit 1
fi

# Sanity check that all aeocha sections actually ran (probe.ae would
# also report "0 passing" with exit 0 if the describe blocks were
# skipped silently — though the framework prints "passing" line).
if ! grep -q "passing" "$tmpdir/out.log"; then
    echo "  [FAIL] aeocha_expect_matchers — no passing summary line"
    cat "$tmpdir/out.log"
    exit 1
fi

# Confirm at least one of the new matchers actually ran.
if ! grep -qE "expect_(exit|http_status|stdout_line_field)" "$tmpdir/out.log"; then
    # Aeocha only prints individual `it()` names, not matcher names.
    # Look for the describe headers instead.
    if ! grep -q "expect_\* — process matchers" "$tmpdir/out.log"; then
        echo "  [FAIL] aeocha_expect_matchers — describe header missing"
        cat "$tmpdir/out.log"
        exit 1
    fi
fi

echo "  [PASS] aeocha_expect_matchers"
exit 0
