#!/bin/bash
# AetherUIDriver — AetherUIDriver script for aether_ui — drives the testable app via HTTP.
#
# Usage:
#   ./build/perry_testable &       # start the app
#   ./contrib/aether_ui/test_automation.sh   # run the tests
#   kill %1                         # stop the app
#
# Requires: curl, jq (optional for pretty output)

set -e

PORT="${1:-9222}"
BASE="http://127.0.0.1:$PORT"
PASS=0
FAIL=0

assert_eq() {
    local desc="$1" expected="$2" actual="$3"
    if [ "$expected" = "$actual" ]; then
        echo "  PASS: $desc"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: $desc (expected '$expected', got '$actual')"
        FAIL=$((FAIL + 1))
    fi
}

assert_contains() {
    local desc="$1" needle="$2" haystack="$3"
    if echo "$haystack" | grep -q "$needle"; then
        echo "  PASS: $desc"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: $desc (expected to contain '$needle')"
        FAIL=$((FAIL + 1))
    fi
}

assert_status() {
    local desc="$1" expected="$2" actual="$3"
    assert_eq "$desc" "$expected" "$actual"
}

echo "=== AetherUIDriver ==="
echo "Target: $BASE"
echo ""

# Wait for server to be ready
echo "Waiting for test server..."
for i in $(seq 1 30); do
    if curl -s "$BASE/widgets" > /dev/null 2>&1; then
        echo "Server ready."
        break
    fi
    sleep 0.2
done

# 1. List all widgets
echo ""
echo "--- Test 1: Widget listing ---"
WIDGETS=$(curl -s "$BASE/widgets")
assert_contains "widgets list is JSON array" '"type"' "$WIDGETS"
assert_contains "has text widget" '"text"' "$WIDGETS"
assert_contains "has button widget" '"button"' "$WIDGETS"
assert_contains "banner present" '"banner":true' "$WIDGETS"

# 2. Check initial state
echo ""
echo "--- Test 2: Initial state ---"
STATE=$(curl -s "$BASE/state/1")
assert_contains "counter state exists" '"value"' "$STATE"
assert_contains "counter is 0" '0.000000' "$STATE"

# 3. Click the +1 button
echo ""
echo "--- Test 3: Click +1 button ---"
# Find the +1 button handle
PLUS_BTN=$(echo "$WIDGETS" | grep -o '"id":[0-9]*,"type":"button","text":"+1"' | grep -o '"id":[0-9]*' | grep -o '[0-9]*')
if [ -n "$PLUS_BTN" ]; then
    RESULT=$(curl -s -X POST "$BASE/widget/$PLUS_BTN/click")
    assert_contains "click succeeded" '"ok":true' "$RESULT"
    sleep 0.1
    STATE=$(curl -s "$BASE/state/1")
    assert_contains "counter is now 1" '1.000000' "$STATE"

    # Click again
    curl -s -X POST "$BASE/widget/$PLUS_BTN/click" > /dev/null
    sleep 0.1
    STATE=$(curl -s "$BASE/state/1")
    assert_contains "counter is now 2" '2.000000' "$STATE"
else
    echo "  SKIP: +1 button not found"
fi

# 4. Set state directly
echo ""
echo "--- Test 4: Set state via API ---"
RESULT=$(curl -s -X POST "$BASE/state/1/set?v=42")
assert_contains "state set succeeded" '"ok":true' "$RESULT"
sleep 0.1
STATE=$(curl -s "$BASE/state/1")
assert_contains "counter is now 42" '42.000000' "$STATE"

# 5. Sealed widget — should be rejected
echo ""
echo "--- Test 5: Sealed widget protection ---"
DANGER_BTN=$(echo "$WIDGETS" | grep -o '"id":[0-9]*,"type":"button","text":"Delete Everything"' | grep -o '"id":[0-9]*' | grep -o '[0-9]*')
if [ -n "$DANGER_BTN" ]; then
    STATUS=$(curl -s -o /dev/null -w '%{http_code}' -X POST "$BASE/widget/$DANGER_BTN/click")
    assert_status "sealed widget returns 403" "403" "$STATUS"
    BODY=$(curl -s -X POST "$BASE/widget/$DANGER_BTN/click")
    assert_contains "error says sealed" '"widget is sealed"' "$BODY"
else
    echo "  SKIP: danger button not found"
fi

# 6. Banner protection
echo ""
echo "--- Test 6: Banner cannot be manipulated ---"
BANNER_ID=$(echo "$WIDGETS" | grep -o '"id":[0-9]*[^}]*"banner":true' | grep -o '"id":[0-9]*' | grep -o '[0-9]*')
if [ -n "$BANNER_ID" ]; then
    STATUS=$(curl -s -o /dev/null -w '%{http_code}' -X POST "$BASE/widget/$BANNER_ID/set_text?v=hacked")
    assert_status "banner manipulation returns 403" "403" "$STATUS"
else
    echo "  SKIP: banner not found in widget list"
fi

# 7. Toggle
echo ""
echo "--- Test 7: Toggle widget ---"
TOGGLE=$(echo "$WIDGETS" | grep -o '"id":[0-9]*,"type":"toggle"' | grep -o '"id":[0-9]*' | grep -o '[0-9]*')
if [ -n "$TOGGLE" ]; then
    RESULT=$(curl -s -X POST "$BASE/widget/$TOGGLE/toggle")
    assert_contains "toggle succeeded" '"ok":true' "$RESULT"
else
    echo "  SKIP: toggle not found"
fi

# 8. Set text on textfield
echo ""
echo "--- Test 8: Set textfield text ---"
TF=$(echo "$WIDGETS" | grep -o '"id":[0-9]*,"type":"textfield"' | grep -o '"id":[0-9]*' | grep -o '[0-9]*')
if [ -n "$TF" ]; then
    RESULT=$(curl -s -X POST "$BASE/widget/$TF/set_text?v=automated")
    assert_contains "set_text succeeded" '"ok":true' "$RESULT"
    sleep 0.1
    INFO=$(curl -s "$BASE/widget/$TF")
    assert_contains "textfield has new text" '"automated"' "$INFO"
else
    echo "  SKIP: textfield not found"
fi

# Summary
echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
[ "$FAIL" -eq 0 ] && exit 0 || exit 1
