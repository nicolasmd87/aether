# Makefile for Aether Project (Windows-friendly)

# Compiler variables
CC = gcc
AS = as
CFLAGS = -O2 -lpthread

# Files and paths
AE_COMPILER = aetherc.exe
AEC_SRC = src/aetherc.c
AE_SRC = src/main.ae
AE_GEN = build/main.c
ASM_SRC = asm/syscalls.s
ASM_OBJ = asm/syscalls.o
OUTPUT = aether_program.exe

# Test variables
TEST_DIR = tests
TEST_HARNESS = $(TEST_DIR)/test_harness.c
TEST_MAIN = $(TEST_DIR)/test_main.c
TEST_SOURCES = $(TEST_DIR)/test_examples.c $(TEST_DIR)/test_integration.c \
               $(TEST_DIR)/test_lexer.c $(TEST_DIR)/test_parser.c \
               $(TEST_DIR)/test_codegen.c $(TEST_DIR)/test_typechecker.c
TEST_EXE = build/test_runner.exe

.PHONY: all clean run test test-examples test-unit test-integration test-loop

all: $(OUTPUT)

$(OUTPUT): $(AE_GEN) $(ASM_OBJ)
	$(CC) $(AE_GEN) $(ASM_OBJ) -o $(OUTPUT) $(CFLAGS)

$(AE_GEN): $(AE_SRC) $(AE_COMPILER)
	./$(AE_COMPILER) $(AE_SRC) $(AE_GEN)

$(ASM_OBJ): $(ASM_SRC)
	$(AS) --64 $(ASM_SRC) -o $(ASM_OBJ)

$(AE_COMPILER): $(AEC_SRC)
	cd src && $(MAKE) -f Makefile
	@if exist src\aetherc.exe copy src\aetherc.exe $(AE_COMPILER)
	@if exist src\aetherc copy src\aetherc $(AE_COMPILER)

clean:
	@echo Cleaning build files...
	@if not exist build mkdir build
	@del /Q build\main.c 2>nul || true
	@del /Q asm\syscalls.o 2>nul || true
	@del /Q $(OUTPUT) 2>nul || true
	@del /Q $(AE_COMPILER) 2>nul || true
	@del /Q build\test_runner.exe 2>nul || true

run: all
	./$(OUTPUT)

# Test targets
test: $(TEST_EXE)
	@echo Running all tests...
	./$(TEST_EXE)

test-examples: $(TEST_EXE)
	@echo Running example tests...
	./$(TEST_EXE) examples

test-unit: $(TEST_EXE)
	@echo Running unit tests...
	./$(TEST_EXE) unit

test-integration: $(TEST_EXE)
	@echo Running integration tests...
	./$(TEST_EXE) integration

test-loop: $(TEST_EXE)
	@echo Running loop-specific tests...
	./$(TEST_EXE) loop

$(TEST_EXE): $(TEST_HARNESS) $(TEST_MAIN) $(TEST_SOURCES) $(AE_COMPILER)
	@echo Building test runner...
	@if not exist build mkdir build
	$(CC) $(TEST_HARNESS) $(TEST_MAIN) $(TEST_SOURCES) -o $(TEST_EXE) $(CFLAGS) -I$(TEST_DIR) -Isrc -Iruntime
