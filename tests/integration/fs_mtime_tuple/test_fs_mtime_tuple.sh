#!/bin/sh
# Regression: fs.mtime returns a Go-style (mtime, err) tuple,
# distinguishing "stat failed" from "file's mtime is 0". The legacy
# `file_mtime` extern collapses both into 0 — keeping it for back-
# compat, but `fs.mtime` is the canonical path now.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AE="$ROOT/build/ae"

if [ ! -x "$AE" ]; then
    echo "  [SKIP] fs_mtime_tuple: $AE not built"
    exit 0
fi

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

# Run the probe FROM inside the tmpdir and use bare filenames in the
# embedded Aether source. Embedding `$tmpdir` directly produces an
# MSYS2-style path on the Windows runner (`/tmp/tmp.XXXX/...`) which
# the native Windows binary's stat() can't resolve — MSYS2 translates
# paths at process exec, not inside text files. The CWD-relative
# shape sidesteps the translation gap entirely; the same approach
# works on every platform.
cd "$tmpdir" || exit 1

echo "x" > file_that_exists

cat > mtime_probe.ae <<'EOF'
import std.fs

main() {
    // Existing file: should return (mtime > 0 in any practical case, "")
    m1, e1 = fs.mtime("file_that_exists")
    if e1 != "" {
        println("FAIL: real-file mtime returned error: ${e1}")
        return
    }
    if m1 <= 0 {
        println("FAIL: real-file mtime non-positive: ${m1}")
        return
    }
    println("ok-real")

    // Missing file: must return (0, error). Distinguishes from 1970-
    // epoch file, which the legacy file_mtime cannot.
    m2, e2 = fs.mtime("no_such_file_at_all")
    if e2 == "" {
        println("FAIL: missing-file mtime returned no error (got ${m2})")
        return
    }
    if m2 != 0 {
        println("FAIL: missing-file mtime non-zero: ${m2}")
        return
    }
    println("ok-missing")
}
EOF

if ! "$AE" build mtime_probe.ae -o mt >"$tmpdir/build.log" 2>&1; then
    echo "  [FAIL] fs_mtime_tuple: build failed"
    cat "$tmpdir/build.log"
    exit 1
fi

# `./mt` on POSIX, `./mt.exe` on Windows MSYS2 — `ae build`'s output
# adds the platform's exe extension automatically. Probe both.
if [ -x ./mt ]; then
    out="$(./mt 2>&1)"
elif [ -x ./mt.exe ]; then
    out="$(./mt.exe 2>&1)"
else
    echo "  [FAIL] fs_mtime_tuple: built binary not found"
    ls -la
    exit 1
fi

if ! echo "$out" | grep -q "ok-real"; then
    echo "  [FAIL] fs_mtime_tuple: real-file probe failed"
    echo "  ---- output ----"
    echo "$out"
    exit 1
fi
if ! echo "$out" | grep -q "ok-missing"; then
    echo "  [FAIL] fs_mtime_tuple: missing-file probe failed"
    echo "  ---- output ----"
    echo "$out"
    exit 1
fi

echo "  [PASS] fs_mtime_tuple: real file -> (mtime, \"\"); missing file -> (0, error)"
exit 0
