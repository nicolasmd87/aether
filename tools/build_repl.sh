#!/bin/bash
# Build script for Aether REPL (Linux/macOS)

echo "Building Aether REPL..."

# Compile the REPL
gcc -O2 -Wall tools/aether_repl.c -o aether-repl -lreadline

if [ $? -eq 0 ]; then
    echo "✓ REPL built successfully: aether-repl"
    echo ""
    echo "Run with: ./aether-repl"
else
    echo "✗ Build failed"
    exit 1
fi
