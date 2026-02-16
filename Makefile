.PHONY: all clean test compiler examples examples-run

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
    # Check if we're in MSYS2/MinGW (common on GitHub Actions Windows)
    ifneq ($(findstring MINGW,$(DETECTED_OS)),)
        EXE_EXT := .exe
    else ifneq ($(findstring MSYS,$(DETECTED_OS)),)
        EXE_EXT := .exe
    else ifneq ($(findstring CYGWIN,$(DETECTED_OS)),)
        EXE_EXT := .exe
    else
        EXE_EXT :=
    endif
    PATH_SEP := /
    MKDIR := mkdir -p
    RM := rm -f
    RM_DIR := rm -rf
endif

# Version from VERSION file (single source of truth)
VERSION := $(shell cat VERSION 2>/dev/null || echo "0.0.0")

# Compiler configuration with ccache support
CC := $(shell command -v ccache 2>/dev/null && echo "ccache gcc" || echo "gcc")
CFLAGS = -O2 -Icompiler -Iruntime -Iruntime/actors -Iruntime/scheduler -Iruntime/utils -Iruntime/memory -Iruntime/config -Istd -Istd/string -Istd/io -Istd/math -Istd/net -Istd/collections -Istd/json -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -MMD -MP -DAETHER_VERSION=\"$(VERSION)\"
LDFLAGS = -pthread -lm

# Zero warnings achieved - ready for -Werror
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj

# Windows-specific libraries (check both OS variable and uname for MSYS2)
ifeq ($(OS),Windows_NT)
    LDFLAGS += -lws2_32
else ifneq ($(findstring MINGW,$(DETECTED_OS)),)
    LDFLAGS += -lws2_32
else ifneq ($(findstring MSYS,$(DETECTED_OS)),)
    LDFLAGS += -lws2_32
else ifneq ($(findstring CYGWIN,$(DETECTED_OS)),)
    LDFLAGS += -lws2_32
endif

COMPILER_SRC = compiler/aetherc.c compiler/frontend/lexer.c compiler/frontend/parser.c compiler/ast.c compiler/analysis/typechecker.c compiler/backend/codegen.c compiler/aether_error.c compiler/aether_module.c compiler/analysis/type_inference.c compiler/backend/optimizer.c compiler/aether_diagnostics.c runtime/actors/aether_message_registry.c
COMPILER_LIB_SRC = compiler/frontend/lexer.c compiler/frontend/parser.c compiler/ast.c compiler/analysis/typechecker.c compiler/backend/codegen.c compiler/aether_error.c compiler/aether_module.c compiler/analysis/type_inference.c compiler/backend/optimizer.c compiler/aether_diagnostics.c runtime/actors/aether_message_registry.c
RUNTIME_SRC = runtime/scheduler/multicore_scheduler.c runtime/scheduler/scheduler_optimizations.c runtime/config/aether_optimization_config.c runtime/memory/memory.c runtime/memory/aether_arena.c runtime/memory/aether_pool.c runtime/memory/aether_memory_stats.c runtime/utils/aether_tracing.c runtime/utils/aether_bounds_check.c runtime/utils/aether_test.c runtime/memory/aether_arena_optimized.c runtime/aether_runtime_types.c runtime/utils/aether_cpu_detect.c runtime/memory/aether_batch.c runtime/utils/aether_simd_vectorized.c runtime/aether_runtime.c runtime/aether_numa.c runtime/actors/aether_send_buffer.c runtime/actors/aether_send_message.c runtime/actors/aether_actor_thread.c
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
TEST_SRC = tests/runtime/test_harness.c \
           tests/runtime/test_main.c \
           tests/runtime/test_64bit.c \
           tests/runtime/test_runtime_collections.c \
           tests/runtime/test_runtime_strings.c \
           tests/runtime/test_runtime_math.c \
           tests/runtime/test_runtime_json.c \
           tests/runtime/test_runtime_http.c \
           tests/runtime/test_runtime_net.c \
           tests/runtime/test_scheduler.c \
           tests/runtime/test_scheduler_stress.c \
           tests/runtime/test_zerocopy.c \
           tests/runtime/test_actor_pool.c \
           tests/runtime/test_lockfree_mailbox.c \
           tests/runtime/test_scheduler_optimizations.c \
           tests/runtime/test_spsc_queue.c \
           tests/memory/test_memory_arena.c \
           tests/memory/test_memory_pool.c \
           tests/compiler/test_lexer.c

