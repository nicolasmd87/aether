/**
 * C++ Ping-Pong Benchmark
 * Fair comparison using proper synchronization primitives
 *
 * Three implementations provided:
 * 1. std::mutex + std::condition_variable (default, fair comparison to C pthread)
 * 2. std::atomic with proper barriers (lock-free but busy-wait)
 * 3. std::promise/std::future (message passing style)
 *
 * Compile with: g++ -O3 -std=c++17 -march=native ping_pong.cpp -o ping_pong -pthread
 */

#include <iostream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>
#include <cstdlib>

#ifdef _WIN32
#include <intrin.h>
static inline uint64_t rdtsc() { return __rdtsc(); }
#elif defined(__x86_64__) || defined(__i386__)
#include <x86intrin.h>
static inline uint64_t rdtsc() { return __rdtsc(); }
#else
// Fallback for non-x86
#include <chrono>
static inline uint64_t rdtsc() {
    return std::chrono::steady_clock::now().time_since_epoch().count();
}
#endif

static int MESSAGES = 100000;  // Default for "low" preset
#define USE_MUTEX 1  // Set to 0 for atomic busy-wait (unfair), 1 for mutex (fair)

#if USE_MUTEX

// FAIR IMPLEMENTATION: Using mutex + condition_variable
// This matches the C pthread implementation for fair comparison

struct Channel {
    std::mutex mtx;
    std::condition_variable cv;
    bool ready = false;
    int value = 0;
};

Channel chan_a, chan_b;

void ping_thread() {
    for (int i = 0; i < MESSAGES; i++) {
        // Send to A
        {
            std::unique_lock<std::mutex> lock(chan_a.mtx);
            chan_a.value = i;
            chan_a.ready = true;
        }
        chan_a.cv.notify_one();

        // Wait for B
        int received_value;
        {
            std::unique_lock<std::mutex> lock(chan_b.mtx);
            chan_b.cv.wait(lock, []{ return chan_b.ready; });
            received_value = chan_b.value;
            chan_b.ready = false;
        }

        // VALIDATE: Must receive echo of what we sent
        if (received_value != i) {
            std::cerr << "ERROR: Ping sent " << i << " but got back " << received_value << std::endl;
        }
    }
}

void pong_thread() {
    for (int i = 0; i < MESSAGES; i++) {
        // Wait for A
        int received_value;
        {
            std::unique_lock<std::mutex> lock(chan_a.mtx);
            chan_a.cv.wait(lock, []{ return chan_a.ready; });
            received_value = chan_a.value;
            chan_a.ready = false;
        }

        // VALIDATE: Must receive expected sequence
        if (received_value != i) {
            std::cerr << "ERROR: Pong expected " << i << " but got " << received_value << std::endl;
        }

        // Send to B - echo back what we received
        {
            std::unique_lock<std::mutex> lock(chan_b.mtx);
            chan_b.value = received_value;
            chan_b.ready = true;
        }
        chan_b.cv.notify_one();
    }
}

#else

// UNFAIR IMPLEMENTATION: Atomic busy-wait (for reference only)
// This is NOT a fair comparison - included only to show why it's unfair

std::atomic<int> ping_counter{0};
std::atomic<int> pong_counter{0};

void ping_thread() {
    for (int i = 0; i < MESSAGES; i++) {
        ping_counter.fetch_add(1, std::memory_order_release);
        // Busy-wait - wastes CPU cycles
        while (pong_counter.load(std::memory_order_acquire) < i) {
            std::this_thread::yield();
        }
    }
}

void pong_thread() {
    for (int i = 0; i < MESSAGES; i++) {
        // Busy-wait - wastes CPU cycles
        while (ping_counter.load(std::memory_order_acquire) <= i) {
            std::this_thread::yield();
        }
        pong_counter.fetch_add(1, std::memory_order_release);
    }
}

#endif

int main() {
    const char* env = getenv("BENCHMARK_MESSAGES");
    if (env) MESSAGES = atoi(env);

    std::cout << "=== C++ Ping-Pong Benchmark ===" << std::endl;
    std::cout << "Messages: " << MESSAGES << std::endl;
#if USE_MUTEX
    std::cout << "Using std::mutex + std::condition_variable (fair comparison)" << std::endl;
#else
    std::cout << "Using std::atomic with busy-wait (UNFAIR - burns CPU)" << std::endl;
#endif
    std::cout << std::endl;

    uint64_t start = rdtsc();

    std::thread t1(ping_thread);
    std::thread t2(pong_thread);

    t1.join();
    t2.join();

    uint64_t end = rdtsc();
    uint64_t cycles = end - start;
    double cycles_per_msg = (double)cycles / MESSAGES;
    double throughput = 3000.0 / cycles_per_msg; // MHz

    std::cout << "Cycles/msg:     " << cycles_per_msg << std::endl;
    std::cout << "Throughput:     " << std::fixed << std::setprecision(2) << throughput << " M msg/sec" << std::endl;

    return 0;
}
