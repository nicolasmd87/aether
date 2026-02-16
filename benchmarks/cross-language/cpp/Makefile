.PHONY: all clean ping_pong counting thread_ring fork_join

CXX = g++
CXXFLAGS = -O3 -std=c++17 -march=native -pthread -Wall

all: ping_pong counting thread_ring fork_join

ping_pong: ping_pong.cpp
	$(CXX) $(CXXFLAGS) ping_pong.cpp -o ping_pong

counting: counting.cpp
	$(CXX) $(CXXFLAGS) counting.cpp -o counting

thread_ring: thread_ring.cpp
	$(CXX) $(CXXFLAGS) thread_ring.cpp -o thread_ring

fork_join: fork_join.cpp
	$(CXX) $(CXXFLAGS) fork_join.cpp -o fork_join

clean:
	rm -f ping_pong counting thread_ring fork_join