# Standalone test programs with their own main() - build separately
# These are not part of the main test suite but can be built manually
STANDALONE_TESTS = tests/runtime/test_runtime_manual.c \
                   tests/compiler/test_arrays.c

all: compiler

# Create object directories
$(OBJ_DIR)/compiler $(OBJ_DIR)/compiler/frontend $(OBJ_DIR)/compiler/backend $(OBJ_DIR)/compiler/analysis $(OBJ_DIR)/runtime $(OBJ_DIR)/runtime/actors $(OBJ_DIR)/runtime/scheduler $(OBJ_DIR)/runtime/memory $(OBJ_DIR)/runtime/config $(OBJ_DIR)/runtime/simd $(OBJ_DIR)/runtime/utils $(OBJ_DIR)/std $(OBJ_DIR)/std/string $(OBJ_DIR)/std/io $(OBJ_DIR)/std/math $(OBJ_DIR)/std/net $(OBJ_DIR)/std/fs $(OBJ_DIR)/std/log $(OBJ_DIR)/std/collections $(OBJ_DIR)/std/json $(OBJ_DIR)/tests $(OBJ_DIR)/tests/compiler $(OBJ_DIR)/tests/memory $(OBJ_DIR)/tests/runtime:
	@mkdir -p $@

# Pattern rule for object files
$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)/compiler $(OBJ_DIR)/compiler/frontend $(OBJ_DIR)/compiler/backend $(OBJ_DIR)/compiler/analysis $(OBJ_DIR)/runtime $(OBJ_DIR)/runtime/actors $(OBJ_DIR)/runtime/scheduler $(OBJ_DIR)/runtime/memory $(OBJ_DIR)/runtime/config $(OBJ_DIR)/runtime/simd $(OBJ_DIR)/runtime/utils $(OBJ_DIR)/std $(OBJ_DIR)/std/string $(OBJ_DIR)/std/io $(OBJ_DIR)/std/math $(OBJ_DIR)/std/net $(OBJ_DIR)/std/fs $(OBJ_DIR)/std/log $(OBJ_DIR)/std/collections $(OBJ_DIR)/std/json $(OBJ_DIR)/tests $(OBJ_DIR)/tests/compiler $(OBJ_DIR)/tests/memory $(OBJ_DIR)/tests/runtime
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
ifneq ($(findstring MINGW,$(DETECTED_OS)),)
	@bash -c './build/test_runner$(EXE_EXT); exit $$?'
else ifneq ($(findstring MSYS,$(DETECTED_OS)),)
	@bash -c './build/test_runner$(EXE_EXT); exit $$?'
else
	./build/test_runner$(EXE_EXT)
endif

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
	$(CC) $(CFLAGS) -O0 -g $(TEST_SRC) $(COMPILER_SRC) $(RUNTIME_SRC) $(STD_SRC) -Icompiler -Istd -o build/test_runner$(EXE_EXT) $(LDFLAGS)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 ./build/test_runner$(EXE_EXT)

test-asan: compiler
	@echo "==================================="
	@echo "Running Tests with AddressSanitizer"
	@echo "==================================="
	$(CC) -fsanitize=address -fsanitize=leak -fno-omit-frame-pointer -O1 -g $(TEST_SRC) $(COMPILER_SRC) $(RUNTIME_SRC) $(STD_SRC) -Icompiler -Istd -o build/test_runner_asan$(EXE_EXT) -lpthread -lm
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./build/test_runner_asan$(EXE_EXT)

test-memory: compiler
	@echo "==================================="
	@echo "Running Memory Tracking Tests"
	@echo "==================================="
	$(CC) $(CFLAGS) -DAETHER_MEMORY_TRACKING $(TEST_SRC) $(COMPILER_SRC) $(RUNTIME_SRC) $(STD_SRC) -Icompiler -Istd -o build/test_runner_mem$(EXE_EXT) $(LDFLAGS)
	./build/test_runner_mem$(EXE_EXT)

test-manual-runtime: compiler
	@echo "Building manual runtime test..."
	$(CC) $(CFLAGS) tests/test_runtime_manual.c $(RUNTIME_SRC) $(LDFLAGS) -o build/test_runtime_manual$(EXE_EXT)
	@echo "Running manual runtime test..."
	./build/test_runtime_manual$(EXE_EXT)

# Benchmark presets: full (10M), medium (1M), low (100K), stress (100M)
BENCHMARK_PRESET ?= low

