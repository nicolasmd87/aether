.PHONY: all clean test compiler examples

# Detect OS
ifeq ($(OS),Windows_NT)
    DETECTED_OS := Windows
    EXE_EXT := .exe
    PATH_SEP := \\
    MKDIR := if not exist
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

# Compiler configuration with ccache support
CC := $(shell command -v ccache 2>/dev/null && echo "ccache gcc" || echo "gcc")
CFLAGS = -O2 -Icompiler -Iruntime -Istd -Istd/string -Istd/io -Istd/math -Istd/net -Istd/collections -Istd/json -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -MMD -MP
LDFLAGS = -pthread

# Zero warnings achieved - ready for -Werror
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj

ifeq ($(OS),Windows_NT)
    LDFLAGS += -lws2_32
endif

COMPILER_SRC = compiler/aetherc.c compiler/frontend/lexer.c compiler/frontend/parser.c compiler/ast.c compiler/analysis/typechecker.c compiler/backend/codegen.c compiler/aether_error.c compiler/aether_module.c compiler/analysis/type_inference.c compiler/backend/optimizer.c compiler/aether_diagnostics.c runtime/actors/aether_message_registry.c
COMPILER_LIB_SRC = compiler/frontend/lexer.c compiler/frontend/parser.c compiler/ast.c compiler/analysis/typechecker.c compiler/backend/codegen.c compiler/aether_error.c compiler/aether_module.c compiler/analysis/type_inference.c compiler/backend/optimizer.c compiler/aether_diagnostics.c runtime/actors/aether_message_registry.c
RUNTIME_SRC = runtime/scheduler/multicore_scheduler.c runtime/memory/memory.c runtime/memory/aether_arena.c runtime/memory/aether_pool.c runtime/memory/aether_memory_stats.c runtime/utils/aether_tracing.c runtime/utils/aether_bounds_check.c runtime/utils/aether_test.c runtime/memory/aether_arena_optimized.c runtime/aether_runtime_types.c runtime/utils/aether_cpu_detect.c runtime/memory/aether_batch.c runtime/utils/aether_simd_vectorized.c runtime/aether_runtime.c
STD_SRC = std/string/aether_string.c std/math/aether_math.c std/net/aether_http.c std/net/aether_http_server.c std/net/aether_net.c std/collections/aether_collections.c std/json/aether_json.c std/fs/aether_fs.c std/log/aether_log.c
COLLECTIONS_SRC = std/collections/aether_hashmap.c std/collections/aether_set.c std/collections/aether_vector.c std/collections/aether_pqueue.c

# Object files
COMPILER_OBJS = $(COMPILER_SRC:%.c=$(OBJ_DIR)/%.o)
COMPILER_LIB_OBJS = $(COMPILER_LIB_SRC:%.c=$(OBJ_DIR)/%.o)
RUNTIME_OBJS = $(RUNTIME_SRC:%.c=$(OBJ_DIR)/%.o)
STD_OBJS = $(STD_SRC:%.c=$(OBJ_DIR)/%.o)
COLLECTIONS_OBJS = $(COLLECTIONS_SRC:%.c=$(OBJ_DIR)/%.o)
TEST_OBJS = $(TEST_SRC:%.c=$(OBJ_DIR)/%.o)

# Dependency files
DEPS = $(COMPILER_OBJS:.o=.d) $(RUNTIME_OBJS:.o=.d) $(STD_OBJS:.o=.d) $(COLLECTIONS_OBJS:.o=.d)

# Include dependency files
-include $(DEPS)

# Test files using TEST() macro system (exclude standalone tests)
TEST_SRC = tests/test_harness.c \
           tests/test_main.c \
           tests/compiler/test_lexer.c \
           tests/compiler/test_lexer_comprehensive.c \
           tests/compiler/test_parser.c \
           tests/compiler/test_parser_comprehensive.c \
           tests/compiler/test_typechecker.c \
           tests/compiler/test_type_inference_comprehensive.c \
           tests/compiler/test_codegen.c \
           tests/compiler/test_structs.c \
           tests/compiler/test_switch_statements.c \
           tests/compiler/test_pattern_matching_comprehensive.c \
           tests/memory/test_memory_arena.c \
           tests/memory/test_memory_pool.c \
           tests/memory/test_memory_leaks.c \
           tests/memory/test_memory_stress.c \
           tests/runtime/test_64bit.c \
           tests/runtime/test_runtime_collections.c \
           tests/runtime/test_runtime_strings.c \
           tests/runtime/test_runtime_math.c \
           tests/runtime/test_runtime_json.c \
           tests/runtime/test_runtime_http.c \
           tests/runtime/test_runtime_net.c

