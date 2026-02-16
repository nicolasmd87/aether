# Aether Cross-Language Benchmark Suite

.PHONY: all benchmark-ui run-benchmarks build-ui clean help

# Default target
all: benchmark-ui

# Main target: Run benchmarks and launch UI
benchmark-ui: run-benchmarks build-ui
	@echo ""
	@echo "=========================================="
	@echo "  Launching Benchmark Visualization UI"
	@echo "=========================================="
	@echo ""
	@echo "Open your browser at http://localhost:8080"
	@echo "Press Ctrl+C to stop the server"
	@echo ""
	@cd visualize && ./server

# Run all benchmarks
run-benchmarks:
	@./run_benchmarks.sh

# Build the visualization server (Aether)
build-ui:
	@lsof -ti :8080 | xargs kill -9 2>/dev/null || true
	@pkill -9 -f "visualize/server" 2>/dev/null || true
	@sleep 1
	@echo "Building Aether HTTP server..."
	@cd visualize && ../../../build/aetherc server.ae server_generated.c && \
		gcc -O2 -o server server_generated.c server_ffi.c \
		-I../../../runtime -I../../../runtime/actors -I../../../runtime/scheduler \
		-I../../../runtime/memory -I../../../runtime/utils -I../../../runtime/config \
		-I../../../std -I../../../std/string \
		../../../runtime/scheduler/multicore_scheduler.c \
		../../../runtime/config/aether_optimization_config.c \
		../../../runtime/memory/memory.c ../../../runtime/memory/aether_arena.c \
		../../../runtime/memory/aether_pool.c ../../../runtime/memory/aether_memory_stats.c \
		../../../runtime/memory/aether_arena_optimized.c ../../../runtime/memory/aether_batch.c \
		../../../runtime/utils/aether_tracing.c ../../../runtime/utils/aether_bounds_check.c \
		../../../runtime/utils/aether_test.c ../../../runtime/utils/aether_cpu_detect.c \
		../../../runtime/aether_runtime_types.c ../../../runtime/aether_runtime.c \
		../../../runtime/aether_numa.c ../../../runtime/actors/aether_send_buffer.c \
		../../../runtime/actors/aether_actor.c \
		../../../runtime/actors/aether_actor_thread.c ../../../runtime/actors/aether_message_registry.c \
		../../../std/string/aether_string.c \
		-lpthread -lm
	@rm -f visualize/server_generated.c
	@echo "Server built successfully"

# Clean all build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@rm -f visualize/server visualize/server_generated.c
	@echo "Done!"

# Help target
help:
	@echo "Aether Cross-Language Benchmark Suite"
	@echo ""
	@echo "Targets:"
	@echo "  make benchmark-ui   - Run all benchmarks and launch visualization (default)"
	@echo "  make run-benchmarks - Only run benchmarks, don't launch UI"
	@echo "  make build-ui       - Only build the visualization server (written in Aether)"
	@echo "  make clean          - Clean all build artifacts"
	@echo "  make help           - Show this help message"
