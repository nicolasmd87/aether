.PHONY: all clean test compiler examples

CC = gcc
CFLAGS = -O2 -Icompiler -Iruntime
LDFLAGS = -pthread

COMPILER_SRC = compiler/aetherc.c compiler/lexer.c compiler/parser.c compiler/ast.c compiler/typechecker.c compiler/codegen.c
RUNTIME_SRC = runtime/multicore_scheduler.c

all: compiler

compiler:
	@mkdir -p build
	$(CC) $(CFLAGS) $(COMPILER_SRC) -o build/aetherc.exe

test: compiler
	@echo "Running compiler tests..."
	$(CC) tests/test_*.c $(COMPILER_SRC) -Icompiler -o build/test_runner.exe
	build/test_runner.exe
	@echo ""
	@echo "Running actor compilation tests..."
	build/aetherc.exe examples/test_actor_working.ae build/test1.c
	$(CC) -c build/test1.c -Iruntime -o build/test1.o
	build/aetherc.exe examples/test_multiple_actors.ae build/test2.c
	$(CC) -c build/test2.c -Iruntime -o build/test2.o
	@echo "All tests passed"

benchmark: compiler
	@echo "Single-core benchmark..."
	$(CC) examples/ring_benchmark_manual.c -Iruntime -O2 -o build/ring_bench.exe
	build/ring_bench.exe
	@echo ""
	@echo "Multi-core benchmark..."
	$(CC) examples/multicore_bench.c $(RUNTIME_SRC) -Iruntime $(LDFLAGS) -O2 -o build/mc_bench.exe
	build/mc_bench.exe

examples: compiler
	@echo "Compiling examples..."
	build/aetherc.exe examples/test_actor_working.ae build/actor1.c
	build/aetherc.exe examples/test_multiple_actors.ae build/actor2.c
	@echo "Examples compiled to build/"

clean:
	rm -rf build/*.exe build/*.o build/*.c examples/*.ae.c

help:
	@echo "Aether Build System"
	@echo ""
	@echo "Targets:"
	@echo "  make          - Build compiler"
	@echo "  make test     - Run all tests"
	@echo "  make benchmark - Run performance benchmarks"
	@echo "  make examples  - Compile example programs"
	@echo "  make clean     - Remove build artifacts"