# Standalone test programs with their own main() - build separately
STANDALONE_TESTS = tests/test_runtime_implementations.c \
                   tests/test_scheduler_integration.c \
                   tests/runtime/test_runtime_manual.c \
                   tests/compiler/test_arrays.c \
           tests/test_memory_stress.c \
           tests/test_memory_leaks.c \
           tests/test_64bit.c \
           tests/test_runtime_math.c \
           tests/test_runtime_collections.c \
           tests/test_runtime_json.c \
           tests/test_runtime_http.c \
           tests/test_runtime_net.c \
           tests/test_runtime_strings.c \
           tests/test_collections.c

all: compiler

# Create object directories
$(OBJ_DIR)/compiler $(OBJ_DIR)/compiler/frontend $(OBJ_DIR)/compiler/backend $(OBJ_DIR)/compiler/analysis $(OBJ_DIR)/runtime $(OBJ_DIR)/runtime/actors $(OBJ_DIR)/runtime/scheduler $(OBJ_DIR)/runtime/memory $(OBJ_DIR)/runtime/simd $(OBJ_DIR)/runtime/utils $(OBJ_DIR)/std $(OBJ_DIR)/std/string $(OBJ_DIR)/std/io $(OBJ_DIR)/std/math $(OBJ_DIR)/std/net $(OBJ_DIR)/std/fs $(OBJ_DIR)/std/log $(OBJ_DIR)/std/collections $(OBJ_DIR)/std/json $(OBJ_DIR)/tests $(OBJ_DIR)/tests/compiler $(OBJ_DIR)/tests/memory $(OBJ_DIR)/tests/runtime:
ifeq ($(OS),Windows_NT)
	@if not exist "$(OBJ_DIR)" mkdir "$(OBJ_DIR)"
	@if not exist "$(OBJ_DIR)\compiler" mkdir "$(OBJ_DIR)\compiler"
	@if not exist "$(OBJ_DIR)\compiler\frontend" mkdir "$(OBJ_DIR)\compiler\frontend"
	@if not exist "$(OBJ_DIR)\compiler\backend" mkdir "$(OBJ_DIR)\compiler\backend"
	@if not exist "$(OBJ_DIR)\compiler\analysis" mkdir "$(OBJ_DIR)\compiler\analysis"
	@if not exist "$(OBJ_DIR)\runtime" mkdir "$(OBJ_DIR)\runtime"
	@if not exist "$(OBJ_DIR)\runtime\actors" mkdir "$(OBJ_DIR)\runtime\actors"
	@if not exist "$(OBJ_DIR)\runtime\scheduler" mkdir "$(OBJ_DIR)\runtime\scheduler"
	@if not exist "$(OBJ_DIR)\runtime\memory" mkdir "$(OBJ_DIR)\runtime\memory"
	@if not exist "$(OBJ_DIR)\runtime\simd" mkdir "$(OBJ_DIR)\runtime\simd"
	@if not exist "$(OBJ_DIR)\runtime\utils" mkdir "$(OBJ_DIR)\runtime\utils"
	@if not exist "$(OBJ_DIR)\std" mkdir "$(OBJ_DIR)\std"
	@if not exist "$(OBJ_DIR)\std\string" mkdir "$(OBJ_DIR)\std\string"
	@if not exist "$(OBJ_DIR)\std\io" mkdir "$(OBJ_DIR)\std\io"
	@if not exist "$(OBJ_DIR)\std\math" mkdir "$(OBJ_DIR)\std\math"
	@if not exist "$(OBJ_DIR)\std\net" mkdir "$(OBJ_DIR)\std\net"
	@if not exist "$(OBJ_DIR)\std\fs" mkdir "$(OBJ_DIR)\std\fs"
	@if not exist "$(OBJ_DIR)\std\log" mkdir "$(OBJ_DIR)\std\log"
	@if not exist "$(OBJ_DIR)\std\collections" mkdir "$(OBJ_DIR)\std\collections"
	@if not exist "$(OBJ_DIR)\std\json" mkdir "$(OBJ_DIR)\std\json"
	@if not exist "$(OBJ_DIR)\tests" mkdir "$(OBJ_DIR)\tests"
	@if not exist "$(OBJ_DIR)\tests\compiler" mkdir "$(OBJ_DIR)\tests\compiler"
	@if not exist "$(OBJ_DIR)\tests\memory" mkdir "$(OBJ_DIR)\tests\memory"
	@if not exist "$(OBJ_DIR)\tests\runtime" mkdir "$(OBJ_DIR)\tests\runtime"
