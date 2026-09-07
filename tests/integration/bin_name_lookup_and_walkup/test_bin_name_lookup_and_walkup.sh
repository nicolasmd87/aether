#!/bin/sh
# Tests two related improvements to `ae build` (#280):
#   (1) Bin-name lookup: `ae build myprobe` resolves `myprobe` to the
#       [[bin]] entry's `path` field instead of treating it as a
#       literal file path that doesn't exist.
#   (2) aether.toml walk-up: running `ae build foo.ae` from a
#       subdirectory where the toml lives in an ancestor directory
#       finds and uses that toml — extra_sources are applied, the
#       link succeeds.
#   (3) The two TOGETHER (#1905): `ae build <bin-name>` from a
#       subdirectory. The walk-up rebases a relative positional so a
#       file path still resolves after the chdir, and it used to do
#       that to a bin NAME as well: `myprobe` became `ae/myprobe`,
#       which is not a file, so the build failed from a subdirectory
#       while the identical command worked from the root.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AE="$ROOT/build/ae"

case "$(uname -s 2>/dev/null)" in
    MINGW*|MSYS*|CYGWIN*|Windows_NT)
        echo "  [SKIP] bin_name_lookup_and_walkup on Windows"; exit 0 ;;
esac

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

pass=0
fail=0

# Case 1: ae build <bin-name> from project root.
cd "$SCRIPT_DIR" || exit 1
if "$AE" build myprobe -o "$TMPDIR/probe1" >"$TMPDIR/build1.log" 2>&1; then
    if "$TMPDIR/probe1" 2>&1 | grep -q "^42$"; then
        echo "  [PASS] ae build <bin-name> resolves to [[bin]] entry"
        pass=$((pass + 1))
    else
        echo "  [FAIL] bin built but didn't print 42 (extra_sources not applied?)"
        fail=$((fail + 1))
    fi
else
    echo "  [FAIL] ae build myprobe failed"
    cat "$TMPDIR/build1.log"
    fail=$((fail + 1))
fi

# Case 2: ae build <file.ae> from a subdir, walking up to find toml.
cd "$SCRIPT_DIR/ae" || exit 1
if "$AE" build myprobe.ae -o "$TMPDIR/probe2" >"$TMPDIR/build2.log" 2>&1; then
    if "$TMPDIR/probe2" 2>&1 | grep -q "^42$"; then
        echo "  [PASS] ae build foo.ae walks up to find aether.toml"
        pass=$((pass + 1))
    else
        echo "  [FAIL] walk-up bin built but didn't print 42"
        fail=$((fail + 1))
    fi
else
    echo "  [FAIL] ae build myprobe.ae from subdir failed"
    cat "$TMPDIR/build2.log"
    fail=$((fail + 1))
fi

# Case 3 (#1905): ae build <bin-name> from a subdir. The name must NOT be
# rebased into a path.
cd "$SCRIPT_DIR/ae" || exit 1
if "$AE" build myprobe -o "$TMPDIR/probe3" >"$TMPDIR/build3.log" 2>&1; then
    if "$TMPDIR/probe3" 2>&1 | grep -q "^42$"; then
        echo "  [PASS] ae build <bin-name> works from a subdirectory"
        pass=$((pass + 1))
    else
        echo "  [FAIL] subdir bin-name built but didn't print 42"
        fail=$((fail + 1))
    fi
else
    echo "  [FAIL] ae build myprobe from a subdir failed"
    cat "$TMPDIR/build3.log"
    fail=$((fail + 1))
fi

# Case 4: an argument that is neither a bin name nor a file is reported as
# what the user TYPED, not as a path they never mentioned.
cd "$SCRIPT_DIR/ae" || exit 1
"$AE" build nosuchthing -o "$TMPDIR/probe4" >"$TMPDIR/build4.log" 2>&1
if grep -q "nosuchthing" "$TMPDIR/build4.log" && \
   ! grep -q "ae/nosuchthing" "$TMPDIR/build4.log"; then
    echo "  [PASS] an unknown positional is reported as typed"
    pass=$((pass + 1))
else
    echo "  [FAIL] unknown positional was rebased in the error message"
    cat "$TMPDIR/build4.log"
    fail=$((fail + 1))
fi

echo ""
echo "bin_name_lookup_and_walkup: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
