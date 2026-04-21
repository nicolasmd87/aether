AETHER_COMPILER = ../../../build/aetherc
CC = gcc
CFLAGS = -O3 -march=native -I../../../compiler -I../../../runtime -I../../../runtime/actors -I../../../runtime/scheduler -I../../../runtime/config -I../../../runtime/utils -I../../../std -I../../../std/string -I../../../std/io -I../../../std/math -I../../../std/net -I../../../std/collections -I../../../std/json
LDFLAGS = -pthread -lm
RUNTIME_SRCS = ../../../runtime/scheduler/multicore_scheduler.c \
               ../../../runtime/scheduler/aether_io_poller_epoll.c \
               ../../../runtime/scheduler/aether_io_poller_kqueue.c \
               ../../../runtime/scheduler/aether_io_poller_poll.c \
               ../../../runtime/aether_numa.c \
               ../../../runtime/aether_runtime.c \
               ../../../runtime/config/aether_optimization_config.c \
               ../../../runtime/utils/aether_cpu_detect.c \
               ../../../runtime/actors/aether_send_buffer.c \
               ../../../runtime/actors/aether_send_message.c \
               ../../../runtime/actors/aether_actor_thread.c \
               ../../../runtime/actors/aether_panic.c

.PHONY: all clean ping_pong counting thread_ring fork_join skynet

all: ping_pong counting thread_ring fork_join skynet

ping_pong: ping_pong.ae
	@echo "Compiling Aether ping_pong..."
	@$(AETHER_COMPILER) ping_pong.ae /tmp/ping_pong_aether.c
	@$(CC) $(CFLAGS) /tmp/ping_pong_aether.c $(RUNTIME_SRCS) -o ping_pong $(LDFLAGS)
	@echo "Done: ./ping_pong"

counting: counting.ae
	@echo "Compiling Aether counting..."
	@$(AETHER_COMPILER) counting.ae /tmp/counting_aether.c
	@$(CC) $(CFLAGS) /tmp/counting_aether.c $(RUNTIME_SRCS) -o counting $(LDFLAGS)
	@echo "Done: ./counting"

thread_ring: thread_ring.ae
	@echo "Compiling Aether thread_ring..."
	@$(AETHER_COMPILER) thread_ring.ae /tmp/thread_ring_aether.c
	@$(CC) $(CFLAGS) /tmp/thread_ring_aether.c $(RUNTIME_SRCS) -o thread_ring $(LDFLAGS)
	@echo "Done: ./thread_ring"

fork_join: fork_join.ae
	@echo "Compiling Aether fork_join..."
	@$(AETHER_COMPILER) fork_join.ae /tmp/fork_join_aether.c
	@$(CC) $(CFLAGS) /tmp/fork_join_aether.c $(RUNTIME_SRCS) -o fork_join $(LDFLAGS)
	@echo "Done: ./fork_join"

skynet: skynet.ae
	@echo "Compiling Aether skynet..."
	@$(AETHER_COMPILER) skynet.ae /tmp/skynet_aether.c
	@$(CC) $(CFLAGS) /tmp/skynet_aether.c $(RUNTIME_SRCS) -o skynet $(LDFLAGS)
	@echo "Done: ./skynet"

clean:
	rm -f ping_pong counting thread_ring fork_join skynet /tmp/*_aether.c*
