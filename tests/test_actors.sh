#!/bin/bash

set -e

echo "Testing Actor Implementation"
echo "=============================="
echo ""

echo "Test 1: Basic Actor Compilation"
./build/aetherc examples/test_actor_working.ae build/test1.c
gcc -c build/test1.c -Iruntime -o build/test1.o
echo "PASS"
echo ""

echo "Test 2: Multiple Actors"
./build/aetherc examples/test_multiple_actors.ae build/test2.c
gcc -c build/test2.c -Iruntime -o build/test2.o
echo "PASS"
echo ""

echo "Test 3: Verify Generated Code Structure"
if grep -q "self->count" build/test1.c && \
   grep -q "actor_state_machine.h" build/test1.c && \
   grep -q "typedef struct Counter" build/test1.c && \
   grep -q "void Counter_step" build/test1.c; then
    echo "PASS"
else
    echo "FAIL: Generated code missing expected content"
    exit 1
fi
echo ""

echo "All tests passed"
