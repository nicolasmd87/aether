// C++ Fork-Join Benchmark (Savina-style)
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <vector>
#include <queue>
#include <cstdlib>

constexpr int NUM_WORKERS = 8;
static int MESSAGES_PER_WORKER = 12500;  // Default for "low" preset (100000 / 8)

class Worker {
    std::mutex mtx;
    std::condition_variable cv;
    std::queue<int> inbox;
    bool done = false;
    int processed = 0;

public:
    void send(int value) {
        std::unique_lock<std::mutex> lock(mtx);
        inbox.push(value);
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
            cv.wait(lock, [this] { return !inbox.empty() || done; });

            while (!inbox.empty()) {
                inbox.pop();
                processed++;
            }

            if (done && inbox.empty()) break;
        }
    }

    int getProcessed() const { return processed; }
};

int main() {
    const char* env = getenv("BENCHMARK_MESSAGES");
    if (env) MESSAGES_PER_WORKER = atoi(env) / NUM_WORKERS;

    int total = NUM_WORKERS * MESSAGES_PER_WORKER;
    std::cout << "=== C++ Fork-Join Throughput Benchmark ===" << std::endl;
    std::cout << "Workers: " << NUM_WORKERS << ", Messages: " << total << "\n" << std::endl;

    std::vector<Worker> workers(NUM_WORKERS);
    std::vector<std::thread> threads;

    // Start workers
    for (int i = 0; i < NUM_WORKERS; i++) {
        threads.emplace_back([&workers, i] { workers[i].run(); });
    }

    auto start = std::chrono::high_resolution_clock::now();

    // Send messages round-robin
    for (int i = 0; i < total; i++) {
        workers[i % NUM_WORKERS].send(i);
    }

    // Signal workers to finish
    for (int i = 0; i < NUM_WORKERS; i++) {
        workers[i].finish();
    }

    // Wait for all workers
    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(end - start).count();

    // Collect results
    int total_processed = 0;
    for (int i = 0; i < NUM_WORKERS; i++) {
        total_processed += workers[i].getProcessed();
    }

    if (total_processed != total) {
        std::cout << "VALIDATION FAILED: expected " << total
                  << ", got " << total_processed << std::endl;
    }

    double throughput = total / elapsed / 1e6;
    double cycles_per_msg = elapsed * 3e9 / total;

    std::cout << "Cycles/msg:     " << std::fixed << cycles_per_msg << std::endl;
    std::cout << "Throughput:     " << throughput << " M msg/sec" << std::endl;

    return 0;
}
