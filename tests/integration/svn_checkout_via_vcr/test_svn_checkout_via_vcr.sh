#!/bin/sh
# Aether-driver replay validation against the canonical SVN-recorded
# tape. Drives 17 WebDAV interactions through std.http.client against
# an in-process VCR-backed server and asserts byte-for-byte equality
# on status, body, and full response-headers (preserving the
# duplicate-keyed `DAV:` headers SVN's protocol depends on).
#
# Skips on Windows (the tape's request-body XML uses POSIX line
# endings; the test path is more useful on POSIX).

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AE="$ROOT/build/ae"

if [ "$OS" = "Windows_NT" ]; then
    echo "  [SKIP] svn_checkout_via_vcr: Windows (POSIX-shaped paths in tape)"
    exit 0
fi

if [ ! -x "$AE" ]; then
    echo "  [SKIP] svn_checkout_via_vcr: ae not built"
    exit 0
fi

cd "$ROOT" || exit 1

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"; rm -f /tmp/svn_vcr_scrubbed.*.md' EXIT

if ! AETHER_HOME="$ROOT" "$AE" run "$SCRIPT_DIR/probe.ae" >"$TMPDIR/out.log" 2>&1; then
    echo "  [FAIL] svn_checkout_via_vcr"
    tail -60 "$TMPDIR/out.log" | sed 's/^/    /'
    exit 1
fi

# Probe prints a summary line; sanity-check it landed and shows 0 failing.
if ! grep -q "svn_vcr_replay:.*0 failing" "$TMPDIR/out.log"; then
    echo "  [FAIL] svn_checkout_via_vcr — non-zero failures or missing summary"
    tail -60 "$TMPDIR/out.log" | sed 's/^/    /'
    exit 1
fi

echo "  [PASS] svn_checkout_via_vcr"
exit 0
