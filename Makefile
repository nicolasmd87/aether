.PHONY: all clean test compiler examples

# Detect OS
ifeq ($(OS),Windows_NT)
    DETECTED_OS := Windows
    EXE_EXT := .exe
    PATH_SEP := \\
    MKDIR := if not exist build mkdir build
    RM := del /Q
    RM_DIR := rd /S /Q
else
    DETECTED_OS := $(shell uname -s)
    EXE_EXT :=
    PATH_SEP := /
    MKDIR := mkdir -p
    RM := rm -f
    RM_DIR := rm -rf
endif

CC = gcc
CFLAGS = -O2 -Icompiler -Iruntime -Wall
LDFLAGS = -pthread

COMPILER_SRC = compiler/aetherc.c compiler/lexer.c compiler/parser.c compiler/ast.c compiler/typechecker.c compiler/codegen.c compiler/aether_error.c compiler/aether_module.c compiler/type_inference.c
RUNTIME_SRC = runtime/multicore_scheduler.c runtime/memory.c runtime/aether_string.c runtime/aether_io.c runtime/aether_math.c runtime/aether_supervision.c runtime/aether_tracing.c runtime/aether_bounds_check.c runtime/aether_test.c

# Exclude test files that have main() functions
TEST_SRC = $(filter-out tests/test_runtime_manual.c, $(wildcard tests/test_*.c))

all: compiler

compiler:
	@$(MKDIR)
	$(CC) $(CFLAGS) $(COMPILER_SRC) -o build/aetherc$(EXE_EXT)

test: compiler
	@echo "==================================="
	@echo "Building Test Suite ($(DETECTED_OS))"
	@echo "==================================="
	$(CC) $(CFLAGS) $(TEST_SRC) $(COMPILER_SRC) -Icompiler -o build/test_runner$(EXE_EXT)
	@echo ""
	@echo "==================================="
	@echo "Running Tests"
	@echo "==================================="
	./build/test_runner$(EXE_EXT)

test-manual-runtime: compiler
	@echo "Building manual runtime test..."
	$(CC) $(CFLAGS) tests/test_runtime_manual.c $(RUNTIME_SRC) $(LDFLAGS) -o build/test_runtime_manual$(EXE_EXT)
	@echo "Running manual runtime test..."
	./build/test_runtime_manual$(EXE_EXT)

benchmark: compiler
	@echo "Single-core benchmark..."
	$(CC) examples/ring_benchmark_manual.c -Iruntime -O2 -o build/ring_bench$(EXE_EXT)
	./build/ring_bench$(EXE_EXT)
	@echo ""
	@echo "Multi-core benchmark..."
	$(CC) examples/multicore_bench.c $(RUNTIME_SRC) -Iruntime $(LDFLAGS) -O2 -o build/mc_bench$(EXE_EXT)
	./build/mc_bench$(EXE_EXT)

examples: compiler
	@echo "Compiling examples..."
	./build/aetherc$(EXE_EXT) examples/test_actor_working.ae build/actor1.c
	./build/aetherc$(EXE_EXT) examples/test_multiple_actors.ae build/actor2.c
	@echo "Examples compiled to build/"

clean:
	$(RM_DIR) build

help:
	@echo "Aether Build System ($(DETECTED_OS))"
	@echo ""
	@echo "Targets:"
	@echo "  make                - Build compiler"
	@echo "  make test           - Run automated test suite"
	@echo "  make test-manual-runtime - Run manual runtime test"
	@echo "  make benchmark      - Run performance benchmarks"
	@echo "  make examples       - Compile example programs"
	@echo "  make clean          - Remove build artifacts"
	@echo ""
	@echo "Platform: $(DETECTED_OS)"
