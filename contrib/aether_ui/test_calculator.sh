#!/bin/bash
# AetherUIDriver tests for the calculator.
#
# Usage:
#   AETHER_UI_TEST_PORT=9222 ./build/calculator &
#   ./contrib/aether_ui/test_calculator.sh
#   kill %1

set -e

PORT="${1:-9222}"
BASE="http://127.0.0.1:$PORT"
PASS=0
FAIL=0

assert_display() {
    local desc="$1" expected="$2"
    sleep 0.05
    actual=$(curl -s "$BASE/state/1" | grep -oP '"value":\K[0-9.-]+')
    # Truncate .000000
    actual_int=$(echo "$actual" | sed 's/\.0*$//')
    if [ "$actual_int" = "$expected" ]; then
        echo "  PASS: $desc (display = $expected)"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: $desc (expected $expected, got $actual_int)"
        FAIL=$((FAIL + 1))
    fi
}

WIDGET_CACHE=""
cache_widgets() {
    WIDGET_CACHE=$(curl -s "$BASE/widgets")
}

click_btn() {
    local label="$1"
    [ -z "$WIDGET_CACHE" ] && cache_widgets
    # Use Python for reliable JSON + special char handling
    local id=$(echo "$WIDGET_CACHE" | python3 -c "
import sys, json
widgets = json.load(sys.stdin)
for w in widgets:
    if w.get('type') == 'button' and w.get('text') == '$label':
        print(w['id']); break
" 2>/dev/null)
    if [ -z "$id" ]; then
        echo "  WARN: button '$label' not found"
        return 1
    fi
    curl -s -X POST "$BASE/widget/$id/click" > /dev/null
}

echo "=== AetherUIDriver: Calculator Tests ==="
echo "Target: $BASE"
echo ""

# Wait for server
for i in $(seq 1 30); do
    curl -s "$BASE/widgets" > /dev/null 2>&1 && break
    sleep 0.2
done

# --- Basic digit entry ---
echo "--- Digit entry ---"
click_btn "4"
click_btn "2"
assert_display "4 then 2 = 42" "42"

# --- Clear ---
echo "--- Clear ---"
click_btn "C"
assert_display "C resets to 0" "0"

# --- Addition: 7 + 3 = 10 ---
echo "--- Addition ---"
click_btn "7"
click_btn "+"
click_btn "3"
click_btn "="
assert_display "7 + 3 = 10" "10"

click_btn "C"

# --- Subtraction: 9 - 4 = 5 ---
echo "--- Subtraction ---"
click_btn "9"
click_btn "-"
click_btn "4"
click_btn "="
assert_display "9 - 4 = 5" "5"

click_btn "C"

# --- Multiplication: 6 * 8 = 48 ---
echo "--- Multiplication ---"
click_btn "6"
click_btn "*"
click_btn "8"
click_btn "="
assert_display "6 * 8 = 48" "48"

click_btn "C"

# --- Division: 9 / 3 = 3 ---
echo "--- Division ---"
click_btn "9"
click_btn "/"
click_btn "3"
click_btn "="
assert_display "9 / 3 = 3" "3"

click_btn "C"

# --- Multi-digit: 12 + 34 = 46 ---
echo "--- Multi-digit ---"
click_btn "1"
click_btn "2"
click_btn "+"
click_btn "3"
click_btn "4"
click_btn "="
assert_display "12 + 34 = 46" "46"

click_btn "C"

# --- Chained: 5 * 3 = 15, then + 5 = 20 ---
echo "--- Chained operations ---"
click_btn "5"
click_btn "*"
click_btn "3"
click_btn "="
assert_display "5 * 3 = 15" "15"
click_btn "+"
click_btn "5"
click_btn "="
assert_display "15 + 5 = 20" "20"

click_btn "C"

# --- Division by zero: 7 / 0 = 7 (no crash) ---
echo "--- Division by zero ---"
click_btn "7"
click_btn "/"
click_btn "0"
click_btn "="
assert_display "7 / 0 = 7 (safe)" "7"

click_btn "C"

# --- Under Remote Control banner present ---
echo "--- Banner ---"
WIDGETS=$(curl -s "$BASE/widgets")
if echo "$WIDGETS" | grep -q '"banner":true'; then
    echo "  PASS: Under Remote Control banner present"
    PASS=$((PASS + 1))
else
    echo "  FAIL: banner not found"
    FAIL=$((FAIL + 1))
fi

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
[ "$FAIL" -eq 0 ] && exit 0 || exit 1