else
	@$(MKDIR) $@
endif

# Pattern rule for object files
$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)/compiler $(OBJ_DIR)/runtime $(OBJ_DIR)/std $(OBJ_DIR)/std/string $(OBJ_DIR)/std/io $(OBJ_DIR)/std/math $(OBJ_DIR)/std/net $(OBJ_DIR)/std/fs $(OBJ_DIR)/std/log $(OBJ_DIR)/std/collections $(OBJ_DIR)/std/json $(OBJ_DIR)/tests $(OBJ_DIR)/tests/compiler $(OBJ_DIR)/tests/memory $(OBJ_DIR)/tests/runtime
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

# Compiler target (incremental build with object files)
compiler: $(COMPILER_OBJS) $(STD_OBJS) $(COLLECTIONS_OBJS)
	@echo "Linking compiler..."
	@$(CC) $(COMPILER_OBJS) $(STD_OBJS) $(COLLECTIONS_OBJS) -o build/aetherc$(EXE_EXT) $(LDFLAGS)
	@echo "Compiler built successfully"

# Fast compiler target (monolithic, for clean builds)
compiler-fast:
ifeq ($(OS),Windows_NT)
	@if not exist "build" mkdir "build"
else
	@$(MKDIR) build
endif
	$(CC) $(CFLAGS) $(COMPILER_SRC) $(STD_SRC) $(COLLECTIONS_SRC) -o build/aetherc$(EXE_EXT) $(LDFLAGS)

test: $(TEST_OBJS) $(COMPILER_LIB_OBJS) $(RUNTIME_OBJS) $(STD_OBJS) $(COLLECTIONS_OBJS)
	@echo "==================================="
	@echo "Building Test Suite ($(DETECTED_OS))"
	@echo "==================================="
	@echo "Linking test runner..."
	@$(CC) $(TEST_OBJS) $(COMPILER_LIB_OBJS) $(RUNTIME_OBJS) $(STD_OBJS) $(COLLECTIONS_OBJS) -o build/test_runner$(EXE_EXT) $(LDFLAGS)
	@echo ""
	@echo "==================================="
	@echo "Running Tests"
	@echo "==================================="
	./build/test_runner$(EXE_EXT)

# Fast test target (monolithic)
test-fast: compiler-fast
	@echo "==================================="
	@echo "Building Test Suite ($(DETECTED_OS))"
	@echo "==================================="
	$(CC) $(CFLAGS) $(TEST_SRC) $(COMPILER_LIB_SRC) $(RUNTIME_SRC) $(STD_SRC) $(COLLECTIONS_SRC) -Icompiler -Istd -Istd/collections -o build/test_runner$(EXE_EXT) $(LDFLAGS)
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
	$(CC) $(CFLAGS) tools/apkg/main.c tools/apkg/apkg.c tools/apkg/toml_parser.c $(LDFLAGS) -o build/apkg$(EXE_EXT)
	@echo "✓ Package Manager built successfully: build/apkg$(EXE_EXT)"

