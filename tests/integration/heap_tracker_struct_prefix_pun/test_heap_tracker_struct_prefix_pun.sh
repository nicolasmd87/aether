#!/bin/sh
# A wide struct written through a narrow prefix-view pointer keeps its fields
# intact (#1879-regression).
#
# #1879 appended `_heap_<field>` trackers after a struct's declared fields.
# Two structs sharing a named prefix (the punning idiom: allocate the wide
# one, write it through the narrow prefix type) then disagreed on the
# tracker's offset, so a string write through the narrow view stamped a real
# data field of the wide object -- silent corruption. The fix places each
# tracker immediately after its string field, making the narrow struct a true
# memory prefix of the wide one.
#
# Two assertions: the program prints "ok" at runtime (a clobbered field prints
# CLOBBERED), and -- so this means something with no run -- the two structs
# agree on the tracker's position in the generated C.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
AE="$ROOT/build/ae"
AETHERC="$ROOT/build/aetherc"

TMP="$(mktemp -d)"
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT

if ! "$AETHERC" "$SCRIPT_DIR/prog.ae" "$TMP/out.c" > "$TMP/gen.log" 2>&1; then
    echo "  [FAIL] heap_tracker_struct_prefix_pun: codegen failed"
    sed 's/^/        /' "$TMP/gen.log" | head -8
    exit 1
fi

# The tracker must sit at the same field index in both structs. With inline
# placement CommonOptions is { event_id; _heap_event_id; retry_ms }, so
# `_heap_event_id` precedes `retry_ms` -- i.e. the narrow struct is a genuine
# prefix of the wide one. If the tracker were appended (the bug), it would
# follow retry_ms in CommonOptions and follow only_if_missing in SignalOptions,
# at different offsets.
common_body=$(awk '/typedef struct CommonOptions \{/{p=1} p{print} /} CommonOptions;/{if(p)exit}' "$TMP/out.c")
if ! printf '%s\n' "$common_body" | grep -q '_heap_event_id'; then
    echo "  [FAIL] heap_tracker_struct_prefix_pun: no tracker in CommonOptions"
    printf '%s\n' "$common_body" | sed 's/^/        /'
    exit 1
fi
# _heap_event_id must come BEFORE retry_ms in CommonOptions (inline placement).
heap_line=$(printf '%s\n' "$common_body" | grep -n '_heap_event_id' | head -1 | cut -d: -f1)
retry_line=$(printf '%s\n' "$common_body" | grep -n 'retry_ms' | head -1 | cut -d: -f1)
if [ "$heap_line" -gt "$retry_line" ]; then
    echo "  [FAIL] heap_tracker_struct_prefix_pun: tracker appended, not inline --"
    echo "         CommonOptions is not a memory prefix of SignalOptions, the pun corrupts"
    printf '%s\n' "$common_body" | sed 's/^/        /'
    exit 1
fi

# Compile and run: the field must survive the punned string write.
if ! "$AE" run "$SCRIPT_DIR/prog.ae" > "$TMP/run.log" 2>&1; then
    echo "  [FAIL] heap_tracker_struct_prefix_pun: program failed to run"
    sed 's/^/        /' "$TMP/run.log" | head -8
    exit 1
fi
if ! grep -q '^ok:' "$TMP/run.log"; then
    echo "  [FAIL] heap_tracker_struct_prefix_pun: field was clobbered"
    sed 's/^/        /' "$TMP/run.log" | head -4
    exit 1
fi

echo "  [PASS] heap_tracker_struct_prefix_pun: prefix-view string write leaves the wide field intact"
