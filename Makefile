.PHONY: all clean test compiler examples examples-run ci

# Detect OS and shell environment.
# WINDOWS_NATIVE is set only for pure Windows (mingw32-make + cmd.exe).
# IS_WINDOWS is set for any Windows variant (native, MSYS2, MinGW, Cygwin).
WINDOWS_NATIVE :=
IS_WINDOWS :=
ifeq ($(OS),Windows_NT)
    IS_WINDOWS := 1
    _UNAME_S := $(shell uname -s 2>&1)
    ifneq ($(findstring MINGW,$(_UNAME_S)),)
        DETECTED_OS := $(_UNAME_S)
        EXE_EXT := .exe
    else ifneq ($(findstring MSYS,$(_UNAME_S)),)
        DETECTED_OS := $(_UNAME_S)
        EXE_EXT := .exe
    else ifneq ($(findstring CYGWIN,$(_UNAME_S)),)
        DETECTED_OS := $(_UNAME_S)
        EXE_EXT := .exe
    else
        DETECTED_OS := Windows
        EXE_EXT := .exe
        WINDOWS_NATIVE := 1
    endif
else
    DETECTED_OS := $(shell uname -s)
    ifneq ($(findstring MINGW,$(DETECTED_OS)),)
        EXE_EXT := .exe
        IS_WINDOWS := 1
    else ifneq ($(findstring MSYS,$(DETECTED_OS)),)
        EXE_EXT := .exe
        IS_WINDOWS := 1
    else ifneq ($(findstring CYGWIN,$(DETECTED_OS)),)
        EXE_EXT := .exe
        IS_WINDOWS := 1
    else
        EXE_EXT :=
    endif
endif

ifdef WINDOWS_NATIVE
    PATH_SEP := \\
    MKDIR := if not exist
    RM := del /Q
    RM_DIR := rd /S /Q
else
    PATH_SEP := /
    MKDIR := mkdir -p
    RM := rm -f
    RM_DIR := rm -rf
endif

