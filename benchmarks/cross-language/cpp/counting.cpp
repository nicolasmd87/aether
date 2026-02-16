// C++ Counting Actor Benchmark (Savina-style)
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>
#include <cstdlib>

static int MESSAGES = 100000;  // Default for "low" preset

class Counter {
    std::mutex mtx;
    std::condition_variable cv;
    int pending = 0;
    bool done = false;
    int processed = 0;

public:
    void increment() {
        std::unique_lock<std::mutex> lock(mtx);
        pending++;
        cv.notify_one();
    }

    void finish() {
        std::unique_lock<std::mutex> lock(mtx);
        done = true;
        cv.notify_one();
    }

    void run() {
        while (true) {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this] { return pending > 0 || done; });

            if (done && pending == 0) break;

            while (pending > 0) {
                pending--;
                processed++;
            }
        }
    }

    int getProcessed() const { return processed; }
};

int main() {
    const char* env = getenv("BENCHMARK_MESSAGES");
    if (env) MESSAGES = atoi(env);

    std::cout << "=== C++ Counting Actor Benchmark ===" << std::endl;
    std::cout << "Messages: " << MESSAGES << "\n" << std::endl;

    Counter counter;
    std::thread worker([&counter] { counter.run(); });

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < MESSAGES; i++) {
        counter.increment();
    }
    counter.finish();

    worker.join();

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(end - start).count();

    if (counter.getProcessed() != MESSAGES) {
        std::cout << "VALIDATION FAILED: expected " << MESSAGES
                  << ", got " << counter.getProcessed() << std::endl;
    }

    double throughput = MESSAGES / elapsed / 1e6;
    double cycles_per_msg = elapsed * 3e9 / MESSAGES;

    std::cout << "Cycles/msg:     " << std::fixed << cycles_per_msg << std::endl;
    std::cout << "Throughput:     " << throughput << " M msg/sec" << std::endl;

    return 0;
}