benchmark:
	@echo "============================================"
	@echo "  Running Cross-Language Benchmark Suite"
	@echo "  Preset: $(BENCHMARK_PRESET)"
	@echo "============================================"
	@echo ""
	@cd benchmarks/cross-language && BENCHMARK_PRESET=$(BENCHMARK_PRESET) $(MAKE) benchmark-ui

examples: compiler
	@echo "==================================="
	@echo "  Building Aether Examples"
	@echo "==================================="
	@$(MKDIR) $(BUILD_DIR)/examples $(BUILD_DIR)/examples/basics $(BUILD_DIR)/examples/actors $(BUILD_DIR)/examples/applications $(BUILD_DIR)/examples/c-interop
	@pass=0; fail=0; \
	for src in $$(find examples -name '*.ae' | sort); do \
		name=$$(echo $$src | sed 's|examples/||;s|\.ae$$||'); \
		dir=$$(dirname $$src); \
		extra_c=""; \
		if [ -d "$$dir" ]; then \
			extra_c=$$(find "$$dir" -maxdepth 1 -name '*.c' 2>/dev/null | tr '\n' ' '); \
		fi; \
		printf "  %-30s " "$$name"; \
		if ./build/aetherc$(EXE_EXT) $$src $(BUILD_DIR)/examples/$$name.c 2>/dev/null && \
		   $(CC) $(CFLAGS) $(BUILD_DIR)/examples/$$name.c $$extra_c $(RUNTIME_SRC) $(STD_SRC) $(COLLECTIONS_SRC) \
		         -o $(BUILD_DIR)/examples/$$name$(EXE_EXT) $(LDFLAGS) 2>/dev/null; then \
			echo "OK"; \
			pass=$$((pass + 1)); \
		else \
			echo "FAIL"; \
			fail=$$((fail + 1)); \
		fi; \
	done; \
	echo ""; \
	echo "  $$pass passed, $$fail failed"; \
	echo "  Binaries in $(BUILD_DIR)/examples/"

examples-run: examples
	@echo "==================================="
	@echo "  Running Aether Examples"
	@echo "==================================="
	@for bin in $$(find $(BUILD_DIR)/examples -type f -perm +111 ! -name '*.c' | sort); do \
		name=$$(echo $$bin | sed "s|$(BUILD_DIR)/examples/||"); \
		echo "--- $$name ---"; \
		timeout 5 $$bin 2>&1 || true; \
		echo ""; \
	done

lsp: compiler
	@echo "==================================="
	@echo "Building Aether LSP Server ($(DETECTED_OS))"
	@echo "==================================="
	$(CC) $(CFLAGS) lsp/main.c lsp/aether_lsp.c $(COMPILER_LIB_SRC) $(RUNTIME_SRC) $(STD_SRC) $(LDFLAGS) -Icompiler -Istd -o build/aether-lsp$(EXE_EXT)
	@echo "✓ LSP Server built successfully: build/aether-lsp$(EXE_EXT)"

apkg:
	@echo "==================================="
	@echo "Building Aether Package Manager ($(DETECTED_OS))"
	@echo "==================================="
	$(CC) $(CFLAGS) tools/apkg/main.c tools/apkg/apkg.c tools/apkg/toml_parser.c $(LDFLAGS) -o build/apkg$(EXE_EXT)
	@echo "✓ Package Manager built successfully: build/apkg$(EXE_EXT)"