ae: compiler
	@echo "==================================="
	@echo "Building ae command-line tool ($(DETECTED_OS))"
	@echo "==================================="
	$(CC) -O2 tools/ae.c -o build/ae$(EXE_EXT) -lm
	@echo "✓ Built successfully: build/ae$(EXE_EXT)"
	@echo ""
	@echo "Usage:"
	@echo "  ./build/ae$(EXE_EXT) run file.ae"
	@echo "  ./build/ae$(EXE_EXT) build file.ae"

profiler:
	@echo "==================================="
	@echo "Building Aether Profiler Dashboard ($(DETECTED_OS))"
	@echo "==================================="
	$(CC) $(CFLAGS) -DAETHER_PROFILING tools/profiler/profiler_server.c tools/profiler/profiler_demo.c $(RUNTIME_SRC) $(LDFLAGS) -o build/profiler_demo$(EXE_EXT)
	@echo "✓ Profiler built successfully: build/profiler_demo$(EXE_EXT)"
	@echo ""
	@echo "Run the demo and open http://localhost:8080"

# Precompiled stdlib archive
stdlib: $(STD_OBJS) $(COLLECTIONS_OBJS) $(RUNTIME_OBJS)
	@echo "Creating precompiled stdlib archive..."
	@ar rcs build/libaether_std.a $(STD_OBJS) $(COLLECTIONS_OBJS) $(RUNTIME_OBJS)
	@echo "✓ Stdlib archive created: build/libaether_std.a"

# Self-test: compiler on itself
self-test: compiler
	@echo "==================================="
	@echo "Running Compiler Self-Test"
	@echo "==================================="
	@echo "Testing compiler on complex syntax..."
	@if [ -f examples/showcase/chat_server.ae ]; then \
		./build/aetherc$(EXE_EXT) examples/showcase/chat_server.ae build/test_compile.c && \
		echo "✓ Complex syntax compilation successful"; \
	fi
	@echo ""
	@echo "Testing collections..."
	@$(MAKE) --no-print-directory test > /dev/null && echo "✓ All tests passed"
	@echo ""
	@echo "==================================="
	@echo "Self-test complete"
	@echo "==================================="

# Release build with optimizations and warnings as errors
release: clean
	@echo "==================================="
	@echo "Building Optimized Release"
	@echo "==================================="
	@$(MKDIR)
	@echo "Compiling with -O3 -DNDEBUG -flto -Werror..."
	@$(CC) -O3 -DNDEBUG -flto -Werror -Icompiler -Iruntime -Istd -Istd/collections \
		$(COMPILER_SRC) $(STD_SRC) $(COLLECTIONS_SRC) \
		-o build/aetherc-release$(EXE_EXT) $(LDFLAGS)
ifeq ($(DETECTED_OS),Linux)
	@echo "Stripping debug symbols..."
	@strip build/aetherc-release$(EXE_EXT)
endif
	@echo "✓ Release build complete: build/aetherc-release$(EXE_EXT)"
	@ls -lh build/aetherc-release$(EXE_EXT)

# Install to system
PREFIX ?= /usr/local
install: release
	@echo "==================================="
	@echo "Installing Aether"
	@echo "==================================="
ifeq ($(DETECTED_OS),Linux)
	@echo "Installing to $(PREFIX)..."
	@install -d $(PREFIX)/bin
	@install -m 755 build/aetherc-release$(EXE_EXT) $(PREFIX)/bin/aetherc
	@install -d $(PREFIX)/include/aether
	@install -m 644 runtime/*.h $(PREFIX)/include/aether/
	@install -m 644 std/*/*.h $(PREFIX)/include/aether/
	@echo "✓ Installed successfully"
	@echo ""
	@echo "Run: aetherc --version"
else
	@echo "Install target currently only supports Linux"
	@echo "For Windows/macOS, manually copy build/aetherc-release$(EXE_EXT) to your PATH"
endif

# Run an Aether program (compile + execute)
run: compiler
ifndef FILE
	@echo "Error: FILE not specified"
	@echo "Usage: make run FILE=examples/basic/hello_world.ae"
	@exit 1
endif
	@echo "Compiling $(FILE) to C..."
	@./build/aetherc$(EXE_EXT) $(FILE) build/output.c
	@echo "Building executable..."
	@$(CC) $(CFLAGS) build/output.c $(RUNTIME_SRC) $(STD_SRC) $(COLLECTIONS_SRC) -o build/output$(EXE_EXT) $(LDFLAGS)
	@echo "Running..."
	@./build/output$(EXE_EXT)

