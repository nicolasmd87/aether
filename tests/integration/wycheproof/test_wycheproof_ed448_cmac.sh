#!/bin/sh
# Wycheproof adversarial vector suites — Ed448 signature verify + AES-CMAC, wave 6.
#
# Two families sharing one harness slot. AES-CMAC is symmetric-fast (full sweep
# by default). Ed448 verify is bignum-heavy (~4s each), so it stride-samples
# (default 4); WYCHEPROOF_FULL=1 (the nightly) sweeps all 87, WYCHEPROOF_STRIDE=N
# overrides both.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
cd "$ROOT" || exit 1

AE="$ROOT/build/ae"
[ -n "${EXE_EXT:-}" ] && AE="$AE$EXE_EXT"
[ -x "$AE" ] || { echo "  [FAIL] wycheproof_ed448_cmac: build/ae missing (run make)"; exit 1; }

rc=0
for drv in wp_aes_cmac wp_ed448; do
    out="$("$AE" run "tests/integration/wycheproof/$drv.ae" 2>&1)"
    if printf '%s' "$out" | grep -q "^ALL PASS"; then
        printf '%s\n' "$out" | grep "^wycheproof" | sed 's/^/  [PASS] /'
    else
        echo "  [FAIL] wycheproof $drv:"
        printf '%s\n' "$out" | tail -12 | sed 's/^/        /'
        rc=1
    fi
done
exit $rc
