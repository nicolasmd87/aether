#!/bin/sh
# Tests that the #297 auto-unwrap (`aether_string_data(arg)` injected
# at C extern call sites for `string`-typed parameters) fires
# regardless of whether the extern is declared inline in the same
# .ae file or reached through `import std.X`.
#
# Pre-fix (the bug): module-imported externs were stored in the
# registry under their bare extern name (e.g.
# `http_response_set_body_n`), but call sites reaching them via
# `import std.http; http.response_set_body_n(...)` searched the
# registry for the dotted form (`http.response_set_body_n`). The
# strcmp in find_extern_registry_index missed, is_extern_func
# returned false, and the auto-unwrap branch in codegen_expr.c
# was skipped. Downstream ports duplicated stdlib extern decls
# inline as a workaround.
#
# Post-fix: find_extern_registry_index tries both the exact form
# and the dot-normalised form (replacing `.` with `_`). Module-
# qualified call sites now match the underscore-named registry
# entry.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AE="$ROOT/build/ae"

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

cd "$SCRIPT_DIR" || exit 1

if ! "$AE" build probe.ae -o "$tmpdir/probe" >"$tmpdir/build.log" 2>&1; then
    echo "  [FAIL] module_extern_auto_unwrap: ae build failed"
    sed 's/^/    /' "$tmpdir/build.log" | head -20
    exit 1
fi

if ! "$tmpdir/probe" > "$tmpdir/run.out" 2>&1 || ! grep -q "^OK$" "$tmpdir/run.out"; then
    echo "  [FAIL] module_extern_auto_unwrap: binary did not print OK"
    sed 's/^/    /' "$tmpdir/run.out" | head -10
    exit 1
fi

echo '  [PASS] module_extern_auto_unwrap: stdlib-imported extern auto-unwraps `string` arg'
exit 0