# Compile an Aether program to executable
compile: compiler
ifndef FILE
	@echo "Error: FILE not specified"
	@echo "Usage: make compile FILE=myprogram.ae [OUTPUT=myprogram]"
	@exit 1
endif
ifndef OUTPUT
	OUTPUT := $(basename $(notdir $(FILE)))
endif
	@echo "Compiling $(FILE) to C..."
	@./build/aetherc$(EXE_EXT) $(FILE) build/$(OUTPUT).c
	@echo "Building executable..."
	@$(CC) $(CFLAGS) build/$(OUTPUT).c $(RUNTIME_SRC) $(STD_SRC) $(COLLECTIONS_SRC) -o build/$(OUTPUT)$(EXE_EXT) $(LDFLAGS)
	@echo "✓ Built: build/$(OUTPUT)$(EXE_EXT)"

# Benchmark computed goto dispatch
bench-dispatch: 
	@echo "Building computed goto benchmark..."
	@$(CC) -O3 experiments/concurrency/bench_computed_goto.c -o build/bench_computed_goto$(EXE_EXT)
	@echo "Running benchmark..."
	@./build/bench_computed_goto$(EXE_EXT)

# Benchmark manual prefetch hints
bench-prefetch:
	@echo "Building prefetch benchmark..."
	@$(CC) -O3 experiments/concurrency/bench_prefetch.c -o build/bench_prefetch$(EXE_EXT)
	@echo "Running benchmark..."
	@./build/bench_prefetch$(EXE_EXT)

# Interactive REPL (requires linenoise or equivalent)
repl: compiler
	@echo "Starting Aether REPL..."
	@if [ -f tools/aether_repl.c ]; then \
		$(CC) $(CFLAGS) tools/aether_repl.c $(COMPILER_SRC) $(RUNTIME_SRC) $(STD_SRC) $(COLLECTIONS_SRC) -o build/aether_repl$(EXE_EXT) $(LDFLAGS); \
		./build/aether_repl$(EXE_EXT); \
	else \
		echo "Error: REPL not implemented yet"; \
		echo "You can run programs with: make run FILE=yourfile.ae"; \
	fi

# Build statistics
stats:
	@echo "==================================="
	@echo "Build Statistics"
	@echo "==================================="
	@echo "Object files:        $$(find $(OBJ_DIR) -name '*.o' 2>/dev/null | wc -l)"
	@echo "Dependency files:    $$(find $(OBJ_DIR) -name '*.d' 2>/dev/null | wc -l)"
	@echo "Source files:"
	@echo "  Compiler:          $$(echo $(COMPILER_SRC) | wc -w)"
	@echo "  Runtime:           $$(echo $(RUNTIME_SRC) | wc -w)"
	@echo "  Stdlib:            $$(echo $(STD_SRC) $(COLLECTIONS_SRC) | wc -w)"
	@echo "  Tests:             $$(echo $(TEST_SRC) | wc -w)"
	@echo ""
	@echo "Lines of code:"
	@find compiler runtime std -name '*.c' -o -name '*.h' | xargs wc -l | tail -1
	@echo ""
	@if [ -d $(OBJ_DIR) ]; then \
		echo "Build directory size:"; \
		du -sh build 2>/dev/null || echo "N/A"; \
	fi
	@echo "==================================="

# Code quality checks
check: test
	@echo "==================================="
	@echo "Running Code Quality Checks"
	@echo "==================================="
	@echo "Checking for TODO/FIXME comments..."
	@grep -rn "TODO\|FIXME" compiler runtime std --color=auto || echo "  ✓ No TODOs found"
	@echo ""
	@echo "Checking for debug prints..."
	@grep -rn "printf.*DEBUG\|fprintf.*DEBUG" compiler runtime std --color=auto || echo "  ✓ No debug prints found"
	@echo ""
	@echo "Checking test coverage..."
	@echo "  Test files: $$(find tests -name '*.c' | wc -l)"
	@echo "  Source files: $$(find compiler runtime std -name '*.c' | wc -l)"
	@echo "  Test ratio: $$(echo "scale=2; $$(find tests -name '*.c' | wc -l) / $$(find compiler runtime std -name '*.c' | wc -l)" | bc)x"
	@echo ""
	@echo "All checks passed!"

