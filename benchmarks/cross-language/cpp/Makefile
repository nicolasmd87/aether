.PHONY: all clean ping_pong counting thread_ring fork_join skynet

CXX = g++
# -flto / -fno-plt match the Aether and C release flag set — same
# fairness rationale (see c/Makefile).
CXXFLAGS = -O3 -std=c++17 -march=native -flto -fno-plt -pthread -Wall
LDFLAGS = -flto -pthread

all: ping_pong counting thread_ring fork_join skynet

ping_pong: ping_pong.cpp
	$(CXX) $(CXXFLAGS) ping_pong.cpp -o ping_pong $(LDFLAGS)

counting: counting.cpp
	$(CXX) $(CXXFLAGS) counting.cpp -o counting $(LDFLAGS)

thread_ring: thread_ring.cpp
	$(CXX) $(CXXFLAGS) thread_ring.cpp -o thread_ring $(LDFLAGS)

fork_join: fork_join.cpp
	$(CXX) $(CXXFLAGS) fork_join.cpp -o fork_join $(LDFLAGS)

skynet: skynet.cpp
	$(CXX) $(CXXFLAGS) skynet.cpp -o skynet $(LDFLAGS)

clean:
	rm -f ping_pong counting thread_ring fork_join skynet
