// Java Thread Ring Benchmark (Savina-style)
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.CountDownLatch;

public class ThreadRing {
    private static final int RING_SIZE = 100;
    private static int getMessages() {
        String env = System.getenv("BENCHMARK_MESSAGES");
        return env != null ? Integer.parseInt(env) : 100000;
    }
    private static final int NUM_HOPS = getMessages();

    static class RingNode implements Runnable {
        private final BlockingQueue<Integer> inbox;
        private BlockingQueue<Integer> next;
        private final CountDownLatch done;
        private int received = 0;

        RingNode(CountDownLatch done) {
            // Queue size 8 reduces context switching while preserving ordering
            this.inbox = new ArrayBlockingQueue<>(8);
            this.done = done;
        }

        void setNext(BlockingQueue<Integer> next) {
            this.next = next;
        }

        BlockingQueue<Integer> getInbox() {
            return inbox;
        }

        @Override
        public void run() {
            try {
                while (true) {
                    int token = inbox.take();
                    received++;
                    if (token == 0) {
                        done.countDown();
                        return;
                    }
                    next.put(token - 1);
                }
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }

        int getReceived() {
            return received;
        }
    }

    public static void main(String[] args) throws InterruptedException {
        System.out.println("=== Java Thread Ring Benchmark ===");
        System.out.println("Ring size: " + RING_SIZE + ", Hops: " + NUM_HOPS + "\n");

        CountDownLatch done = new CountDownLatch(1);
        RingNode[] nodes = new RingNode[RING_SIZE];
        Thread[] threads = new Thread[RING_SIZE];

        // Create nodes
        for (int i = 0; i < RING_SIZE; i++) {
            nodes[i] = new RingNode(done);
        }

        // Link nodes in ring
        for (int i = 0; i < RING_SIZE; i++) {
            nodes[i].setNext(nodes[(i + 1) % RING_SIZE].getInbox());
        }

        // Start threads
        for (int i = 0; i < RING_SIZE; i++) {
            threads[i] = new Thread(nodes[i]);
            threads[i].start();
        }

        long start = System.nanoTime();

        // Inject token
        nodes[0].getInbox().put(NUM_HOPS);

        // Wait for completion
        done.await();

        long end = System.nanoTime();
        double elapsedSec = (end - start) / 1e9;

        int totalMessages = NUM_HOPS + 1;
        double throughput = totalMessages / elapsedSec / 1e6;
        double cyclesPerMsg = elapsedSec * 3e9 / totalMessages;

        System.out.printf("Cycles/msg:     %.2f%n", cyclesPerMsg);
        System.out.printf("Throughput:     %.2f M msg/sec%n", throughput);

        // Cleanup - interrupt remaining threads
        for (Thread t : threads) {
            t.interrupt();
        }
    }
}
