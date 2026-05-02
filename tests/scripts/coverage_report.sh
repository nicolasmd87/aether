#!/usr/bin/env bash
# coverage_report.sh — walk .gcda files produced by a coverage-instrumented
# test run, invoke gcov, and produce a per-file + grand-total summary.
#
# Inputs:
#   - .gcno files alongside .o objects under build/cov-obj/
#   - .gcda files emitted by running the test binaries (sit next to the
#     .gcno files because gcc records absolute paths in the .gcno)
#
# Outputs:
#   - build/coverage/*.gcov     (per-source coverage reports)
#   - build/coverage/SUMMARY    (per-file lines hit/total + percentage)
#
# Phase 1's #line directives mean gcov walks each .c, follows the
# directives, and produces both `<file>.c.gcov` and the more useful
# `<file>.ae.gcov` showing line + branch hits attributed back to the
# original Aether source.
#
# Reports are bucketed by source-tree role:
#   stdlib   — std/* hits  (which stdlib lines run during testing)
#   runtime  — runtime/*   (actor / scheduler / RAII glue)
#   compiler — compiler/*  (which compiler paths are exercised)
#   tests    — tests/*.ae  (which test files actually ran)

set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
COV_OBJ_DIR="$ROOT/build/cov-obj"
OUT_DIR="$ROOT/build/coverage"

if [ ! -d "$COV_OBJ_DIR" ]; then
    echo "  No build/cov-obj/ — run 'make ci-coverage' first."
    exit 1
fi

mkdir -p "$OUT_DIR"
# Run gcov from the repo root so source paths in the .gcno/.gcda files
# (which gcc recorded as repo-relative when objects were compiled)
# resolve correctly. The -p flag below preserves full paths in output
# names, so concurrent .gcov writes don't collide. Output files land
# in $OUT_DIR via an explicit move at the end.
cd "$ROOT"

# Collect all .gcda files. Two sources:
#   1. build/cov-obj/  — the C-level test runner's stdlib + runtime
#      + compiler instrumentation (built via `stdlib-cov` Makefile
#      pattern with `gcc --coverage`).
#   2. build/          — per-.ae-test program .gcda files written by
#      `ae build --coverage` (the AE_BUILD_FLAGS env var injects the
#      flag into every `make test-ae` per-test build). Each test
#      program's .gcda sits next to its binary in build/.
# gcov needs to be invoked one .gcda at a time because it dedupes
# inputs across one command line.
gcda_count=$(
    {
        find "$COV_OBJ_DIR" -name '*.gcda' 2>/dev/null
        find build -maxdepth 1 -name '*.gcda' 2>/dev/null
    } | wc -l
)
if [ "$gcda_count" -eq 0 ]; then
    echo "  No .gcda files found — did the tests actually run?"
    exit 1
fi

echo "==================================="
echo "  Aether coverage report"
echo "==================================="
echo "  Scanning $gcda_count .gcda files..."
echo ""

# Run gcov on every .gcda. Each invocation must be one .gcda at a
# time — gcov dedupes inputs across one command line, so passing the
# whole batch via xargs causes it to process file #1 and skip the
# rest as "already processed". One gcda → one gcov call.
#
# Flags:
#   -p   preserve full path in output filenames (foo#bar.c.gcov)
#        so two files with the same basename (e.g. std/foo/bar.c
#        and std/baz/bar.c) don't collide in the output dir
#   -b   add branch coverage (uncovered branches show via 0 counts)
#   -c   show branch counts as numbers, not percentages, so the
#        summary parser can read them mechanically
#   -o <dir>  tell gcov where to find the .gcno (alongside the .gcda).
#
# DON'T pass -r (relative-only): when gcov runs from the repo root
# it filters to "files relative to PWD," which is fine — but if any
# source is referenced by absolute path in the .gcno, gcov produces
# an empty output file. Dropping -r makes the report tolerant.
: > /tmp/coverage_gcov.log
while IFS= read -r -d '' gcda; do
    obj_dir=$(dirname "$gcda")
    base=$(basename "$gcda" .gcda)
    gcov -p -b -c -o "$obj_dir" "$base" >>/tmp/coverage_gcov.log 2>&1 || true
done < <(
    {
        find "$COV_OBJ_DIR" -name '*.gcda' -print0 2>/dev/null
        find build -maxdepth 1 -name '*.gcda' -print0 2>/dev/null
    }
)

# Move all generated .gcov files into the OUT_DIR so they don't
# litter the repo root.
find "$ROOT" -maxdepth 1 -name '*.gcov' -exec mv {} "$OUT_DIR/" \; 2>/dev/null || true

# Summarise. gcov produces one .gcov per source file walked. With
# Phase 1's #line directives, that includes both .c.gcov AND .ae.gcov
# files — the .ae ones are what we actually want.
ae_files=$(find "$OUT_DIR" -name '*.ae.gcov' | wc -l)
c_files=$(find "$OUT_DIR" -name '*.c.gcov' | wc -l)

echo "  Per-source reports written: $OUT_DIR/"
echo "    $ae_files .ae.gcov files (the prize: line + branch hits against .ae)"
echo "    $c_files  .c.gcov  files (raw C-level coverage)"
echo ""

