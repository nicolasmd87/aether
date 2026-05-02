#!/bin/sh
# Regression: `ae cflags` prints pkg-config-style include and link
# flags so external tooling can `$(ae cflags)` instead of carrying
# its own copy of the install layout. Issue #329 follow-on item 1.
#
# Also exercises the dynamic -I enumeration that backs cflags — the
# previous hardcoded list silently dropped new modules; the walker
# can't miss them. Confirmed by checking that recently-added stdlib
# subdirs (std/cryptography, std/zlib, std/dl, std/config, std/actors,
# std/http*) all appear in the output.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AE="$ROOT/build/ae"

if [ ! -x "$AE" ]; then
    echo "  [SKIP] ae_cflags: $AE not built"
    exit 0
fi

flags="$("$AE" cflags 2>/dev/null)"
if [ -z "$flags" ]; then
    echo "  [FAIL] ae_cflags: produced empty output"
    exit 1
fi

# Spot-check: every previously-missing stdlib subdir must show up.
for needle in std/cryptography std/zlib std/dl std/config std/actors std/http; do
    if ! printf %s "$flags" | grep -q -- "-I.*$needle"; then
        echo "  [FAIL] ae_cflags: missing -I for $needle (the dynamic walker should pick it up)"
        echo "  ---- output ----"
        echo "$flags"
        exit 1
    fi
done

# Must include -laether (or skip if no precompiled lib was built).
if ! printf %s "$flags" | grep -q -- "-laether"; then
    if [ -f "$ROOT/build/libaether.a" ]; then
        echo "  [FAIL] ae_cflags: libaether.a exists but -laether is missing from output"
        echo "  ---- output ----"
        echo "$flags"
        exit 1
    fi
fi

# --cflags subset: must be -I-only, no -L / -l flags.
cflags_only="$("$AE" cflags --cflags 2>/dev/null)"
if printf %s "$cflags_only" | grep -qE -- "(^|[[:space:]])-(L|l)"; then
    echo "  [FAIL] ae_cflags --cflags: leaked link flags into the compile-only subset"
    echo "  ---- output ----"
    echo "$cflags_only"
    exit 1
fi

# --libs subset: no -I flags.
libs_only="$("$AE" cflags --libs 2>/dev/null)"
if printf %s "$libs_only" | grep -q -- "-I"; then
    echo "  [FAIL] ae_cflags --libs: leaked include flags into the link-only subset"
    echo "  ---- output ----"
    echo "$libs_only"
    exit 1
fi

# Functional check: `gcc trivial.c $(ae cflags) -o out` runs end-to-end.
tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT
cat > "$tmpdir/trivial.c" <<'C'
#include <stdio.h>
int main(void) { printf("ae_cflags ok\n"); return 0; }
C

if ! gcc "$tmpdir/trivial.c" $flags -o "$tmpdir/trivial" >"$tmpdir/gcc.log" 2>&1; then
    echo "  [FAIL] ae_cflags: gcc \$(ae cflags) failed end-to-end"
    cat "$tmpdir/gcc.log"
    exit 1
fi

actual="$("$tmpdir/trivial")"
if [ "$actual" != "ae_cflags ok" ]; then
    echo "  [FAIL] ae_cflags: end-to-end binary printed '$actual' (expected 'ae_cflags ok')"
    exit 1
fi

echo "  [PASS] ae_cflags: full / --cflags / --libs subsets correct, gcc \$(ae cflags) builds"
exit 0
