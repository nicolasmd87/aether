#!/bin/bash
# Build a Aether UI application.
# Usage: ./build.sh <source.ae> [output_binary]
#
# Automatically selects the platform backend:
#   macOS  → aether_ui_macos.m  (AppKit)
#   Linux  → aether_ui_gtk4.c   (GTK4, requires libgtk-4-dev)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
AETHER_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
AETHERC="$AETHER_ROOT/build/aetherc"

SOURCE="${1:?Usage: $0 <source.ae> [output_binary]}"
OUTPUT="${2:-build/$(basename "$SOURCE" .ae)}"
C_FILE="${OUTPUT}.c"

AETHER_INCLUDES="\
    -I$AETHER_ROOT/runtime \
    -I$AETHER_ROOT/runtime/actors \
    -I$AETHER_ROOT/runtime/scheduler \
    -I$AETHER_ROOT/runtime/utils \
    -I$AETHER_ROOT/runtime/memory \
    -I$AETHER_ROOT/runtime/config \
    -I$AETHER_ROOT/std \
    -I$AETHER_ROOT/std/string \
    -I$AETHER_ROOT/std/io \
    -I$AETHER_ROOT/std/net \
    -I$AETHER_ROOT/std/collections \
    -I$AETHER_ROOT/std/json \
    -I$AETHER_ROOT/std/fs \
    -I$AETHER_ROOT/std/log"

echo "Compiling $SOURCE -> $C_FILE"
"$AETHERC" "$SOURCE" "$C_FILE"

OS="$(uname -s)"
case "$OS" in
    Darwin)
        echo "Platform: macOS (AppKit)"
        clang -O0 -g -fobjc-arc \
            $AETHER_INCLUDES \
            "$C_FILE" "$SCRIPT_DIR/aether_ui_macos.m" \
            -L"$AETHER_ROOT/build" -laether \
            -o "$OUTPUT" \
            -framework AppKit -framework Foundation -pthread -lm
        ;;
    Linux)
        if ! pkg-config --exists gtk4 2>/dev/null; then
            echo "Error: GTK4 dev libraries not found."
            echo "Install with: sudo apt install libgtk-4-dev"
            exit 1
        fi
        echo "Platform: Linux (GTK4)"
        gcc -O0 -g -pipe \
            $(pkg-config --cflags gtk4) \
            $AETHER_INCLUDES \
            "$C_FILE" "$SCRIPT_DIR/aether_ui_gtk4.c" \
            -L"$AETHER_ROOT/build" -laether \
            -o "$OUTPUT" \
            -pthread -lm $(pkg-config --libs gtk4)
        ;;
    *)
        echo "Error: Unsupported platform '$OS'."
        echo "Aether UI supports macOS (AppKit) and Linux (GTK4)."
        exit 1
        ;;
esac

echo "Built: $OUTPUT"