ae: compiler
	@echo "==================================="
	@echo "Building ae command-line tool ($(DETECTED_OS)) v$(VERSION)"
	@echo "==================================="
	$(CC) -O2 -DAETHER_VERSION=\"$(VERSION)\" -Itools tools/ae.c tools/apkg/toml_parser.c -o build/ae$(EXE_EXT) -lm
	@echo "✓ Built successfully: build/ae$(EXE_EXT)"
	@echo ""
	@echo "Usage:"
	@echo "  ./build/ae run file.ae       Run a program"
	@echo "  ./build/ae build file.ae     Build an executable"
	@echo "  ./build/ae init myproject    Create a new project"
	@echo "  ./build/ae test              Run tests"
	@echo "  ./build/ae help              Show all commands"

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
	@ar rcs build/libaether.a $(STD_OBJS) $(COLLECTIONS_OBJS) $(RUNTIME_OBJS)
	@echo "✓ Stdlib archive created: build/libaether.a"

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
install: release ae stdlib
	@echo "==================================="
	@echo "Installing Aether to $(PREFIX)"
	@echo "==================================="
	@install -d $(PREFIX)/bin
	@install -m 755 build/ae$(EXE_EXT) $(PREFIX)/bin/ae
	@install -m 755 build/aetherc-release$(EXE_EXT) $(PREFIX)/bin/aetherc
	@install -d $(PREFIX)/lib/aether
	@install -m 644 build/libaether.a $(PREFIX)/lib/aether/
	@for dir in runtime runtime/actors runtime/scheduler runtime/utils \
	            runtime/memory runtime/config std/string std/math std/net \
	            std/collections std/json std/fs std/log std/io; do \
		if [ -d $$dir ]; then \
			install -d $(PREFIX)/include/aether/$$dir; \
			for h in $$dir/*.h; do \
				[ -f "$$h" ] && install -m 644 "$$h" $(PREFIX)/include/aether/$$dir/ 2>/dev/null || true; \
			done; \
		fi; \
	done
	@install -d $(PREFIX)/share/aether/runtime
	@install -d $(PREFIX)/share/aether/std
	@echo "✓ Installed successfully"
	@echo ""
	@echo "Run: ae version"

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
	@$(CC) -O3 experiments/concurrency/bench_computed_goto.c -o build/bench_computed_goto$(EXE_EXT) $(LDFLAGS)
	@echo "Running benchmark..."
	@./build/bench_computed_goto$(EXE_EXT)

# Benchmark manual prefetch hints
bench-prefetch:
	@echo "Building prefetch benchmark..."
	@$(CC) -O3 experiments/concurrency/bench_prefetch.c -o build/bench_prefetch$(EXE_EXT) $(LDFLAGS)
	@echo "Running benchmark..."
	@./build/bench_prefetch$(EXE_EXT)

# Profile-Guided Optimization (PGO) - 10-20% improvement
pgo-generate:
	@echo "==================================="
	@echo "PGO Step 1: Building with instrumentation..."
	@echo "==================================="
	@$(CC) -O3 -fprofile-generate experiments/concurrency/pgo_workload.c -o build/pgo_workload$(EXE_EXT) $(LDFLAGS)
	@echo "Running workload to collect profile data..."
	@./build/pgo_workload$(EXE_EXT)
	@echo "Profile data collected in *.gcda files"

pgo-build:
	@echo "==================================="
	@echo "PGO Step 2: Building with profile data..."
	@echo "==================================="
	@$(CC) -O3 -fprofile-use -D__PGO__ experiments/concurrency/bench_pgo.c -o build/bench_pgo_optimized$(EXE_EXT) $(LDFLAGS)
	@echo "PGO-optimized benchmark built"

pgo-baseline:
	@echo "Building baseline (no PGO)..."
	@$(CC) -O3 experiments/concurrency/bench_pgo.c -o build/bench_pgo_baseline$(EXE_EXT) $(LDFLAGS)
	@echo "Baseline benchmark built"

pgo-benchmark: pgo-baseline pgo-generate pgo-build
	@echo "==================================="
	@echo "PGO BENCHMARK COMPARISON"
	@echo "==================================="
	@echo ""
	@echo "Baseline (no PGO):"
	@./build/bench_pgo_baseline$(EXE_EXT)
	@echo ""
	@echo "-----------------------------------"
	@echo ""
	@echo "PGO-Optimized:"
	@./build/bench_pgo_optimized$(EXE_EXT)

pgo-clean:
	@echo "Cleaning PGO profile data..."
	@$(RM) *.gcda *.gcno 2>nul || true
	@$(RM) build/pgo_workload$(EXE_EXT) build/bench_pgo_baseline$(EXE_EXT) build/bench_pgo_optimized$(EXE_EXT) 2>nul || true
	@echo "✓ PGO data cleaned"

# Interactive REPL (requires readline library)
repl: compiler
	@echo "Starting Aether REPL..."
	@echo "Checking dependencies..."
ifeq ($(DETECTED_OS),Darwin)
	@$(CC) $(CFLAGS) -I/opt/homebrew/include tools/aether_repl.c -o build/aether_repl$(EXE_EXT) -L/opt/homebrew/lib -lreadline 2>/dev/null || \
	$(CC) $(CFLAGS) -I/usr/local/include tools/aether_repl.c -o build/aether_repl$(EXE_EXT) -L/usr/local/lib -lreadline 2>/dev/null || \
	$(CC) $(CFLAGS) tools/aether_repl.c -o build/aether_repl$(EXE_EXT) -ledit 2>/dev/null || \
	$(CC) $(CFLAGS) tools/aether_repl.c -o build/aether_repl$(EXE_EXT) -lreadline 2>/dev/null || \
	(echo "readline not found. Install with: brew install readline" && exit 1)
else ifeq ($(DETECTED_OS),Linux)
	@$(CC) $(CFLAGS) tools/aether_repl.c -o build/aether_repl$(EXE_EXT) -lreadline 2>/dev/null || \
	(echo "readline not found. Install with: sudo apt-get install libreadline-dev" && exit 1)
else
	@echo "Error: REPL not supported on $(DETECTED_OS)"
	@exit 1
endif
	@echo "✓ REPL built: build/aether_repl$(EXE_EXT)"
	@echo "Run with: ./build/ae repl"

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
	@echo "Pre-Commit Checks:"
	@echo "  make check          - Quick check (build + tests, ~30s)"
	@echo "  make check-full     - Full CI/CD check (includes memory checks, ~2min)"
	@echo ""
	@echo "CI/CD Targets:"
	@echo "  make ci             - Run full CI suite (native)"
	@echo "  make docker-ci      - Run CI in Docker (with Valgrind)"
	@echo "  make docker-build-ci- Build Docker CI image"
	@echo "  make valgrind-check - Run Valgrind memory leak detection (Linux only)"
	@echo "  ./scripts/run-ci-local.sh - Full CI with Docker (recommended)"
	@echo ""
	@echo "Tool Targets:"
	@echo "  make lsp            - Build LSP server"
	@echo "  make apkg           - Build package manager"
	@echo "  make profiler       - Build profiler dashboard"
	@echo ""
	@echo "Other Targets:"
	@echo "  make benchmark      - Run performance benchmarks"
	@echo "  make examples       - Compile example programs"
	@echo "  make install        - Install to $(PREFIX)"
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

# Docker CI/CD targets
docker-build-ci:
	@echo "Building Docker CI image..."
	docker build -f docker/Dockerfile.ci -t aether-ci:latest .

docker-ci: docker-build-ci
	@echo "Running CI tests in Docker..."
	docker run --rm -v $(PWD):/aether -w /aether aether-ci make ci

ci: clean
	@echo "==================================="
	@echo "Running Full CI Suite"
	@echo "==================================="
	@echo "Building compiler..."
	@$(MAKE) compiler CFLAGS="-O2 -Wall -Wextra -Werror"
	@echo ""
	@echo "Building tests..."
	@$(MAKE) test-build CFLAGS="-O0 -g"
	@echo ""
	@echo "Running tests..."
	@./build/test_runner$(EXE_EXT)
	@echo ""
	@echo "Building Docker CI image..."
	@$(MAKE) docker-build-ci
	@echo ""
	@echo "Running Valgrind in Docker..."
	@docker run --rm -v $(PWD):/aether -w /aether aether-ci bash -c "\
		make clean && \
		make compiler CFLAGS='-O0 -g' && \
		make test-build CFLAGS='-O0 -g' && \
		valgrind --leak-check=full \
			--show-leak-kinds=all \
			--track-origins=yes \
			--error-exitcode=1 \
			--suppressions=.valgrind-suppressions \
			./build/test_runner || (echo 'Memory leaks detected!' && exit 1)"
	@echo ""
	@echo "✓ CI passed with no memory leaks"

valgrind-check: clean
	@echo "==================================="
	@echo "Running Valgrind Memory Check"
	@echo "==================================="
	@$(MAKE) compiler CFLAGS="-O0 -g"
	@$(MAKE) test-build CFLAGS="-O0 -g"
	@valgrind --leak-check=full \
		--show-leak-kinds=all \
		--track-origins=yes \
		--error-exitcode=1 \
		--suppressions=.valgrind-suppressions \
		./build/test_runner$(EXE_EXT) || (echo "Memory leaks detected!" && exit 1)
	@echo "✓ No memory leaks detected"

.PHONY: all compiler lsp apkg ae profiler test test-build test-valgrind test-asan test-memory test-manual-runtime benchmark benchmark-ui examples run compile repl clean help self-test release install stats stdlib ci docker-ci docker-build-ci valgrind-check

# Cross-language benchmark UI
benchmark-ui:
	@cd benchmarks/cross-language && $(MAKE) benchmark-ui
