// Java Fork-Join Benchmark (Savina-style)
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.CountDownLatch;

public class ForkJoin {
    private static final int NUM_WORKERS = 8;
    private static int getMessages() {
        String env = System.getenv("BENCHMARK_MESSAGES");
        return env != null ? Integer.parseInt(env) : 100000;
    }
    private static final int MESSAGES_PER_WORKER = getMessages();

    static class Worker implements Runnable {
        private final BlockingQueue<Integer> inbox;
        private final CountDownLatch done;
        private int processed = 0;

        Worker(CountDownLatch done) {
            this.inbox = new ArrayBlockingQueue<>(1024);
            this.done = done;
        }

        BlockingQueue<Integer> getInbox() {
            return inbox;
        }

        @Override
        public void run() {
            try {
                while (true) {
                    Integer msg = inbox.take();
                    if (msg == -1) {
                        done.countDown();
                        return;
                    }
                    processed++;
                }
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }

        int getProcessed() {
            return processed;
        }
    }

    public static void main(String[] args) throws InterruptedException {
        int total = NUM_WORKERS * MESSAGES_PER_WORKER;
        System.out.println("=== Java Fork-Join Throughput Benchmark ===");
        System.out.println("Workers: " + NUM_WORKERS + ", Messages: " + total + "\n");

        CountDownLatch done = new CountDownLatch(NUM_WORKERS);
        Worker[] workers = new Worker[NUM_WORKERS];
        Thread[] threads = new Thread[NUM_WORKERS];

        // Create and start workers
        for (int i = 0; i < NUM_WORKERS; i++) {
            workers[i] = new Worker(done);
            threads[i] = new Thread(workers[i]);
            threads[i].start();
        }

        long start = System.nanoTime();

        // Send messages round-robin
        for (int i = 0; i < total; i++) {
            workers[i % NUM_WORKERS].getInbox().put(i);
        }

        // Signal workers to finish
        for (int i = 0; i < NUM_WORKERS; i++) {
            workers[i].getInbox().put(-1);
        }

        // Wait for all workers
        done.await();

        long end = System.nanoTime();
        double elapsedSec = (end - start) / 1e9;

        // Collect results
        int totalProcessed = 0;
        for (int i = 0; i < NUM_WORKERS; i++) {
            totalProcessed += workers[i].getProcessed();
        }

        if (totalProcessed != total) {
            System.out.println("VALIDATION FAILED: expected " + total + ", got " + totalProcessed);
        }

        double throughput = total / elapsedSec / 1e6;
        double cyclesPerMsg = elapsedSec * 3e9 / total;

        System.out.printf("Cycles/msg:     %.2f%n", cyclesPerMsg);
        System.out.printf("Throughput:     %.2f M msg/sec%n", throughput);
    }
}
