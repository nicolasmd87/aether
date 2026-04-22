#!/bin/sh
# Regression: `[[bin]] extra_sources` matches the current ae_file
# across the three invocation shapes a user can reach.
#
#   1. Bare relative path — `ae build probe.ae` from the project dir.
#   2. Leading-./ relative path — `ae build ./probe.ae`.
#   3. Absolute path — `ae build /abs/path/to/probe.ae`.
#
# All three must pull in shim.c via the [[bin]] entry's extra_sources.
# Without shim.c, the link step fails with an undefined reference to
# ae_bin_probe_value. Success is "OK" on stdout from probe.ae.
#
# Also exercises the size_t-underflow edge case in the match code:
# when aef == eq length-wise but differ in bytes, the old `>=` guard
# would read aef[-1]. The fix tightens it to `>`.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AE="$ROOT/build/ae"

fail=0
tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

cd "$SCRIPT_DIR" || exit 1

run_case() {
    label="$1"
    ae_arg="$2"
    out="$tmpdir/run_$3.log"
    if ! "$AE" run "$ae_arg" >"$out" 2>&1; then
        echo "  [FAIL] bin_path_match: $label — build/run failed"
        sed 's/^/    /' "$out" | head -15
        fail=1
        return
    fi
    if ! grep -q "^OK$" "$out"; then
        echo "  [FAIL] bin_path_match: $label — probe did not print OK"
        sed 's/^/    /' "$out" | head -10
        fail=1
        return
    fi
    echo "  [PASS] bin_path_match: $label"
}

# Case 1: bare relative path from project root.
run_case "bare relative path"    "probe.ae"                "relative"

# Case 2: leading-./ relative path. Match logic strips "./" from both
# sides before comparing.
run_case "leading-./ relative"   "./probe.ae"              "dotslash"

# Case 3: absolute path. This is the 7a case in AETHER_ISSUES.md —
# previously relied on the ends-with suffix match without the size_t
# underflow guard.
run_case "absolute path"         "$SCRIPT_DIR/probe.ae"    "absolute"

exit $fail