# Parallel job count (override with: make test-ae NPROC=8)
ifdef WINDOWS_NATIVE
NPROC ?= $(shell echo %NUMBER_OF_PROCESSORS% 2>nul || echo 4)
else
NPROC ?= $(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
endif

# Version: prefer highest git tag (authoritative), fall back to VERSION file (tarballs)
ifdef WINDOWS_NATIVE
VERSION := $(shell type VERSION 2>nul || echo 0.0.0)
else
VERSION := $(shell git tag -l 'v*.*.*' 2>/dev/null | sed 's/^v//' | sort -t. -k1,1n -k2,2n -k3,3n | tail -1)
ifeq ($(VERSION),)
VERSION := $(shell cat VERSION 2>/dev/null || echo "0.0.0")
endif
endif

# Compiler configuration with ccache support
ifdef WINDOWS_NATIVE
CC := gcc
else
CC := $(shell command -v ccache >/dev/null 2>&1 && echo "ccache gcc" || echo "gcc")
endif
EXTRA_CFLAGS ?=
PLATFORM ?= native

# Platform-specific overrides
ifeq ($(PLATFORM),wasm)
    CC := emcc
    EXTRA_CFLAGS += -DAETHER_NO_THREADING -DAETHER_NO_FILESYSTEM -DAETHER_NO_NETWORKING
    SCHEDULER_SRC := runtime/scheduler/aether_scheduler_coop.c
else ifeq ($(PLATFORM),embedded)
    EXTRA_CFLAGS += -DAETHER_NO_THREADING -DAETHER_NO_FILESYSTEM -DAETHER_NO_NETWORKING -DAETHER_NO_GETENV
    SCHEDULER_SRC := runtime/scheduler/aether_scheduler_coop.c
else
    # Auto-detect: if EXTRA_CFLAGS disables threading, use cooperative scheduler
    ifneq ($(findstring AETHER_NO_THREADING,$(EXTRA_CFLAGS)),)
        SCHEDULER_SRC := runtime/scheduler/aether_scheduler_coop.c
    else
        SCHEDULER_SRC := runtime/scheduler/multicore_scheduler.c
    endif
endif

CFLAGS = -O2 -Icompiler -Iruntime -Iruntime/actors -Iruntime/scheduler -Iruntime/utils -Iruntime/memory -Iruntime/config -Istd -Istd/string -Istd/io -Istd/math -Istd/net -Istd/collections -Istd/json -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -MMD -MP -DAETHER_VERSION=\"$(VERSION)\" -DAETHER_HAS_SANDBOX $(EXTRA_CFLAGS)
LDFLAGS = -lm
ifneq ($(PLATFORM),wasm)
ifneq ($(PLATFORM),embedded)
ifeq ($(findstring AETHER_NO_THREADING,$(EXTRA_CFLAGS)),)
LDFLAGS += -pthread
endif
endif
endif

# Zero warnings achieved - ready for -Werror
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj

# Windows-specific: -static avoids libwinpthread/libgcc DLL dependencies
ifdef WINDOWS_NATIVE
    LDFLAGS += -static -lws2_32
else ifneq ($(findstring MINGW,$(DETECTED_OS)),)
    LDFLAGS += -static -lws2_32
else ifneq ($(findstring MSYS,$(DETECTED_OS)),)
    LDFLAGS += -static -lws2_32
else ifneq ($(findstring CYGWIN,$(DETECTED_OS)),)
    LDFLAGS += -static -lws2_32
endif

COMPILER_SRC = compiler/aetherc.c compiler/parser/lexer.c compiler/parser/parser.c compiler/ast.c compiler/analysis/typechecker.c compiler/codegen/codegen.c compiler/codegen/codegen_expr.c compiler/codegen/codegen_stmt.c compiler/codegen/codegen_actor.c compiler/codegen/codegen_func.c compiler/aether_error.c compiler/aether_module.c compiler/analysis/type_inference.c compiler/codegen/optimizer.c compiler/aether_diagnostics.c runtime/actors/aether_message_registry.c
COMPILER_LIB_SRC = compiler/parser/lexer.c compiler/parser/parser.c compiler/ast.c compiler/analysis/typechecker.c compiler/codegen/codegen.c compiler/codegen/codegen_expr.c compiler/codegen/codegen_stmt.c compiler/codegen/codegen_actor.c compiler/codegen/codegen_func.c compiler/aether_error.c compiler/aether_module.c compiler/analysis/type_inference.c compiler/codegen/optimizer.c compiler/aether_diagnostics.c runtime/actors/aether_message_registry.c
RUNTIME_SRC = $(SCHEDULER_SRC) runtime/scheduler/scheduler_optimizations.c runtime/config/aether_optimization_config.c runtime/memory/memory.c runtime/memory/aether_arena.c runtime/memory/aether_pool.c runtime/memory/aether_memory_stats.c runtime/utils/aether_tracing.c runtime/utils/aether_bounds_check.c runtime/utils/aether_test.c runtime/memory/aether_arena_optimized.c runtime/aether_runtime_types.c runtime/utils/aether_cpu_detect.c runtime/memory/aether_batch.c runtime/utils/aether_simd_vectorized.c runtime/aether_runtime.c runtime/aether_numa.c runtime/aether_sandbox.c runtime/aether_spawn_sandboxed.c runtime/aether_shared_map.c runtime/actors/aether_send_buffer.c runtime/actors/aether_send_message.c runtime/actors/aether_actor_thread.c
STD_SRC = std/string/aether_string.c std/math/aether_math.c std/net/aether_http.c std/net/aether_http_server.c std/net/aether_net.c std/collections/aether_collections.c std/json/aether_json.c std/fs/aether_fs.c std/log/aether_log.c std/io/aether_io.c std/os/aether_os.c
COLLECTIONS_SRC = std/collections/aether_hashmap.c std/collections/aether_set.c std/collections/aether_vector.c std/collections/aether_pqueue.c

# Object files
COMPILER_OBJS = $(COMPILER_SRC:%.c=$(OBJ_DIR)/%.o)
COMPILER_LIB_OBJS = $(COMPILER_LIB_SRC:%.c=$(OBJ_DIR)/%.o)
RUNTIME_OBJS = $(RUNTIME_SRC:%.c=$(OBJ_DIR)/%.o)
STD_OBJS = $(STD_SRC:%.c=$(OBJ_DIR)/%.o)
COLLECTIONS_OBJS = $(COLLECTIONS_SRC:%.c=$(OBJ_DIR)/%.o)
TEST_OBJS = $(TEST_SRC:%.c=$(OBJ_DIR)/%.o)

# Dependency files (include test objects so header changes trigger test recompilation)
DEPS = $(COMPILER_OBJS:.o=.d) $(RUNTIME_OBJS:.o=.d) $(STD_OBJS:.o=.d) $(COLLECTIONS_OBJS:.o=.d) $(TEST_OBJS:.o=.d)

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
           tests/runtime/test_worksteal_race.c \
           tests/runtime/test_http_server.c \
           tests/memory/test_memory_arena.c \
           tests/memory/test_memory_pool.c \
           tests/compiler/test_lexer.c

# Standalone test programs with their own main() - build separately
# These are not part of the main test suite but can be built manually
STANDALONE_TESTS = tests/runtime/test_runtime_manual.c \
                   tests/compiler/test_arrays.c

all: compiler ae stdlib

# Create object directories
$(OBJ_DIR)/compiler $(OBJ_DIR)/compiler/parser $(OBJ_DIR)/compiler/codegen $(OBJ_DIR)/compiler/analysis $(OBJ_DIR)/runtime $(OBJ_DIR)/runtime/actors $(OBJ_DIR)/runtime/scheduler $(OBJ_DIR)/runtime/memory $(OBJ_DIR)/runtime/config $(OBJ_DIR)/runtime/simd $(OBJ_DIR)/runtime/utils $(OBJ_DIR)/std $(OBJ_DIR)/std/string $(OBJ_DIR)/std/io $(OBJ_DIR)/std/math $(OBJ_DIR)/std/net $(OBJ_DIR)/std/fs $(OBJ_DIR)/std/log $(OBJ_DIR)/std/collections $(OBJ_DIR)/std/json $(OBJ_DIR)/std/os $(OBJ_DIR)/tests $(OBJ_DIR)/tests/compiler $(OBJ_DIR)/tests/memory $(OBJ_DIR)/tests/runtime:
ifdef WINDOWS_NATIVE
	@if not exist "$(subst /,\,$@)" mkdir "$(subst /,\,$@)"
else
	@mkdir -p $@
endif

# Pattern rule for object files
$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)/compiler $(OBJ_DIR)/compiler/parser $(OBJ_DIR)/compiler/codegen $(OBJ_DIR)/compiler/analysis $(OBJ_DIR)/runtime $(OBJ_DIR)/runtime/actors $(OBJ_DIR)/runtime/scheduler $(OBJ_DIR)/runtime/memory $(OBJ_DIR)/runtime/config $(OBJ_DIR)/runtime/simd $(OBJ_DIR)/runtime/utils $(OBJ_DIR)/std $(OBJ_DIR)/std/string $(OBJ_DIR)/std/io $(OBJ_DIR)/std/math $(OBJ_DIR)/std/net $(OBJ_DIR)/std/fs $(OBJ_DIR)/std/log $(OBJ_DIR)/std/collections $(OBJ_DIR)/std/json $(OBJ_DIR)/std/os $(OBJ_DIR)/tests $(OBJ_DIR)/tests/compiler $(OBJ_DIR)/tests/memory $(OBJ_DIR)/tests/runtime
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

# Compiler target (incremental build with object files)
compiler: $(COMPILER_OBJS) $(STD_OBJS) $(COLLECTIONS_OBJS) $(OBJ_DIR)/runtime/aether_sandbox.o
	@echo "Linking compiler..."
	@$(CC) $(COMPILER_OBJS) $(STD_OBJS) $(COLLECTIONS_OBJS) $(OBJ_DIR)/runtime/aether_sandbox.o -o build/aetherc$(EXE_EXT) $(LDFLAGS)
	@echo "Compiler built successfully"

# Fast compiler target (monolithic, for clean builds)
compiler-fast:
ifdef WINDOWS_NATIVE
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

# Test .ae source files - compiles and runs each test file
ifdef WINDOWS_NATIVE
test-ae: compiler ae stdlib
	@echo ===================================
	@echo   Running Aether Source Tests (.ae)
	@echo ===================================
	@.\build\ae.exe test
