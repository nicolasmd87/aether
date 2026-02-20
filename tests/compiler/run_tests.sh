#!/usr/bin/env bash
# Aether Compiler Tests Runner
# Compiles ALL compiler tests together with test harness

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

echo ""
echo "=================================="
echo "  Aether Compiler Tests"
echo "=================================="
echo ""

# All compiler test files (except test_arrays which has its own main)
TEST_FILES=(
    "tests/compiler/test_lexer.c"
    "tests/compiler/test_lexer_comprehensive.c"
    "tests/compiler/test_parser.c"
    "tests/compiler/test_parser_comprehensive.c"
    "tests/compiler/test_typechecker.c"
    "tests/compiler/test_type_inference_comprehensive.c"
    "tests/compiler/test_switch_statements.c"
    "tests/compiler/test_pattern_matching_comprehensive.c"
    "tests/compiler/test_codegen.c"
    "tests/compiler/test_structs.c"
)

# Test harness and main
FRAMEWORK_FILES=(
    "tests/compiler/test_harness.c"
    "tests/compiler/test_runner_main.c"
)

# Compiler dependencies
COMPILER_DEPS=(
    "compiler/parser/lexer.c"
    "compiler/parser/parser.c"
    "compiler/analysis/typechecker.c"
    "compiler/analysis/type_inference.c"
    "compiler/codegen/codegen.c"
    "compiler/codegen/codegen_expr.c"
    "compiler/codegen/codegen_stmt.c"
    "compiler/codegen/codegen_actor.c"
    "compiler/codegen/codegen_func.c"
    "compiler/codegen/optimizer.c"
    "compiler/ast.c"
    "compiler/aether_error.c"
    "compiler/aether_module.c"
    "compiler/aether_diagnostics.c"
    "runtime/memory/memory.c"
    "runtime/actors/aether_message_registry.c"
)

ALL_SOURCES=("${TEST_FILES[@]}" "${FRAMEWORK_FILES[@]}" "${COMPILER_DEPS[@]}")
INCLUDES="-I. -Icompiler -Iruntime -Itests/compiler"
FLAGS="-std=c11 -Wno-unused-parameter -Wno-unused-function"
OUTPUT="tests/compiler/compiler_tests_all"

echo "Compiling all compiler tests together..."

cd "$ROOT_DIR"

# Compile all tests
if gcc "${ALL_SOURCES[@]}" -o "$OUTPUT" $INCLUDES $FLAGS 2>&1; then
    echo "Compilation successful!"
    echo ""

    # Run all tests
    if ./"$OUTPUT"; then
        main_tests_result=0
    else
        main_tests_result=1
    fi

    # Compile and run test_arrays separately (it has its own main)
    echo ""
    echo "Compiling test_arrays (standalone)..."
    if gcc tests/compiler/test_arrays.c "${COMPILER_DEPS[@]}" -o tests/compiler/test_arrays $INCLUDES $FLAGS 2>&1; then
        if ./tests/compiler/test_arrays; then
            arrays_result=0
        else
            arrays_result=1
        fi
    else
        echo "test_arrays compilation failed!"
        arrays_result=1
    fi

    # Cleanup
    rm -f "$OUTPUT" tests/compiler/test_arrays

    if [ $main_tests_result -eq 0 ] && [ $arrays_result -eq 0 ]; then
        echo ""
        echo "=================================="
        echo "  Test Summary"
        echo "=================================="
        echo "All compiler tests passed!"
        echo "=================================="
        exit 0
    else
        echo ""
        echo "Some tests failed."
        exit 1
    fi
else
    echo "Compilation failed!"
    exit 1
fi
