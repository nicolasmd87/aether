# Aether Language Compiler - Professional Makefile
# Supports both Windows (MinGW) and Linux

# Compiler and flags
CC = gcc
CFLAGS = -O2 -Wall -Wextra -std=c99
LDFLAGS = -lpthread

# Detect OS
ifeq ($(OS),Windows_NT)
    TARGET = aetherc.exe
    RM = del /Q
    MKDIR = mkdir
    COPY = copy
else
    TARGET = aetherc
    RM = rm -f
    MKDIR = mkdir -p
    COPY = cp
endif

# Source files
LEXER_SRC = lexer.c
PARSER_SRC = parser.c
AST_SRC = ast.c
TYPECHECKER_SRC = typechecker.c
CODEGEN_SRC = codegen.c
MAIN_SRC = aetherc.c

# Object files
LEXER_OBJ = lexer.o
PARSER_OBJ = parser.o
AST_OBJ = ast.o
TYPECHECKER_OBJ = typechecker.o
CODEGEN_OBJ = codegen.o
MAIN_OBJ = aetherc.o

# Runtime files
RUNTIME_SRC = ../runtime/actor.c ../runtime/scheduler.c ../runtime/memory.c
RUNTIME_OBJ = actor.o scheduler.o memory.o

# Default target
all: $(TARGET)

# Main target
$(TARGET): $(MAIN_OBJ) $(LEXER_OBJ) $(PARSER_OBJ) $(AST_OBJ) $(TYPECHECKER_OBJ) $(CODEGEN_OBJ) $(RUNTIME_OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $^ $(LDFLAGS)

# Compile main source
$(MAIN_OBJ): $(MAIN_SRC) tokens.h ast.h parser.h typechecker.h codegen.h
	$(CC) $(CFLAGS) -c $(MAIN_SRC)

# Compile lexer
$(LEXER_OBJ): $(LEXER_SRC) tokens.h
	$(CC) $(CFLAGS) -c $(LEXER_SRC)

# Compile parser
$(PARSER_OBJ): $(PARSER_SRC) tokens.h ast.h parser.h
	$(CC) $(CFLAGS) -c $(PARSER_SRC)

# Compile AST
$(AST_OBJ): $(AST_SRC) ast.h tokens.h
	$(CC) $(CFLAGS) -c $(AST_SRC)

# Compile typechecker
$(TYPECHECKER_OBJ): $(TYPECHECKER_SRC) ast.h typechecker.h
	$(CC) $(CFLAGS) -c $(TYPECHECKER_SRC)

# Compile codegen
$(CODEGEN_OBJ): $(CODEGEN_SRC) ast.h codegen.h
	$(CC) $(CFLAGS) -c $(CODEGEN_SRC)

# Compile runtime
$(RUNTIME_OBJ): $(RUNTIME_SRC)
	$(CC) $(CFLAGS) -c ../runtime/actor.c
	$(CC) $(CFLAGS) -c ../runtime/scheduler.c
	$(CC) $(CFLAGS) -c ../runtime/memory.c

# Build runtime library
runtime: $(RUNTIME_OBJ)
	ar rcs libaetheruntime.a $(RUNTIME_OBJ)

# Test compilation
test: $(TARGET)
	@echo "Testing Aether compiler..."
	@echo 'main() { print("Hello, Aether!"); }' > test.ae
	@./$(TARGET) test.ae test.c
	@$(CC) test.c ../runtime/*.c -o test_program $(LDFLAGS)
	@./test_program
	@$(RM) test.ae test.c test_program$(if $(findstring Windows,$(OS)),.exe,)
	@echo "Test completed successfully!"

# Clean build artifacts
clean:
	@echo "Cleaning build files..."
	$(RM) *.o $(TARGET) libaetheruntime.a
	$(RM) test.ae test.c test_program$(if $(findstring Windows,$(OS)),.exe,)

# Clean everything including runtime
clean-all: clean
	$(RM) ../runtime/*.o

# Install (optional)
install: $(TARGET)
ifeq ($(OS),Windows_NT)
	@echo "Windows installation: Copy $(TARGET) to your PATH"
else
	$(COPY) $(TARGET) /usr/local/bin/
	@echo "Aether compiler installed to /usr/local/bin/$(TARGET)"
endif

# Debug build
debug: CFLAGS = -g -O0 -Wall -Wextra -std=c99 -DDEBUG
debug: $(TARGET)

# Release build
release: CFLAGS = -O3 -Wall -Wextra -std=c99 -DNDEBUG
release: clean $(TARGET)

# Help
help:
	@echo "Aether Language Compiler Makefile"
	@echo "Available targets:"
	@echo "  all       - Build the compiler (default)"
	@echo "  test      - Build and run a test program"
	@echo "  clean     - Remove build artifacts"
	@echo "  clean-all - Remove all generated files"
	@echo "  debug     - Build with debug symbols"
	@echo "  release   - Build optimized release version"
	@echo "  install   - Install compiler to system"
	@echo "  help      - Show this help message"

.PHONY: all test clean clean-all install debug release help runtime