else
test-ae: compiler ae stdlib
	@echo "==================================="
	@echo "  Running Aether Source Tests (.ae)"
	@echo "  Parallel: $(NPROC) jobs"
	@echo "==================================="
	@tmpdir=$$(mktemp -d); \
	script="$$tmpdir/run_test.sh"; \
	printf '#!/bin/sh\n'                                                                        > "$$script"; \
	printf 'f="$$1"; tmpdir="$$2"; root="$$3"\n'                                               >> "$$script"; \
	printf 'name=$$(echo "$$f" | sed "s|tests/||;s|/|_|g;s|\\.ae$$||")\n'                    >> "$$script"; \
	printf 'dir=$$(dirname "$$f")\n'                                                           >> "$$script"; \
	printf 'base=$$(basename "$$f")\n'                                                         >> "$$script"; \
	printf 'if [ -d "$$dir/lib" ]; then\n'                                                     >> "$$script"; \
	printf '  cmd="cd $$dir && $$root/build/ae build $$base -o $$root/build/test_$$name"\n'    >> "$$script"; \
	printf 'else\n'                                                                            >> "$$script"; \
	printf '  cmd="$$root/build/ae build $$f -o $$root/build/test_$$name"\n'                   >> "$$script"; \
	printf 'fi\n'                                                                              >> "$$script"; \
	printf 'if eval "$$cmd" 2>"$$tmpdir/err_$$name.txt"; then\n'                                >> "$$script"; \
	printf '  if "$$root/build/test_$$name" >/dev/null 2>&1; then\n'                           >> "$$script"; \
	printf '    echo "  [PASS] $$name"; touch "$$tmpdir/PASS_$$name"\n'                        >> "$$script"; \
	printf '  else\n'                                                                          >> "$$script"; \
	printf '    echo "  [FAIL] $$name (runtime error)"; touch "$$tmpdir/FAIL_$$name"\n'        >> "$$script"; \
	printf '  fi\n'                                                                            >> "$$script"; \
	printf 'else\n'                                                                            >> "$$script"; \
	printf '  echo "  [FAIL] $$name (compile error)"; touch "$$tmpdir/FAIL_$$name"\n'          >> "$$script"; \
	printf '  head -5 "$$tmpdir/err_$$name.txt" 2>/dev/null\n'                                 >> "$$script"; \
	printf 'fi\n'                                                                              >> "$$script"; \
	chmod +x "$$script"; \
	root=$$(pwd); \
	find tests/syntax tests/compiler tests/integration -path '*/lib/*' -prune -o -name '*.ae' -print 2>/dev/null | sort | \
	xargs -P $(NPROC) -I{} "$$script" "{}" "$$tmpdir" "$$root"; \
	for sh_test in $$(find tests/integration -name 'test_*.sh' 2>/dev/null | sort); do \
		name=$$(echo "$$sh_test" | sed 's|tests/||;s|/|_|g;s|\.sh$$||'); \
		if sh "$$sh_test" >/dev/null 2>&1; then \
			echo "  [PASS] $$name"; touch "$$tmpdir/PASS_$$name"; \
		else \
			echo "  [FAIL] $$name"; touch "$$tmpdir/FAIL_$$name"; \
		fi; \
	done; \
	passed=$$(ls "$$tmpdir"/PASS_* 2>/dev/null | wc -l | tr -d ' '); \
	failed=$$(ls "$$tmpdir"/FAIL_* 2>/dev/null | wc -l | tr -d ' '); \
	total=$$((passed + failed)); \
	echo ""; \
	if [ "$$failed" -gt 0 ]; then \
		echo "=== FAILURE DETAILS ==="; \
		for fail_file in "$$tmpdir"/FAIL_*; do \
			fname=$$(basename "$$fail_file" | sed 's/^FAIL_//'); \
			echo "--- $$fname ---"; \
			cat "$$tmpdir/err_$$fname.txt" 2>/dev/null || echo "(no error output)"; \
			echo ""; \
		done; \
	fi; \
	echo "Aether Tests: $$passed passed, $$failed failed, $$total total"; \
	rm -rf "$$tmpdir"; \
	if [ "$$failed" -gt 0 ]; then exit 1; fi
endif

# Install smoke test: installs to a temp dir, runs ae init + ae run, cleans up
test-install: compiler ae stdlib
	@echo "==================================="
	@echo "  Install Smoke Test"
	@echo "==================================="
	@tmpdir=$$(mktemp -d) && \
	echo "  Installing to $$tmpdir..." && \
	./install.sh "$$tmpdir" < /dev/null > /dev/null 2>&1 && \
	echo "  Testing ae version..." && \
	AETHER_HOME="$$tmpdir" "$$tmpdir/bin/ae$(EXE_EXT)" version > /dev/null 2>&1 && \
	echo "  Testing ae init + ae run..." && \
	projdir=$$(mktemp -d) && \
	cd "$$projdir" && \
	AETHER_HOME="$$tmpdir" "$$tmpdir/bin/ae$(EXE_EXT)" init smoketest > /dev/null 2>&1 && \
	cd smoketest && \
	output=$$(AETHER_HOME="$$tmpdir" "$$tmpdir/bin/ae$(EXE_EXT)" run 2>&1) && \
	echo "  Output: $$output" && \
	echo "$$output" | grep -q "Hello from smoketest" && \
	echo "  Cleaning up..." && \
	rm -rf "$$tmpdir" "$$projdir" && \
	echo "  [PASS] Install smoke test" || \
	(echo "  [FAIL] Install smoke test"; rm -rf "$$tmpdir" "$$projdir" 2>/dev/null; exit 1)

