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
CFLAGS = -O2 -Icompiler -Iruntime -Istd -Istd/string -Istd/io -Istd/math -Istd/net -Istd/collections -Istd/json -Wall -Wextra -Wno-unused-parameter -Wno-unused-function
LDFLAGS = -pthread

COMPILER_SRC = compiler/aetherc.c compiler/lexer.c compiler/parser.c compiler/ast.c compiler/typechecker.c compiler/codegen.c compiler/aether_error.c compiler/aether_module.c compiler/type_inference.c
RUNTIME_SRC = runtime/multicore_scheduler.c runtime/memory.c runtime/aether_arena.c runtime/aether_pool.c runtime/aether_memory_stats.c runtime/aether_tracing.c runtime/aether_bounds_check.c runtime/aether_test.c
STD_SRC = std/string/aether_string.c std/io/aether_io.c std/math/aether_math.c std/net/aether_http.c std/net/aether_net.c std/collections/aether_collections.c std/json/aether_json.c

# Exclude test files that have main() functions
TEST_SRC = $(filter-out tests/test_runtime_manual.c, $(wildcard tests/test_*.c))

all: compiler

compiler:
	@$(MKDIR)
	$(CC) $(CFLAGS) $(COMPILER_SRC) $(STD_SRC) -o build/aetherc$(EXE_EXT)

test: compiler
	@echo "==================================="
	@echo "Building Test Suite ($(DETECTED_OS))"
	@echo "==================================="
	$(CC) $(CFLAGS) $(TEST_SRC) $(COMPILER_SRC) $(RUNTIME_SRC) $(STD_SRC) -Icompiler -Istd -o build/test_runner$(EXE_EXT)
	@echo ""
	@echo "==================================="
	@echo "Running Tests"
	@echo "==================================="
	./build/test_runner$(EXE_EXT)

test-valgrind: compiler
	@echo "==================================="
	@echo "Running Tests with Valgrind"
	@echo "==================================="
	$(CC) $(CFLAGS) -O0 -g $(TEST_SRC) $(COMPILER_SRC) $(RUNTIME_SRC) $(STD_SRC) -Icompiler -Istd -o build/test_runner$(EXE_EXT)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 ./build/test_runner$(EXE_EXT)

test-asan: compiler
	@echo "==================================="
	@echo "Running Tests with AddressSanitizer"
	@echo "==================================="
	$(CC) -fsanitize=address -fsanitize=leak -fno-omit-frame-pointer -O1 -g $(TEST_SRC) $(COMPILER_SRC) $(RUNTIME_SRC) $(STD_SRC) -Icompiler -Istd -o build/test_runner_asan$(EXE_EXT) -lpthread
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./build/test_runner_asan$(EXE_EXT)

test-memory: compiler
	@echo "==================================="
	@echo "Running Memory Tracking Tests"
	@echo "==================================="
	$(CC) $(CFLAGS) -DAETHER_MEMORY_TRACKING $(TEST_SRC) $(COMPILER_SRC) $(RUNTIME_SRC) $(STD_SRC) -Icompiler -Istd -o build/test_runner_mem$(EXE_EXT)
	./build/test_runner_mem$(EXE_EXT)

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

lsp: compiler
	@echo "==================================="
	@echo "Building Aether LSP Server ($(DETECTED_OS))"
	@echo "==================================="
	$(CC) $(CFLAGS) lsp/main.c lsp/aether_lsp.c $(COMPILER_SRC) $(RUNTIME_SRC) $(STD_SRC) $(LDFLAGS) -Icompiler -Istd -o build/aether-lsp$(EXE_EXT)
	@echo "✓ LSP Server built successfully: build/aether-lsp$(EXE_EXT)"

apkg:
	@echo "==================================="
	@echo "Building Aether Package Manager ($(DETECTED_OS))"
	@echo "==================================="
	$(CC) $(CFLAGS) tools/apkg/main.c tools/apkg/apkg.c $(LDFLAGS) -o build/apkg$(EXE_EXT)
	@echo "✓ Package Manager built successfully: build/apkg$(EXE_EXT)"

clean:
	$(RM_DIR) build

help:
	@echo "Aether Build System ($(DETECTED_OS))"
	@echo ""
	@echo "Targets:"
	@echo "  make                - Build compiler"
	@echo "  make lsp            - Build LSP server"
	@echo "  make apkg           - Build package manager"
	@echo "  make test           - Run automated test suite"
	@echo "  make test-valgrind  - Run tests with Valgrind (memory leak detection)"
	@echo "  make test-asan      - Run tests with AddressSanitizer"
	@echo "  make test-memory    - Run tests with memory tracking enabled"
	@echo "  make test-manual-runtime - Run manual runtime test"
	@echo "  make benchmark      - Run performance benchmarks"
	@echo "  make examples       - Compile example programs"
	@echo "  make clean          - Remove build artifacts"
	@echo ""
	@echo "Platform: $(DETECTED_OS)"

.PHONY: all compiler lsp apkg test test-valgrind test-asan test-memory test-manual-runtime benchmark examples clean help
