#!/bin/sh
# Regression: aetherc emits `#line N "src.ae"` directives in generated
# C so gcc errors, gdb breakpoints, and gcov reports point at the
# original .ae source — including across module-import boundaries
# where one merged .c contains code from multiple .ae files.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AETHERC="$ROOT/build/aetherc"

if [ ! -x "$AETHERC" ]; then
    echo "  [SKIP] source_map_line_directives: toolchain not built"
    exit 0
fi

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

# Test 1: Single-file program. Every Aether statement should produce
# a #line directive pointing at the .ae file, so gcov can attribute
# hits to source lines.
cat > "$tmpdir/single.ae" <<'AE'
import std.string

main() {
    x = 42
    y = x + 1
    println("got ${y}")
}
AE

if ! "$AETHERC" "$tmpdir/single.ae" "$tmpdir/single.c" >/dev/null 2>&1; then
    echo "  [FAIL] source_map: single-file aetherc failed"
    exit 1
fi

if ! grep -q '^#line .* "[^"]*single\.ae"' "$tmpdir/single.c"; then
    echo "  [FAIL] source_map: no #line directive references single.ae"
    grep '^#line' "$tmpdir/single.c" | head -5
    exit 1
fi

# Verify lines 4, 5, 6 (the assignments and println) all get #line
# directives — codegen should emit one per Aether statement.
for n in 4 5 6; do
    if ! grep -q "^#line $n \"[^\"]*single\.ae\"" "$tmpdir/single.c"; then
        echo "  [FAIL] source_map: missing #line for single.ae line $n"
        exit 1
    fi
done

# Test 2: Multi-file program (module import). The merged .c should
# carry #line directives that switch files at the import boundary —
# user code points at main.ae, imported helpers point at the module.
mkdir -p "$tmpdir/helper"
cat > "$tmpdir/helper/module.ae" <<'AE'
import std.string

exports (greet)

greet(name: string) -> string {
    return "hello ${name}"
}
AE

cat > "$tmpdir/main.ae" <<'AE'
import std.string
import helper

main() {
    s = helper.greet("world")
    println(s)
}
AE

cd "$tmpdir" || exit 1
if ! "$AETHERC" main.ae main.c >/dev/null 2>&1; then
    echo "  [FAIL] source_map: multi-file aetherc failed"
    exit 1
fi

# Both file paths must appear in the merged C — codegen switches
# files when crossing the import boundary.
if ! grep -q '^#line .* "[^"]*main\.ae"' main.c; then
    echo "  [FAIL] source_map: main.ae #line directives missing in merged .c"
    exit 1
fi
if ! grep -q '^#line .* "[^"]*helper/module\.ae"' main.c; then
    echo "  [FAIL] source_map: helper/module.ae #line directives missing in merged .c"
    grep '^#line' main.c | sort -u
    exit 1
fi

# Dedup check: back-to-back nodes on the same source line should
# emit ONE #line directive, not many. Count the directives for
# main.ae line 5 (the helper.greet call); it should be exactly 1.
n=$(grep -c '^#line 5 "[^"]*main\.ae"' main.c)
if [ "$n" -gt 1 ]; then
    echo "  [FAIL] source_map: dedup broken — line 5 emitted $n times"
    exit 1
fi

echo "  [PASS] source_map_line_directives"
exit 0
