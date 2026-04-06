#!/bin/bash
# Build the Aether Java sandbox agent
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
OUT="$DIR/../../../build"

mkdir -p "$OUT/java-classes"

echo "Compiling Java sandbox agent..."
javac -d "$OUT/java-classes" \
    "$DIR/src/AetherGrantChecker.java" \
    "$DIR/src/AetherSandboxHooks.java" \
    "$DIR/src/AetherSandboxAgent.java" \
    "$DIR/src/AetherMap.java" \
    "$DIR/src/SandboxTest.java"

echo "Packaging aether-sandbox.jar..."
jar cfm "$OUT/aether-sandbox.jar" "$DIR/MANIFEST.MF" \
    -C "$OUT/java-classes" .

echo "✓ Built: $OUT/aether-sandbox.jar"