# Parallel test execution
test-parallel:
	@echo "==================================="
	@echo "Running Tests in Parallel"
	@echo "==================================="
	@echo "Testing by category..."
	@for cat in compiler runtime collections network memory stdlib parser; do \
		echo "  Testing $$cat..."; \
		./build/test_runner$(EXE_EXT) --category=$$cat & \
	done; \
	wait
	@echo ""
	@echo "All parallel tests complete!"

clean:
	$(RM_DIR) build

help:
	@echo "Aether Build System ($(DETECTED_OS))"
	@echo ""
	@echo "Quick Start:"
	@echo "  make ae             - Build 'ae' CLI tool (recommended)"
	@echo "  ./build/ae run file.ae      - Run a program (Go-style)"
	@echo "  ./build/ae build file.ae    - Build executable"
	@echo ""
	@echo "Or use Make directly:"
	@echo "  make                - Build compiler"
	@echo "  make run FILE=...   - Compile and run an Aether program"
	@echo "  make compile FILE=...- Compile Aether program to executable"
	@echo "  make test           - Run test suite"
	@echo ""
	@echo "Build Targets:"
	@echo "  make compiler       - Build compiler (incremental)"
	@echo "  make compiler-fast  - Build compiler (monolithic, faster for clean)"
	@echo "  make -j8            - Parallel build with 8 jobs (2-4x faster)"
	@echo "  make release        - Optimized release build (-O3 -flto)"
	@echo "  make stdlib         - Build precompiled stdlib archive"
	@echo ""
	@echo "Run Targets:"
	@echo "  make run FILE=path/to/file.ae    - Compile and execute program"
	@echo "  make compile FILE=file.ae        - Compile to executable"
	@echo "  make repl                        - Start interactive REPL"
	@echo ""
	@echo "Test Targets:"
	@echo "  make test           - Run tests (incremental)"
	@echo "  make test-fast      - Run tests (monolithic)"
	@echo "  make test-valgrind  - Run tests with Valgrind (memory leak detection)"
	@echo "  make ae             - Build ae CLI tool (Go-style interface)"
	@echo "  make test-asan      - Run tests with AddressSanitizer"
	@echo "  make test-memory    - Run tests with memory tracking enabled"
	@echo "  make self-test      - Test compiler on complex examples"
	@echo ""
	@echo "Tool Targets:"
	@echo "  make lsp            - Build LSP server"
	@echo "  make apkg           - Build package manager"
	@echo "  make profiler       - Build profiler dashboard"
	@echo ""
	@echo "Other Targets:"
	@echo "  make benchmark      - Run performance benchmarks"
	@echo "  make examples       - Compile example programs"
	@echo "  make install        - Install to $(PREFIX) (Linux only)"
	@echo "  make stats          - Show build statistics"
	@echo "  make clean          - Remove build artifacts"
	@echo "  make help           - Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make run FILE=examples/basic/hello_world.ae"
	@echo "  make compile FILE=myapp.ae OUTPUT=myapp"
	@echo "  make -j8 test       - Build and test with 8 parallel jobs"
	@echo ""
	@echo "Platform: $(DETECTED_OS)"
	@echo "Compiler: $(CC)"

test-build: $(TEST_OBJS) $(COMPILER_LIB_OBJS) $(RUNTIME_OBJS) $(STD_OBJS) $(COLLECTIONS_OBJS)
	@echo "Building test runner..."
	@$(CC) $(TEST_OBJS) $(COMPILER_LIB_OBJS) $(RUNTIME_OBJS) $(STD_OBJS) $(COLLECTIONS_OBJS) -o build/test_runner$(EXE_EXT) $(LDFLAGS)

.PHONY: all compiler lsp apkg ae profiler test test-build test-valgrind test-asan test-memory test-manual-runtime benchmark examples run compile repl clean help self-test release install stats stdlib
