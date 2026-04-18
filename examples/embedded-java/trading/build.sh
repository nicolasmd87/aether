#!/bin/sh
# Build the worked trading example end to end. Requires:
#   - ae (the Aether toolchain; see top-level README)
#   - javac/java JDK 22+
set -e
cd "$(dirname "$0")"

echo "[1/3] Building Aether namespace..."
ae build --namespace aether/

echo "[2/3] Compiling Java demo..."
mkdir -p build
javac -d build \
    java/src/main/java/TradingDemo.java \
    aether/com/example/trading/Trading.java

echo "[3/3] Running TradingDemo..."
java --enable-native-access=ALL-UNNAMED -cp build \
    TradingDemo aether/libtrading.so
