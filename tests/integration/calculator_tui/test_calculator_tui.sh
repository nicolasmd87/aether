#!/bin/sh
# Test: examples/calculator-tui.ae — end-to-end arithmetic via piped stdin.
# Feeds keypresses ("7+3=q") into the calculator and checks the rendered
# display shows the expected result. Regression coverage for the closure-
# as-operator and mutable-capture rewrites.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AE="$ROOT/build/ae"
SRC="$ROOT/examples/calculator-tui.ae"
BIN="$ROOT/build/test_calculator_tui"

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        echo "  [SKIP] calculator-tui (Windows — piped TTY input unreliable)"
        exit 0
        ;;
esac

if [ ! -x "$AE" ]; then
    echo "  [FAIL] calculator-tui: $AE missing (expected to be built before tests)"
    exit 1
fi

# Build the calculator into a dedicated test binary so this test is
# self-contained and doesn't depend on whether `make examples` has run.
"$AE" build "$SRC" -o "$BIN" >/tmp/calctui_build.log 2>&1 || {
    echo "  [FAIL] calculator-tui: build failed"
    cat /tmp/calctui_build.log
    exit 1
}

pass=0
fail=0

# Run the calculator with `keys` piped to stdin. Strip ANSI escapes, then
# pull the displayed number from the LAST render's header. The display row
# is the 3rd line after each "  Calculator" header:
#    "  Calculator" / (blank) / "    <N>                "
# We grab the display from every render and keep the last one — that's
# the post-keypress state.
run_case() {
    name="$1"; keys="$2"; expected="$3"
    actual=$(printf '%s' "$keys" | "$BIN" 2>&1 \
        | sed 's/\x1b\[[0-9;]*[a-zA-Z]//g' \
        | awk '/^  Calculator/{getline; getline; last=$0} END{print last}' \
        | tr -d ' ')
    if [ "$actual" = "$expected" ]; then
        echo "  [PASS] calculator-tui: $name ($keys → $expected)"
        pass=$((pass + 1))
    else
        echo "  [FAIL] calculator-tui: $name (expected $expected, got '$actual')"
        fail=$((fail + 1))
    fi
}

# Digit entry
run_case "single digit"     '7q' 7
run_case "two digits"       '42q' 42
run_case "three digits"     '123q' 123

# Arithmetic — each operator
run_case "addition"         '7+3=q'   10
run_case "subtraction"      '9-4=q'   5
run_case "multiplication"   '6*7=q'   42
run_case "division"         '9/3=q'   3

# Multi-digit operands
run_case "multi-digit add"  '12+34=q' 46

# Chained: 5*3 then press = (one op at a time, calculator semantics)
run_case "5 times 3 equals" '5*3=q' 15

# Division by zero is the `over` closure's guard: returns a when b == 0
run_case "div by zero safe" '7/0=q' 7

# Clear key resets the display
run_case "clear after entry" '42cq' 0

echo ""
echo "calculator-tui: $pass passed, $fail failed"
rm -f "$BIN"
if [ "$fail" -gt 0 ]; then exit 1; fi
