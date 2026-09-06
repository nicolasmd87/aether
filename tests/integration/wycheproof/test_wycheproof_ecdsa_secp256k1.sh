#!/bin/sh
# Wycheproof adversarial vector suites — ECDSA secp256k1 (P1363 + DER), wave 6.
#
# The Bitcoin/Ethereum curve. Its own harness slot: a secp256k1 verify is two
# 256-bit bignum scalar multiplications (~1-2s each at CI's -O0). Default stride
# 10; WYCHEPROOF_FULL=1 (the nightly) sweeps all, WYCHEPROOF_STRIDE=N overrides.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
cd "$ROOT" || exit 1

AE="$ROOT/build/ae"
[ -n "${EXE_EXT:-}" ] && AE="$AE$EXE_EXT"
[ -x "$AE" ] || { echo "  [FAIL] wycheproof_ecdsa_secp256k1: build/ae missing (run make)"; exit 1; }

rc=0
for drv in wp_ecdsa_secp256k1 wp_ecdsa_secp256k1_der; do
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