# Compute summary by bucket. gcov's first three lines of each .gcov
# file have summary metadata; lines starting with `<count>:<line>:` are
# real hit counters where count='-' means non-executable, count='#####'
# means executed-zero (instrumented but never reached), and a digit is
# the hit count.
summarise_bucket() {
    local label="$1"
    local pattern="$2"
    local total_lines=0
    local hit_lines=0
    local files=0
    for gcov_file in $(find "$OUT_DIR" -name '*.gcov' | grep -E "$pattern" || true); do
        files=$((files + 1))
        # Skip the metadata header (first 5 lines) and count executable lines.
        # Field separator is `:`; field 1 is the hit count, field 2 the line number.
        local f_total
        local f_hit
        f_total=$(awk -F: 'NR > 5 && $1 !~ /^[[:space:]]*-[[:space:]]*$/ && $2 ~ /[0-9]/ { c++ } END { print c+0 }' "$gcov_file")
        f_hit=$(awk -F: 'NR > 5 && $1 !~ /^[[:space:]]*-[[:space:]]*$/ && $1 !~ /#####/ && $2 ~ /[0-9]/ { c++ } END { print c+0 }' "$gcov_file")
        total_lines=$((total_lines + f_total))
        hit_lines=$((hit_lines + f_hit))
    done
    if [ "$total_lines" -gt 0 ]; then
        local pct=$(awk -v h="$hit_lines" -v t="$total_lines" 'BEGIN { printf "%.1f", (h * 100.0) / t }')
        printf "  %-12s %5d files, %6d / %-6d lines  %5s%%\n" "$label" "$files" "$hit_lines" "$total_lines" "$pct"
    else
        printf "  %-12s 0 files\n" "$label"
    fi
}

echo "  Coverage by source-tree bucket:"
echo "  -----------------------------------"
# We grep the .gcov filename for the source-tree component. gcov
# encodes the source path with `#` separators (escaped slashes), so
# `std#http#aether_http.c.gcov` shows up for `std/http/aether_http.c`.
summarise_bucket "stdlib (.c)"   "std#.*\.c\.gcov$"
summarise_bucket "stdlib (.ae)"  "std#.*\.ae\.gcov$"
summarise_bucket "runtime (.c)"  "runtime#.*\.c\.gcov$"
summarise_bucket "compiler (.c)" "compiler#.*\.c\.gcov$"
summarise_bucket "tests (.ae)"   "tests#.*\.ae\.gcov$"
echo ""

# Write a SUMMARY file for downstream tooling.
{
    echo "# Aether coverage summary"
    echo "# Generated $(date -u +'%Y-%m-%dT%H:%M:%SZ') from $gcda_count .gcda files."
    echo "# Format: <bucket>\t<files>\t<lines_hit>\t<lines_total>\t<percentage>"
    summarise_bucket "stdlib_c"   "std#.*\.c\.gcov$"   | awk '{print $1"\t"$2"\t"$4"\t"$6"\t"$8}'
    summarise_bucket "stdlib_ae"  "std#.*\.ae\.gcov$"  | awk '{print $1"\t"$2"\t"$4"\t"$6"\t"$8}'
    summarise_bucket "runtime_c"  "runtime#.*\.c\.gcov$"  | awk '{print $1"\t"$2"\t"$4"\t"$6"\t"$8}'
    summarise_bucket "compiler_c" "compiler#.*\.c\.gcov$" | awk '{print $1"\t"$2"\t"$4"\t"$6"\t"$8}'
    summarise_bucket "tests_ae"   "tests#.*\.ae\.gcov$"   | awk '{print $1"\t"$2"\t"$4"\t"$6"\t"$8}'
} > "$OUT_DIR/SUMMARY"

echo "  Summary file: $OUT_DIR/SUMMARY"
echo ""

# Optional richer reports if the user has gcovr or lcov installed.
# Probe-and-degrade — same philosophy as contrib_build.sh.
if command -v gcovr >/dev/null 2>&1; then
    echo "  gcovr detected — generating HTML report..."
    cd "$ROOT"
    gcovr --root . \
          --filter '(std|runtime|compiler)/' \
          --html-details "$OUT_DIR/index.html" \
          --print-summary 2>&1 | tail -5
    echo "  HTML report: $OUT_DIR/index.html"
else
    echo "  (Optional) gcovr not installed — skipping HTML report."
    echo "             Install via: pip install gcovr"
fi
echo ""

if command -v lcov >/dev/null 2>&1 && command -v genhtml >/dev/null 2>&1; then
    echo "  lcov + genhtml detected — generating browsable HTML..."
    cd "$ROOT"
    lcov --capture --directory build/cov-obj/ --output-file "$OUT_DIR/coverage.info" --quiet 2>/dev/null || true
    if [ -s "$OUT_DIR/coverage.info" ]; then
        genhtml --output-directory "$OUT_DIR/lcov-html" "$OUT_DIR/coverage.info" --quiet 2>/dev/null || true
        echo "  lcov HTML: $OUT_DIR/lcov-html/index.html"
    fi
else
    echo "  (Optional) lcov not installed — skipping browsable HTML."
fi
echo ""
echo "  Done."
