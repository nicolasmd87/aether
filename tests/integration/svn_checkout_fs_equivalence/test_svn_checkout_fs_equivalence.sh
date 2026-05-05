#!/bin/sh
# Manual live-vs-replay integration test for the canonical SVN checkout
# tape imported from Servirtium-Java.
#
# It uses the real `svn` executable twice:
#   1. live checkout from https://svn.apache.org/repos/asf/...
#   2. playback checkout from the local std.http.server.vcr server
#
# Then it compares path+hash manifests for regular working-tree files,
# excluding Subversion's `.svn` administrative metadata.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AE="$ROOT/build/ae"

CANONICAL_URL="https://svn.apache.org/repos/asf/synapse/tags/3.0.0/modules/distribution/src/main/conf"
PLAYBACK_URL="http://127.0.0.1:18142/repos/asf/synapse/tags/3.0.0/modules/distribution/src/main/conf"

if [ "$OS" = "Windows_NT" ]; then
    echo "  [SKIP] svn_checkout_fs_equivalence: Windows"
    exit 0
fi

if [ "${AETHER_RUN_LIVE_SVN_CHECKOUT_EQUIV:-}" != "1" ]; then
    echo "  [SKIP] svn_checkout_fs_equivalence: set AETHER_RUN_LIVE_SVN_CHECKOUT_EQUIV=1 for live svn.apache.org check"
    exit 0
fi

if [ ! -x "$AE" ]; then
    echo "  [SKIP] svn_checkout_fs_equivalence: ae not built"
    exit 0
fi

if ! command -v svn >/dev/null 2>&1; then
    echo "  [SKIP] svn_checkout_fs_equivalence: svn not found"
    exit 0
fi

if command -v sha256sum >/dev/null 2>&1; then
    hash_file() { sha256sum "$1" | awk '{print $1}'; }
elif command -v shasum >/dev/null 2>&1; then
    hash_file() { shasum -a 256 "$1" | awk '{print $1}'; }
else
    echo "  [SKIP] svn_checkout_fs_equivalence: sha256sum/shasum not found"
    exit 0
fi

cd "$ROOT" || exit 1

mkdir -p "$ROOT/build"
TMPDIR="$(mktemp -d "$ROOT/build/svn_checkout_fs_equivalence.XXXXXX")"
SRV_PID=""
cleanup() {
    if [ -n "$SRV_PID" ]; then
        kill "$SRV_PID" >/dev/null 2>&1 || true
        wait "$SRV_PID" >/dev/null 2>&1 || true
    fi
    rm -rf "$TMPDIR"
}
trap cleanup EXIT

manifest() {
    dir="$1"
    out="$2"
    (
        cd "$dir" || exit 1
        find . -path '*/.svn/*' -prune -o -type f -print | LC_ALL=C sort |
        while IFS= read -r path; do
            h="$(hash_file "$path")"
            printf '%s  %s\n' "$h" "$path"
        done
    ) >"$out"
}

mkdir "$TMPDIR/live" "$TMPDIR/replay"

if ! svn checkout --non-interactive "$CANONICAL_URL" "$TMPDIR/live/conf" >"$TMPDIR/live.log" 2>&1; then
    if grep -q "no such table: wcroot" "$TMPDIR/live.log"; then
        echo "  [SKIP] svn_checkout_fs_equivalence: local svn working-copy SQLite is unusable (wcroot table missing)"
        exit 0
    fi
    echo "  [FAIL] svn_checkout_fs_equivalence: live checkout failed"
    tail -80 "$TMPDIR/live.log" | sed 's/^/    /'
    exit 1
fi

if ! (
    cd "$SCRIPT_DIR" || exit 1
    AETHER_HOME="$ROOT" "$AE" build server.ae -o "$TMPDIR/server"
) >"$TMPDIR/build.log" 2>&1; then
    echo "  [FAIL] svn_checkout_fs_equivalence: failed to build VCR server"
    tail -80 "$TMPDIR/build.log" | sed 's/^/    /'
    exit 1
