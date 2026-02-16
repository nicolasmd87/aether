// C++ Thread Ring Benchmark (Savina-style)
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <vector>
#include <atomic>
#include <cstdlib>

constexpr int RING_SIZE = 100;
static int NUM_HOPS = 100000;  // Default for "low" preset

class RingNode {
    std::mutex mtx;
    std::condition_variable cv;
    int token = -1;
    bool has_token = false;
    bool done = false;
    RingNode* next = nullptr;
    int received = 0;
    std::atomic<bool>* completion;
    std::atomic<int>* final_received;

public:
    void setNext(RingNode* n) { next = n; }
    void setCompletion(std::atomic<bool>* c, std::atomic<int>* fr) {
        completion = c;
        final_received = fr;
    }

    void send(int t) {
        std::unique_lock<std::mutex> lock(mtx);
        token = t;
        has_token = true;
        cv.notify_one();
    }

    void stop() {
        std::unique_lock<std::mutex> lock(mtx);
        done = true;
        cv.notify_one();
    }

    void run() {
        while (true) {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this] { return has_token || done; });

            if (done) break;

            int t = token;
            has_token = false;
            received++;
            lock.unlock();

            if (t == 0) {
                final_received->store(received);
                completion->store(true);
                return;
            }

            next->send(t - 1);
        }
    }

    int getReceived() const { return received; }
};

int main() {
    const char* env = getenv("BENCHMARK_MESSAGES");
    if (env) NUM_HOPS = atoi(env);

    std::cout << "=== C++ Thread Ring Benchmark ===" << std::endl;
    std::cout << "Ring size: " << RING_SIZE << ", Hops: " << NUM_HOPS << "\n" << std::endl;

    std::vector<RingNode> nodes(RING_SIZE);
    std::vector<std::thread> threads;
    std::atomic<bool> completion{false};
    std::atomic<int> final_received{0};

    // Setup ring
    for (int i = 0; i < RING_SIZE; i++) {
        nodes[i].setNext(&nodes[(i + 1) % RING_SIZE]);
        nodes[i].setCompletion(&completion, &final_received);
    }

    // Start threads
    for (int i = 0; i < RING_SIZE; i++) {
        threads.emplace_back([&nodes, i] { nodes[i].run(); });
    }

    auto start = std::chrono::high_resolution_clock::now();

    // Inject token
    nodes[0].send(NUM_HOPS);

    // Wait for completion
    while (!completion.load()) {
        std::this_thread::yield();
    }

    auto end = std::chrono::high_resolution_clock::now();

    // Stop all threads
    for (int i = 0; i < RING_SIZE; i++) {
        nodes[i].stop();
    }

    for (auto& t : threads) {
        t.join();
    }

    double elapsed = std::chrono::duration<double>(end - start).count();
    int total_messages = NUM_HOPS + 1;

    double throughput = total_messages / elapsed / 1e6;
    double cycles_per_msg = elapsed * 3e9 / total_messages;

    std::cout << "Cycles/msg:     " << std::fixed << cycles_per_msg << std::endl;
    std::cout << "Throughput:     " << throughput << " M msg/sec" << std::endl;

    return 0;
}
