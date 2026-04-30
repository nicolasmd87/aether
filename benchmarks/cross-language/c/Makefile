.PHONY: all clean ping_pong counting thread_ring fork_join skynet

CC = gcc
# -flto / -fno-plt match the Aether and C++ release flag set so the
# cross-language benchmark comparison is on equal footing. Languages
# whose compilers already enable LTO under `--release` (Rust via
# Cargo.toml profile, Pony, Zig ReleaseFast, Go) need no Makefile
# change here. Bytecode runtimes (Erlang, Elixir, Java, Scala) have no
# equivalent knob; their .beam / .class targets stay as-is.
CFLAGS = -O3 -march=native -flto -fno-plt -pthread -Wall
LDFLAGS = -flto -pthread

all: ping_pong counting thread_ring fork_join skynet

ping_pong: ping_pong.c
	$(CC) $(CFLAGS) ping_pong.c -o ping_pong $(LDFLAGS)

counting: counting.c
	$(CC) $(CFLAGS) counting.c -o counting $(LDFLAGS)

thread_ring: thread_ring.c
	$(CC) $(CFLAGS) thread_ring.c -o thread_ring $(LDFLAGS)

fork_join: fork_join.c
	$(CC) $(CFLAGS) fork_join.c -o fork_join $(LDFLAGS)

skynet: skynet.c
	$(CC) $(CFLAGS) skynet.c -o skynet $(LDFLAGS)

clean:
	rm -f ping_pong counting thread_ring fork_join skynet