fi

perl -0pe '
    s/(### Request headers recorded for playback:\n\n```\n).*?(\n```)/$1\n$2/sg;
    s/(### Request body recorded for playback[^\n]*\n\n```\n).*?(\n```)/$1\n$2/sg;
' "$ROOT/tests/integration/ExampleSubversionCheckoutRecording.md" >"$TMPDIR/scrubbed.full.tape"

cat >"$TMPDIR/svn14-report-404.tape" <<'EOF'
## Interaction 4: REPORT /repos/asf/!svn/rvr/1850471/synapse/tags/3.0.0/modules/distribution/src/main/conf

### Request headers recorded for playback:

```

```

### Request body recorded for playback (text/xml):

```

```

### Response headers recorded for playback:

```
Content-Type: text/plain
```

### Response body recorded for playback (404: text/plain):

```
404 Not Found
```

EOF

awk -v synth="$TMPDIR/svn14-report-404.tape" '
    BEGIN { skip = 0 }
    /^## Interaction 4:/ {
        while ((getline line < synth) > 0) print line
        close(synth)
        skip = 1
        next
    }
    /^## Interaction 12:/ { skip = 0 }
    skip == 0 { print }
' "$TMPDIR/scrubbed.full.tape" >"$TMPDIR/scrubbed.tape"

AETHER_HOME="$ROOT" TAPE_PATH="$TMPDIR/scrubbed.tape" ACCESS_LOG="$TMPDIR/access.log" \
    "$TMPDIR/server" >"$TMPDIR/server.log" 2>&1 &
SRV_PID=$!

i=0
while [ "$i" -lt 80 ]; do
    if grep -q '^READY' "$TMPDIR/server.log" 2>/dev/null; then
        break
    fi
    if ! kill -0 "$SRV_PID" >/dev/null 2>&1; then
        echo "  [FAIL] svn_checkout_fs_equivalence: VCR server died before READY"
        sed 's/^/    /' "$TMPDIR/server.log"
        exit 1
    fi
    sleep 0.25
    i=$((i + 1))
done

if ! grep -q '^READY' "$TMPDIR/server.log" 2>/dev/null; then
    echo "  [FAIL] svn_checkout_fs_equivalence: VCR server never reported READY"
    sed 's/^/    /' "$TMPDIR/server.log"
    exit 1
fi

if ! svn checkout --non-interactive "$PLAYBACK_URL" "$TMPDIR/replay/conf" >"$TMPDIR/replay.log" 2>&1; then
    if grep -q "no such table: wcroot" "$TMPDIR/replay.log"; then
        echo "  [SKIP] svn_checkout_fs_equivalence: local svn working-copy SQLite is unusable (wcroot table missing)"
        exit 0
    fi
    echo "  [FAIL] svn_checkout_fs_equivalence: playback checkout failed"
    tail -80 "$TMPDIR/replay.log" | sed 's/^/    /'
    if [ -s "$TMPDIR/access.log" ]; then
        echo "  [access-log]"
        tail -40 "$TMPDIR/access.log" | sed 's/^/    /'
    fi
    echo "  [server]"
    tail -80 "$TMPDIR/server.log" | sed 's/^/    /'
    exit 1
fi

manifest "$TMPDIR/live/conf" "$TMPDIR/live.manifest"
manifest "$TMPDIR/replay/conf" "$TMPDIR/replay.manifest"

if ! diff -u "$TMPDIR/live.manifest" "$TMPDIR/replay.manifest" >"$TMPDIR/manifest.diff"; then
    echo "  [FAIL] svn_checkout_fs_equivalence: live and playback working-tree files differ"
    sed 's/^/    /' "$TMPDIR/manifest.diff"
    exit 1
fi

count="$(wc -l < "$TMPDIR/live.manifest" | tr -d ' ')"
echo "  [PASS] svn_checkout_fs_equivalence: ${count} working-tree files match"
exit 0