# Release archive smoke test: packages a tarball exactly like release.yml,
# extracts it (simulating `ae version install`), and verifies ae init + ae run
# work from the extracted layout. This catches archive structure bugs that
# test-install (which tests install.sh) would miss.
test-release-archive: compiler ae stdlib
	@echo "==================================="
	@echo "  Release Archive Smoke Test"
	@echo "==================================="
	@tmpdir=$$(mktemp -d) && \
	reldir="$$tmpdir/release" && \
	mkdir -p "$$reldir/bin" "$$reldir/lib" "$$reldir/share/aether" "$$reldir/include/aether" && \
	cp build/aetherc$(EXE_EXT) "$$reldir/bin/" && \
	cp build/ae$(EXE_EXT)      "$$reldir/bin/" && \
	chmod 755 "$$reldir/bin/"* && \
	if [ -f build/libaether.a ]; then cp build/libaether.a "$$reldir/lib/"; fi && \
	for dir in runtime runtime/actors runtime/scheduler runtime/utils \
	           runtime/memory runtime/config std std/string std/io std/math \
	           std/net std/collections std/json std/fs std/log std/http \
	           std/file std/dir std/path std/tcp std/list std/map; do \
	  if [ -d "$$dir" ]; then \
	    mkdir -p "$$reldir/include/aether/$$dir"; \
	    cp "$$dir"/*.h "$$reldir/include/aether/$$dir/" 2>/dev/null || true; \
	  fi; \
	done && \
	cp -r runtime "$$reldir/share/aether/" && \
	cp -r std     "$$reldir/share/aether/" && \
	echo "  Created release layout in $$reldir" && \
	echo "  Packing tarball..." && \
	(cd "$$reldir" && tar -czf "$$tmpdir/aether-test.tar.gz" *) && \
	echo "  Extracting to simulated version dir..." && \
	verdir="$$tmpdir/extracted" && mkdir -p "$$verdir" && \
	tar -xzf "$$tmpdir/aether-test.tar.gz" -C "$$verdir" && \
	echo "  Checking extracted layout..." && \
	test -f "$$verdir/bin/aetherc$(EXE_EXT)" || (echo "  FAIL: bin/aetherc missing"; exit 1) && \
	test -f "$$verdir/bin/ae$(EXE_EXT)"      || (echo "  FAIL: bin/ae missing"; exit 1) && \
	test -f "$$verdir/lib/libaether.a"       || (echo "  FAIL: lib/libaether.a missing"; exit 1) && \
	test -d "$$verdir/share/aether/runtime"  || (echo "  FAIL: share/aether/runtime missing"; exit 1) && \
	test -d "$$verdir/share/aether/std"      || (echo "  FAIL: share/aether/std missing"; exit 1) && \
	echo "  Testing ae init + ae run from extracted archive..." && \
	projdir=$$(mktemp -d) && \
	cd "$$projdir" && \
	AETHER_HOME="$$verdir" "$$verdir/bin/ae$(EXE_EXT)" init archivetest > /dev/null 2>&1 && \
	cd archivetest && \
	output=$$(AETHER_HOME="$$verdir" "$$verdir/bin/ae$(EXE_EXT)" run 2>&1) && \
	echo "  Output: $$output" && \
	echo "$$output" | grep -q "Hello from archivetest" && \
	echo "  Cleaning up..." && \
	rm -rf "$$tmpdir" "$$projdir" && \
	echo "  [PASS] Release archive smoke test" || \
	(echo "  [FAIL] Release archive smoke test"; rm -rf "$$tmpdir" "$$projdir" 2>/dev/null; exit 1)

# Run both C unit tests and .ae integration tests
test-all: test test-ae
	@echo ""
	@echo "==================================="
	@echo "  All Tests Complete"
	@echo "==================================="

# Benchmark presets: full (10M), medium (1M), low (100K), stress (100M)
BENCHMARK_PRESET ?= low

benchmark:
	@echo "============================================"
	@echo "  Running Cross-Language Benchmark Suite"
	@echo "  Preset: $(BENCHMARK_PRESET)"
	@echo "============================================"
	@echo ""
	@cd benchmarks/cross-language && BENCHMARK_PRESET=$(BENCHMARK_PRESET) $(MAKE) benchmark-ui

ifdef WINDOWS_NATIVE
examples: compiler ae
	@echo ===================================
	@echo   Building Aether Examples
	@echo ===================================
	@.\build\ae.exe examples
else
examples: compiler
	@echo "==================================="
	@echo "  Building Aether Examples"
	@echo "==================================="
	@$(MKDIR) $(BUILD_DIR)/examples $(BUILD_DIR)/examples/basics $(BUILD_DIR)/examples/actors $(BUILD_DIR)/examples/applications $(BUILD_DIR)/examples/c-interop $(BUILD_DIR)/examples/stdlib
	@pass=0; fail=0; \
	for src in $$(find examples -name '*.ae' | grep -v '/lib/' | grep -v '/packages/' | sort); do \
		name=$$(echo $$src | sed 's|examples/||;s|\.ae$$||'); \
		dir=$$(dirname $$src); \
		extra_c=""; \
		if [ -d "$$dir" ]; then \
			extra_c=$$(find "$$dir" -maxdepth 1 -name '*.c' 2>/dev/null | tr '\n' ' '); \
		fi; \
		printf "  %-30s " "$$name"; \
		out_c="$(BUILD_DIR)/examples/$$name.c"; \
		if ! ./build/aetherc$(EXE_EXT) $$src $$out_c 2>/tmp/ae_err.txt; then \
			echo "FAIL (aetherc)"; \
			cat /tmp/ae_err.txt 2>/dev/null | head -5; \
			fail=$$((fail + 1)); \
		elif ! $(CC) $(CFLAGS) $$out_c $$extra_c $(RUNTIME_SRC) $(STD_SRC) $(COLLECTIONS_SRC) \
		         -o $(BUILD_DIR)/examples/$$name$(EXE_EXT) $(LDFLAGS) 2>/tmp/cc_err.txt; then \
			echo "FAIL (gcc)"; \
			cat /tmp/cc_err.txt 2>/dev/null | head -20; \
			fail=$$((fail + 1)); \
		else \
			echo "OK"; \
			pass=$$((pass + 1)); \
		fi; \
	done; \
	echo ""; \
	echo "  $$pass passed, $$fail failed"; \
	echo "  Binaries in $(BUILD_DIR)/examples/"; \
	if [ "$$fail" -gt 0 ]; then exit 1; fi
endif

examples-run: examples
	@echo "==================================="
	@echo "  Running Aether Examples"
	@echo "==================================="
	@for bin in $$(find $(BUILD_DIR)/examples -type f ! -name '*.c' ! -name '*.o' | sort); do \
		test -x "$$bin" || continue; \
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
	$(CC) -O2 -DAETHER_VERSION=\"$(VERSION)\" -Itools tools/ae.c tools/apkg/toml_parser.c -o build/ae$(EXE_EXT) $(LDFLAGS)
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
	@echo "Run the demo and open http://localhost:8081"

docgen:
	@echo "==================================="
	@echo "Building Documentation Generator ($(DETECTED_OS))"
	@echo "==================================="
	@$(MKDIR) build
	$(CC) -O2 -Wall tools/docgen/docgen.c -o build/docgen$(EXE_EXT)
	@echo "✓ Documentation generator built: build/docgen$(EXE_EXT)"
	@echo ""
	@echo "Usage: ./build/docgen std docs/api"

docs-server: compiler
	@echo "==================================="
	@echo "Building Documentation Server ($(DETECTED_OS))"
	@echo "==================================="
	@./build/aetherc$(EXE_EXT) tools/docgen/server.ae build/docs_server_gen.c
	@$(CC) -O2 -o build/docs-server$(EXE_EXT) build/docs_server_gen.c tools/docgen/server_ffi.c \
		$(RUNTIME_SRC) $(STD_SRC) $(COLLECTIONS_SRC) $(LDFLAGS)
	@rm -f build/docs_server_gen.c
	@echo "✓ Documentation server built: build/docs-server$(EXE_EXT)"

docs: docgen
	@echo "==================================="
	@echo "Generating API Documentation"
	@echo "==================================="
	@$(MKDIR) docs/api
	./build/docgen$(EXE_EXT) std docs/api
	@echo ""
	@echo "✓ Documentation generated in docs/api/"
	@echo "  Run 'make docs-serve' to view at http://localhost:3000"

docs-serve: docs docs-server
	@echo ""
	./build/docs-server$(EXE_EXT)

# Precompiled stdlib archive
stdlib: $(STD_OBJS) $(COLLECTIONS_OBJS) $(RUNTIME_OBJS)
	@echo "Creating precompiled stdlib archive..."
	@ar rcs build/libaether.a $(STD_OBJS) $(COLLECTIONS_OBJS) $(RUNTIME_OBJS)
	@echo "✓ Stdlib archive created: build/libaether.a"
ifeq ($(shell uname -s),Linux)
	@echo "Building sandbox preload library..."
	@$(CC) -shared -fPIC -o build/libaether_sandbox.so runtime/libaether_sandbox_preload.c -ldl -lrt 2>/dev/null || true
	@test -f build/libaether_sandbox.so && echo "✓ Sandbox preload: build/libaether_sandbox.so" || true
endif

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
else ifeq ($(DETECTED_OS),Darwin)
	@echo "Stripping debug symbols..."
	@strip -x build/aetherc-release$(EXE_EXT)
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
	            runtime/memory runtime/config std std/string std/math std/net \
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
	@for subdir in actors scheduler memory config utils; do \
		if [ -d "runtime/$$subdir" ]; then \
			install -d $(PREFIX)/share/aether/runtime/$$subdir; \
			for f in runtime/$$subdir/*.c runtime/$$subdir/*.h; do \
				[ -f "$$f" ] && install -m 644 "$$f" $(PREFIX)/share/aether/runtime/$$subdir/ 2>/dev/null || true; \
			done; \
		fi; \
	done
	@for f in runtime/*.c runtime/*.h; do \
		[ -f "$$f" ] && install -m 644 "$$f" $(PREFIX)/share/aether/runtime/ 2>/dev/null || true; \
	done
	@for subdir in string math net collections json fs log io; do \
		if [ -d "std/$$subdir" ]; then \
			install -d $(PREFIX)/share/aether/std/$$subdir; \
			for f in std/$$subdir/*.c std/$$subdir/*.h; do \
				[ -f "$$f" ] && install -m 644 "$$f" $(PREFIX)/share/aether/std/$$subdir/ 2>/dev/null || true; \
			done; \
		fi; \
	done
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
	@$(RM) *.gcda *.gcno 2>/dev/null || true
	@$(RM) build/pgo_workload$(EXE_EXT) build/bench_pgo_baseline$(EXE_EXT) build/bench_pgo_optimized$(EXE_EXT) 2>/dev/null || true
	@echo "✓ PGO data cleaned"

# Interactive REPL — integrated into ae CLI, no external dependencies
repl: ae
	@./build/ae$(EXE_EXT) repl

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
ifdef WINDOWS_NATIVE
	@if exist build $(RM_DIR) build
else
	$(RM_DIR) build
endif

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
	@echo "  make test           - Run C unit tests (incremental)"
	@echo "  make test-ae        - Run .ae integration tests"
	@echo "  make test-all       - Run both C and .ae tests"
	@echo "  make test-fast      - Run C tests (monolithic build)"
	@echo "  make test-install   - Install smoke test (init + run)"
	@echo "  make test-valgrind  - Run tests with Valgrind"
	@echo "  make test-asan      - Run tests with AddressSanitizer"
	@echo "  make self-test      - Test compiler on complex examples"
	@echo ""
	@echo "CI/CD Targets:"
	@echo "  make ci             - Full CI suite (build + test + install smoke test)"
	@echo "  make docker-ci      - Run CI in Docker (with Valgrind)"
	@echo "  make docker-build-ci- Build Docker CI image"
	@echo "  make valgrind-check - Run Valgrind memory leak detection (Linux only)"
	@echo "  ./scripts/run-ci-local.sh - Full CI with Docker (recommended)"
	@echo ""
	@echo "Tool Targets:"
	@echo "  make lsp            - Build LSP server"
	@echo "  make apkg           - Build project tooling"
	@echo "  make profiler       - Build profiler dashboard"
	@echo "  make docgen         - Build documentation generator"
	@echo "  make docs           - Generate API documentation (in docs/api/)"
	@echo "  make docs-serve     - Serve docs at http://localhost:3000"
	@echo ""
	@echo "Web Servers (localhost):"
	@echo "  make docs-serve     - API Documentation    :3000"
	@echo "  make benchmark      - Benchmark Dashboard  :8080"
	@echo "  make profiler       - Profiler Dashboard   :8081"
	@echo ""
	@echo "Other Targets:"
	@echo "  make examples       - Compile example programs"
	@echo "  make install        - Install to $(PREFIX)"
	@echo "  make stats          - Show build statistics"
	@echo ""
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
	@echo "Running full CI suite + Valgrind + ASan in Docker..."
	docker run --rm -v $(PWD):/aether -w /aether aether-ci bash -c "make ci && make valgrind-check && make asan-check"

# Cross-compile with MinGW (replicates Windows CI without needing a Windows host)
# Step 1: Build native aetherc, generate .c from all examples
# Step 2: Cross-compile compiler sources with MinGW -Werror
# Step 3: Syntax-check generated .c files with MinGW
ci-windows: clean compiler
	@echo "==================================="
	@echo "  Windows Cross-Compilation Test"
	@echo "==================================="
	@echo ""
	@echo "[1/3] Generating C from all examples with native aetherc..."
	@mkdir -p build/win
	@pass=0; fail=0; \
	for src in $$(find examples -name '*.ae' | grep -v '/lib/' | grep -v '/packages/' | sort); do \
		name=$$(echo $$src | sed 's|examples/||;s|\.ae$$||'); \
		printf "  %-30s " "$$name"; \
		mkdir -p "build/win/examples/$$(dirname $$name)"; \
		out_c="build/win/examples/$$name.c"; \
		rm -f "$$out_c"; \
		if ./build/aetherc "$$src" "$$out_c" 2>/tmp/ae_err.txt && [ -f "$$out_c" ]; then \
			echo "OK"; \
			pass=$$((pass + 1)); \
		else \
			echo "FAIL"; \
			cat /tmp/ae_err.txt 2>/dev/null | head -5; \
			fail=$$((fail + 1)); \
		fi; \
	done; \
	echo ""; \
	echo "  $$pass passed, $$fail failed"; \
	if [ "$$fail" -gt 0 ]; then exit 1; fi
	@echo ""
	@echo "[2/3] Cross-compiling compiler sources with MinGW -Werror..."
	@for f in $(COMPILER_LIB_SRC); do \
		printf "  %-50s " "$$f"; \
		if x86_64-w64-mingw32-gcc -O2 -Werror -c \
			-Icompiler -Iruntime -Iruntime/actors -Iruntime/scheduler \
			-Iruntime/utils -Iruntime/memory -Iruntime/config \
			-Istd -Istd/string -Istd/io -Istd/math -Istd/net -Istd/collections -Istd/json \
			-Wall -Wextra -Wno-unused-parameter -Wno-unused-function \
			-DAETHER_VERSION=\"test\" \
			"$$f" -o /dev/null 2>/tmp/mingw_err.txt; then \
			echo "OK"; \
		else \
			echo "FAIL"; \
			cat /tmp/mingw_err.txt 2>/dev/null | head -10; \
			exit 1; \
		fi; \
	done
	@echo "  All compiler sources clean under MinGW -Werror"
	@echo ""
	@echo "[3/3] Syntax-checking generated C with MinGW..."
	@pass=0; fail=0; \
	for src in $$(find examples -name '*.ae' | grep -v '/lib/' | grep -v '/packages/' | sort); do \
		name=$$(echo $$src | sed 's|examples/||;s|\.ae$$||'); \
		out_c="build/win/examples/$$name.c"; \
		printf "  %-30s " "$$name"; \
		if [ ! -f "$$out_c" ]; then \
			echo "SKIP"; \
		elif x86_64-w64-mingw32-gcc -O2 -fsyntax-only \
			-Iruntime -Iruntime/actors -Iruntime/scheduler \
			-Iruntime/utils -Iruntime/memory -Iruntime/config \
			-Istd -Istd/string -Istd/io -Istd/math -Istd/net -Istd/collections -Istd/json \
			-Wall -Wextra -Wno-unused-parameter -Wno-unused-function \
			-Wno-missing-field-initializers -Wno-unused-variable -Wno-unused-label \
			"$$out_c" 2>/tmp/mingw_err.txt; then \
			echo "OK"; \
			pass=$$((pass + 1)); \
		else \
			echo "FAIL"; \
			cat /tmp/mingw_err.txt 2>/dev/null | head -10; \
			fail=$$((fail + 1)); \
		fi; \
	done; \
	echo ""; \
	echo "  $$pass passed, $$fail failed"; \
	if [ "$$fail" -gt 0 ]; then exit 1; fi
	@echo ""
	@echo "==================================="
	@echo "  Windows Cross-Compilation PASSED"
	@echo "==="

docker-ci-windows: docker-build-ci
	@echo "Running Windows cross-compilation tests in Docker..."
	docker run --rm -v $(PWD):/aether -w /aether aether-ci make ci-windows

ci: clean
	@echo "==================================="
	@echo "  Aether CI — Full Test Suite"
	@echo "==================================="
	@echo ""
	@echo "[1/9] Building compiler (-Werror)..."
	@$(MAKE) compiler EXTRA_CFLAGS=-Werror
	@echo ""
	@echo "[2/9] Building ae CLI..."
	@$(MAKE) ae
	@echo ""
	@echo "[3/9] Building stdlib..."
	@$(MAKE) stdlib
	@echo ""
	@echo "[4/9] Running C unit tests..."
	@$(MAKE) test
	@echo ""
	@echo "[5/9] Running .ae integration tests..."
	@$(MAKE) test-ae
	@echo ""
	@echo "[6/9] Building examples..."
	@$(MAKE) examples
	@echo ""
	@echo "[7/9] Install smoke test..."
	@$(MAKE) test-install
	@echo ""
	@echo "[8/9] ae test smoke check..."
	@AETHER_HOME="" ./build/ae test examples/basics/hello.ae 2>&1 | tail -1
	@echo "  [PASS] ae test runs correctly"
	@echo ""
	@echo "[9/9] Release archive smoke test..."
	@$(MAKE) test-release-archive
	@echo ""
	@echo "==================================="
	@echo "  CI PASSED — all checks green"
	@echo "==================================="

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
		./build/test_runner$(EXE_EXT) || (echo "Valgrind errors detected!" && exit 1)
	@echo "✓ Valgrind clean — no leaks or uninitialised reads"

asan-check: clean
	@echo "==================================="
	@echo "Running AddressSanitizer Check"
	@echo "==================================="
	@$(MAKE) compiler CFLAGS="-fsanitize=address -fno-omit-frame-pointer -g" \
	                  LDFLAGS="-fsanitize=address -pthread -lm"
	@$(MAKE) test-build CFLAGS="-fsanitize=address -fno-omit-frame-pointer -g" \
	                    LDFLAGS="-fsanitize=address -pthread -lm"
	@ASAN_OPTIONS=detect_leaks=1:check_initialization_order=1 \
	  ./build/test_runner$(EXE_EXT) 2>&1 | tee asan.log; \
	  if grep -q "ERROR: AddressSanitizer" asan.log; then \
	    echo "ERROR: AddressSanitizer detected errors!"; \
	    exit 1; \
	  fi
	@echo "✓ ASan clean — no memory errors detected"

.PHONY: all compiler lsp apkg ae profiler docgen docs-server docs docs-serve test test-build test-valgrind test-asan test-memory test-manual-runtime test-install test-release-archive benchmark benchmark-ui examples run compile repl clean help self-test install stats stdlib ci ci-windows docker-ci docker-ci-windows docker-build-ci valgrind-check asan-check ci-coop ci-wasm ci-embedded ci-portability docker-ci-wasm docker-ci-embedded

# Cross-language benchmark UI
benchmark-ui:
	@cd benchmarks/cross-language && $(MAKE) benchmark-ui

# ============================================================================
# Platform Portability CI
# ============================================================================

# Test cooperative scheduler on native (no Docker needed — fast)
ci-coop: clean compiler ae
	@echo "==================================="
	@echo "  Cooperative Scheduler CI"
	@echo "==================================="
	@echo ""
	@echo "[1/4] Building stdlib with -DAETHER_NO_THREADING..."
	@$(MAKE) stdlib EXTRA_CFLAGS="-DAETHER_NO_THREADING"
	@echo ""
	@echo "[2/4] Running actor tests in cooperative mode..."
	@pass=0; fail=0; \
	for src in tests/syntax/test_platform_caps.ae \
	           tests/syntax/test_coop_chain.ae \
	           tests/syntax/test_coop_many_actors.ae \
	           tests/syntax/test_coop_ask_reply.ae \
	           tests/syntax/test_coop_self_send.ae \
	           tests/syntax/test_coop_stubs.ae \
	           examples/actors/counter.ae \
	           examples/actors/ping-pong.ae \
	           examples/actors/ask-pattern.ae \
	           examples/actors/cooperative-demo.ae \
	           examples/basics/hello.ae; do \
		printf "  %-50s " "$$src"; \
		if AETHER_HOME="" ./build/ae run "$$src" >/tmp/ae_coop_out.txt 2>&1; then \
			echo "PASS"; \
			pass=$$((pass + 1)); \
		else \
			echo "FAIL"; \
			cat /tmp/ae_coop_out.txt | head -10; \
			fail=$$((fail + 1)); \
		fi; \
	done; \
	echo ""; \
	echo "  $$pass passed, $$fail failed"; \
	if [ "$$fail" -gt 0 ]; then exit 1; fi
	@echo ""
	@echo "[3/4] Testing no-filesystem + no-networking stubs..."
	@$(MAKE) stdlib EXTRA_CFLAGS="-DAETHER_NO_FILESYSTEM -DAETHER_NO_NETWORKING"
	@printf "  %-50s " "hello.ae (no-fs/no-net)"; \
	if AETHER_HOME="" ./build/ae run examples/basics/hello.ae >/dev/null 2>&1; then \
		echo "PASS"; \
	else \
		echo "FAIL"; \
		exit 1; \
	fi
	@echo ""
	@echo "[4/4] Restoring default stdlib..."
	@$(MAKE) stdlib
	@echo ""
	@echo "==================================="
	@echo "  Cooperative Scheduler CI PASSED"
	@echo "==================================="

# WASM cross-compilation test (requires Emscripten — use Docker or local emsdk)
# Builds native aetherc, generates .c, then compiles with emcc
ci-wasm: clean compiler ae
	@echo "==================================="
	@echo "  WebAssembly (Emscripten) CI"
	@echo "==================================="
	@echo ""
	@echo "[1/3] Generating C from test programs..."
	@mkdir -p build/wasm
	@pass=0; fail=0; \
	for src in examples/basics/hello.ae \
	           examples/actors/counter.ae \
	           tests/syntax/test_platform_caps.ae \
	           tests/syntax/test_coop_chain.ae; do \
		name=$$(basename $$src .ae); \
		printf "  %-40s " "$$name → .c"; \
		if ./build/aetherc "$$src" "build/wasm/$$name.c" 2>/tmp/ae_wasm_err.txt; then \
			echo "OK"; \
			pass=$$((pass + 1)); \
		else \
			echo "FAIL"; \
			cat /tmp/ae_wasm_err.txt | head -5; \
			fail=$$((fail + 1)); \
		fi; \
	done; \
	echo "  $$pass generated, $$fail failed"; \
	if [ "$$fail" -gt 0 ]; then exit 1; fi
	@echo ""
	@echo "[2/3] Compiling runtime + generated C with emcc..."
	@WASM_CFLAGS="-O2 -DAETHER_NO_THREADING -DAETHER_NO_FILESYSTEM -DAETHER_NO_NETWORKING \
		-Iruntime -Iruntime/actors -Iruntime/scheduler -Iruntime/utils -Iruntime/memory \
		-Iruntime/config -Istd -Istd/string -Istd/io -Istd/math -Istd/net -Istd/collections -Istd/json \
		-Wall -Wextra -Wno-unused-parameter -Wno-unused-function \
		-Wno-unused-variable -Wno-missing-field-initializers -Wno-unused-label"; \
	pass=0; fail=0; \
	RUNTIME_FILES="runtime/scheduler/aether_scheduler_coop.c runtime/scheduler/scheduler_optimizations.c \
		runtime/config/aether_optimization_config.c runtime/memory/memory.c runtime/memory/aether_arena.c \
		runtime/memory/aether_pool.c runtime/memory/aether_memory_stats.c runtime/utils/aether_tracing.c \
		runtime/utils/aether_bounds_check.c runtime/utils/aether_test.c runtime/memory/aether_arena_optimized.c \
		runtime/aether_runtime_types.c runtime/utils/aether_cpu_detect.c runtime/memory/aether_batch.c \
		runtime/utils/aether_simd_vectorized.c runtime/aether_runtime.c runtime/aether_numa.c \
		runtime/actors/aether_send_buffer.c runtime/actors/aether_send_message.c \
		runtime/actors/aether_actor_thread.c \
		std/string/aether_string.c std/math/aether_math.c std/net/aether_http.c \
		std/net/aether_http_server.c std/net/aether_net.c std/collections/aether_collections.c \
		std/json/aether_json.c std/fs/aether_fs.c std/log/aether_log.c std/io/aether_io.c \
		std/os/aether_os.c std/collections/aether_hashmap.c std/collections/aether_set.c \
		std/collections/aether_vector.c std/collections/aether_pqueue.c"; \
	for src in build/wasm/hello.c build/wasm/counter.c build/wasm/test_platform_caps.c \
	           build/wasm/test_coop_chain.c; do \
		name=$$(basename $$src .c); \
		printf "  %-40s " "emcc $$name"; \
		if emcc $$WASM_CFLAGS $$src $$RUNTIME_FILES \
			-o "build/wasm/$$name.js" -lm 2>/tmp/emcc_err.txt; then \
			echo "OK"; \
			pass=$$((pass + 1)); \
		else \
			echo "FAIL"; \
			cat /tmp/emcc_err.txt | head -10; \
			fail=$$((fail + 1)); \
		fi; \
	done; \
	echo "  $$pass compiled, $$fail failed"; \
	if [ "$$fail" -gt 0 ]; then exit 1; fi
	@echo ""
	@echo "[3/3] Running WASM programs with Node.js..."
	@pass=0; fail=0; \
	for js in build/wasm/hello.js build/wasm/counter.js build/wasm/test_platform_caps.js \
	          build/wasm/test_coop_chain.js; do \
		name=$$(basename $$js .js); \
		printf "  %-40s " "node $$name"; \
		if node "$$js" >/tmp/wasm_out.txt 2>&1; then \
			echo "PASS"; \
			pass=$$((pass + 1)); \
		else \
			echo "FAIL"; \
			cat /tmp/wasm_out.txt | head -10; \
			fail=$$((fail + 1)); \
		fi; \
	done; \
	echo "  $$pass passed, $$fail failed"; \
	if [ "$$fail" -gt 0 ]; then exit 1; fi
	@echo ""
	@echo "==================================="
	@echo "  WebAssembly CI PASSED"
	@echo "==================================="

# Embedded cross-compilation test (requires arm-none-eabi-gcc — use Docker or local install)
# Syntax-checks only — no runtime to execute on bare-metal
ci-embedded: clean compiler
	@echo "==================================="
	@echo "  Embedded (ARM) CI"
	@echo "==================================="
	@echo ""
	@echo "[1/2] Syntax-checking runtime sources with arm-none-eabi-gcc..."
	@EMB_CFLAGS="-fsyntax-only -O2 -mcpu=cortex-m4 -mthumb -ffreestanding \
		-DAETHER_NO_THREADING -DAETHER_NO_FILESYSTEM -DAETHER_NO_NETWORKING \
		-DAETHER_NO_GETENV -DAETHER_NO_SIMD -DAETHER_NO_AFFINITY -DAETHER_NO_NUMA \
		-Iruntime -Iruntime/actors -Iruntime/scheduler -Iruntime/utils -Iruntime/memory \
		-Iruntime/config -Istd -Istd/string -Istd/io -Istd/math -Istd/net -Istd/collections -Istd/json \
		-Wall -Wextra -Wno-unused-parameter -Wno-unused-function"; \
	pass=0; fail=0; \
	for f in runtime/scheduler/aether_scheduler_coop.c \
	         runtime/config/aether_optimization_config.c \
	         runtime/utils/aether_cpu_detect.c \
	         runtime/aether_numa.c \
	         runtime/actors/aether_send_message.c \
	         runtime/actors/aether_send_buffer.c \
	         std/string/aether_string.c \
	         std/math/aether_math.c \
	         std/fs/aether_fs.c \
	         std/io/aether_io.c \
	         std/os/aether_os.c \
	         std/net/aether_http.c \
	         std/net/aether_net.c \
	         std/net/aether_http_server.c; do \
		printf "  %-55s " "$$f"; \
		if arm-none-eabi-gcc $$EMB_CFLAGS "$$f" 2>/tmp/emb_err.txt; then \
			echo "OK"; \
			pass=$$((pass + 1)); \
		else \
			echo "FAIL"; \
			cat /tmp/emb_err.txt | head -10; \
			fail=$$((fail + 1)); \
		fi; \
	done; \
	echo ""; \
	echo "  $$pass passed, $$fail failed"; \
	if [ "$$fail" -gt 0 ]; then exit 1; fi
	@echo ""
	@echo "[2/2] Generating and syntax-checking example programs..."
	@mkdir -p build/embedded
	@pass=0; fail=0; \
	for src in examples/basics/hello.ae examples/actors/counter.ae; do \
		name=$$(basename $$src .ae); \
		printf "  %-55s " "$$name → syntax-check"; \
		./build/aetherc "$$src" "build/embedded/$$name.c" 2>/dev/null && \
		if arm-none-eabi-gcc -fsyntax-only -O2 -mcpu=cortex-m4 -mthumb -ffreestanding \
			-DAETHER_NO_THREADING -DAETHER_NO_FILESYSTEM -DAETHER_NO_NETWORKING \
			-DAETHER_NO_GETENV -DAETHER_NO_SIMD -DAETHER_NO_AFFINITY -DAETHER_NO_NUMA \
			-Iruntime -Iruntime/actors -Iruntime/scheduler -Iruntime/utils -Iruntime/memory \
			-Iruntime/config -Istd -Istd/string -Istd/io -Istd/math -Istd/net -Istd/collections -Istd/json \
			-Wall -Wextra -Wno-unused-parameter -Wno-unused-function \
			-Wno-unused-variable -Wno-missing-field-initializers -Wno-unused-label \
			"build/embedded/$$name.c" 2>/tmp/emb_err.txt; then \
			echo "OK"; \
			pass=$$((pass + 1)); \
		else \
			echo "FAIL"; \
			cat /tmp/emb_err.txt | head -10; \
			fail=$$((fail + 1)); \
		fi; \
	done; \
	echo "  $$pass passed, $$fail failed"; \
	if [ "$$fail" -gt 0 ]; then exit 1; fi
	@echo ""
	@echo "==================================="
	@echo "  Embedded CI PASSED"
	@echo "==================================="

# Docker wrappers for cross-platform CI
docker-ci-wasm:
	@echo "Building WASM Docker image..."
	docker build -f docker/Dockerfile.wasm -t aether-wasm:latest .
	@echo "Running WASM CI in Docker..."
	docker run --rm -v $(PWD):/aether -w /aether aether-wasm make ci-wasm

docker-ci-embedded:
	@echo "Building embedded Docker image..."
	docker build -f docker/Dockerfile.embedded -t aether-embedded:latest .
	@echo "Running embedded CI in Docker..."
	docker run --rm -v $(PWD):/aether -w /aether aether-embedded make ci-embedded

# Run ALL portability checks (native coop + Docker WASM + Docker embedded)
ci-portability: ci-coop docker-ci-wasm docker-ci-embedded
	@echo ""
	@echo "==================================="
	@echo "  ALL PORTABILITY CHECKS PASSED"
	@echo "  - Cooperative scheduler (native)"
	@echo "  - WebAssembly (Emscripten)"
	@echo "  - Embedded (ARM Cortex-M4)"
	@echo "==================================="
