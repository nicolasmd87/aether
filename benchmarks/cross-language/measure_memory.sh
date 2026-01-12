#!/bin/bash
# Measure memory usage of a command using /usr/bin/time on macOS

if [ "$#" -lt 1 ]; then
    echo "Usage: $0 <command> [args...]"
    exit 1
fi

# On macOS, use /usr/bin/time -l to get detailed memory stats
if [[ "$OSTYPE" == "darwin"* ]]; then
    OUTPUT=$(/usr/bin/time -l "$@" 2>&1)
    EXIT_CODE=$?

    # Extract max RSS (in bytes on macOS)
    MAX_RSS=$(echo "$OUTPUT" | grep "maximum resident set size" | awk '{print $1}')

    # Convert to MB (macOS reports in bytes)
    if [ -n "$MAX_RSS" ]; then
        MAX_RSS_MB=$(echo "scale=2; $MAX_RSS / 1024 / 1024" | bc)
        echo "MAX_RSS_MB=$MAX_RSS_MB"
    fi

    # Print the actual output
    echo "$OUTPUT" | grep -v "maximum resident set size"

    exit $EXIT_CODE
else
    # On Linux, use /usr/bin/time -v
    OUTPUT=$(/usr/bin/time -v "$@" 2>&1)
    EXIT_CODE=$?

    # Extract max RSS (in KB on Linux)
    MAX_RSS=$(echo "$OUTPUT" | grep "Maximum resident set size" | awk '{print $6}')

    # Convert to MB
    if [ -n "$MAX_RSS" ]; then
        MAX_RSS_MB=$(echo "scale=2; $MAX_RSS / 1024" | bc)
        echo "MAX_RSS_MB=$MAX_RSS_MB"
    fi

    # Print the actual output
    echo "$OUTPUT" | grep -v "Maximum resident set size"

    exit $EXIT_CODE
fi
