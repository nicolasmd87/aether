#!/usr/bin/env bash
# coverage_html.sh — one-command HTML coverage report.
#
# Looks for gcovr in this order:
#   1. system-wide `gcovr` on PATH
#   2. project-local venv at build/cov-venv/ (created on first run)
#   3. fall through to a clear "install yourself" message
#
# Why a venv: modern Debian / Ubuntu 24.04+ refuse `pip install` system-
# wide without --break-system-packages; pipx isn't always installed.
# A venv inside build/ keeps the project self-contained, doesn't touch
# user packages, and can be wiped with `make clean`.
#
# Inputs:
#   - build/cov-obj/*.gcno + *.gcda from `make ci-coverage` (must
#     have run first; this script depends on the report driver
#     having scanned the data already).
#
# Outputs:
#   - build/coverage/index.html             — gcovr's HTML report
#   - build/coverage/coverage.json          — gcovr's JSON dump
#                                             for CI / dashboards

set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT_DIR="$ROOT/build/coverage"
VENV_DIR="$ROOT/build/cov-venv"

if [ ! -d "$ROOT/build/cov-obj" ]; then
    echo "  No build/cov-obj/ — run 'make ci-coverage' first."
    exit 1
fi

mkdir -p "$OUT_DIR"

# Locate gcovr.
GCOVR=""
if command -v gcovr >/dev/null 2>&1; then
    GCOVR="gcovr"
elif [ -x "$VENV_DIR/bin/gcovr" ]; then
    GCOVR="$VENV_DIR/bin/gcovr"
fi

if [ -z "$GCOVR" ]; then
    echo "==================================="
    echo "  gcovr not found — bootstrapping local venv"
    echo "==================================="
    if ! command -v python3 >/dev/null 2>&1; then
        echo "  Error: python3 not on PATH. Install your distro's python3,"
        echo "  or 'apt/dnf/brew install gcovr' to skip the venv path."
        exit 1
    fi
    if [ ! -d "$VENV_DIR" ]; then
        echo "  Creating venv at $VENV_DIR..."
        if ! python3 -m venv "$VENV_DIR" 2>/tmp/venv.err; then
            echo "  python3 -m venv failed:"
            cat /tmp/venv.err | head -5
            echo ""
            echo "  Two ways forward:"
            echo "    1. apt install python3-venv  (Debian/Ubuntu)"
            echo "       dnf install python3       (Fedora — usually includes venv)"
            echo "    2. apt install gcovr  /  dnf install gcovr  /  brew install gcovr"
            echo "       (system-package path, skips the venv entirely)"
            exit 1
        fi
    fi
    echo "  Installing gcovr into venv..."
    if ! "$VENV_DIR/bin/pip" install --quiet gcovr 2>/tmp/pip.err; then
        echo "  pip install gcovr failed:"
        cat /tmp/pip.err | head -10
        exit 1
    fi
    GCOVR="$VENV_DIR/bin/gcovr"
    echo "  ✓ gcovr installed at $GCOVR"
    echo ""
fi

echo "==================================="
echo "  Generating HTML coverage report"
echo "==================================="
echo "  Using: $GCOVR ($("$GCOVR" --version 2>&1 | head -1))"
echo ""

cd "$ROOT"

# gcovr walks .gcno/.gcda under the search root and produces both
# HTML and JSON. The --filter restricts to the source-tree areas we
# actually own — drops third-party headers, system libc, build/
# scaffolding. Per-file detail HTML lands under build/coverage/
# (one file per source) plus an index.html linking them.
# --merge-mode-functions=merge-use-line-min handles the case where
# the same function name (typically `main`) appears in multiple .ae
# files at different line numbers — every test program defines its
# own `main`, and gcovr's default strict mode rejects that as a
# merge conflict. -min merges by collapsing onto the smallest line
# number (consistent attribution across reports).
"$GCOVR" \
    --root "$ROOT" \
    --filter '(std|runtime|compiler|tools|lsp)/' \
    --merge-mode-functions=merge-use-line-min \
    --gcov-ignore-errors=source_not_found \
    --gcov-ignore-errors=no_working_dir_found \
    --gcov-ignore-errors=output_error \
    --gcov-ignore-parse-errors=negative_hits.warn_once_per_file \
    --html-details "$OUT_DIR/index.html" \
    --json --output "$OUT_DIR/coverage.json" \
    --print-summary \
    2>&1 | grep -vE "^(\(WARNING\)|warning:)" || true

echo ""
echo "  HTML report: file://$OUT_DIR/index.html"
echo "  JSON data:   $OUT_DIR/coverage.json"
echo ""
echo "  Open with:"
echo "    xdg-open $OUT_DIR/index.html        # Linux"
echo "    open     $OUT_DIR/index.html        # macOS"
