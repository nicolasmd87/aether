#!/bin/sh
# Regression: `@aether extern foo(...)` suppresses the call-site
# aether_string_data() unwrap that 718d13d introduced for naive C
# externs. Without the suppression, AetherString headers get
# stripped at the call site and binary content with embedded NULs
# strlen-truncates on the receiving side. See #351.
#
# Acceptance: a .ae round-trips a 7-byte payload with NUL at byte 4
# through an `@aether extern`-declared function and the receiver
# sees length=7 and byte[5]=14, matching what the producer sees.
#
# Companion negative case: a *bare* `extern` declaration of the
# same function still strips (preserves the #297 fix for naive
# C externs that expect a const char*).

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AETHERC="$ROOT/build/aetherc"

if [ ! -x "$AETHERC" ]; then
    echo "  [SKIP] aether_extern_annotation: toolchain not built"
    exit 0
fi

# Need the install for libaether.a + headers.
PREFIX="$HOME/.local"
if [ ! -f "$PREFIX/lib/aether/libaether.a" ] || \
   [ ! -d "$PREFIX/include/aether" ]; then
    echo "  [SKIP] aether_extern_annotation: no install at $PREFIX (run 'make install PREFIX=\$HOME/.local')"
    exit 0
fi

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

# Helper module — produces a binary string with embedded NUL at byte 4
# and consumes it via Aether's str_len-dispatching string.length().
cat > "$tmpdir/helper.ae" <<'AE'
import std.string
import std.bytes

export make_binary() -> string {
    b = bytes.new(16)
    bytes.set(b, 0, 83)
    bytes.set(b, 1, 86)
    bytes.set(b, 2, 78)
    bytes.set(b, 3, 1)
    bytes.set(b, 4, 0)
    bytes.set(b, 5, 14)
    bytes.set(b, 6, 14)
    return bytes.finish(b, 7)
}

export consume_binary(s: string) {
    println("len=${string.length(s)} byte5=${string.char_at(s, 5)}")
}

main() { println("library mode") }
AE

# Repro 1: WITH @aether annotation — must round-trip cleanly.
cat > "$tmpdir/repro_good.ae" <<'AE'
import std.string

@aether extern make_binary() -> string
@aether extern consume_binary(s: string)

main() {
    s = make_binary()
    consume_binary(s)
}
AE

# Repro 2: WITHOUT annotation (bare extern) — must still strip,
# preserving backward compat with naive C externs that expect
# a const char* via aether_string_data() unwrap.
cat > "$tmpdir/repro_bare.ae" <<'AE'
import std.string

extern make_binary() -> string
extern consume_binary(s: string)

main() {
    s = make_binary()
    consume_binary(s)
}
AE

# Compile both flavours.
"$AETHERC" --emit=lib "$tmpdir/helper.ae" "$tmpdir/helper.c" >"$tmpdir/c1.log" 2>&1 || {
    echo "  [FAIL] helper.ae did not compile"
    cat "$tmpdir/c1.log"
    exit 1
}
"$AETHERC" "$tmpdir/repro_good.ae" "$tmpdir/repro_good.c" >"$tmpdir/c2.log" 2>&1 || {
    echo "  [FAIL] repro_good.ae (with @aether) did not compile"
    cat "$tmpdir/c2.log"
    exit 1
}
"$AETHERC" "$tmpdir/repro_bare.ae" "$tmpdir/repro_bare.c" >"$tmpdir/c3.log" 2>&1 || {
    echo "  [FAIL] repro_bare.ae (without @aether) did not compile"
    cat "$tmpdir/c3.log"
    exit 1
}

# Static check: the @aether-annotated repro must NOT contain
# aether_string_data( in the consume_binary call site, the bare
# repro MUST contain it.
if grep -E "consume_binary\(aether_string_data" "$tmpdir/repro_good.c" >/dev/null; then
    echo "  [FAIL] @aether extern still emits aether_string_data() unwrap"
    grep -n "consume_binary" "$tmpdir/repro_good.c"
    exit 1
fi
if ! grep -E "consume_binary\(aether_string_data" "$tmpdir/repro_bare.c" >/dev/null; then
    echo "  [FAIL] bare extern lost its aether_string_data() unwrap (regressed #297)"
    grep -n "consume_binary" "$tmpdir/repro_bare.c"
    exit 1
fi

# Runtime check: the @aether-annotated repro must round-trip the
# binary content intact. Link both .c via gcc against libaether.a.
INC=$(find "$PREFIX/include/aether" -type d 2>/dev/null | sed 's|^|-I|' | tr '\n' ' ')
gcc -O2 -w $INC \
    "$tmpdir/repro_good.c" "$tmpdir/helper.c" \
    -L"$PREFIX/lib/aether" -laether \
    -Wl,--allow-multiple-definition \
    -pthread -lm -ldl \
    -o "$tmpdir/repro_good" 2>"$tmpdir/link_good.err" || {
    echo "  [FAIL] link of @aether repro failed"
    head -5 "$tmpdir/link_good.err"
    exit 1
}

# Run and check the output. Receiver must see length=7 and byte5=14.
output=$("$tmpdir/repro_good" 2>&1)
if ! echo "$output" | grep -q "len=7 byte5=14"; then
    echo "  [FAIL] @aether extern did not round-trip binary content"
    echo "    expected: len=7 byte5=14"
    echo "    got:      $output"
    exit 1
fi

echo "  [PASS] aether_extern_annotation"
exit 0
