// Java ping-pong benchmark using threads and BlockingQueue
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicLong;

public class PingPong {
    private static int getMessages() {
        String env = System.getenv("BENCHMARK_MESSAGES");
        return env != null ? Integer.parseInt(env) : 100000;
    }
    private static final int MESSAGES = getMessages();

    static class PingThread extends Thread {
        private final BlockingQueue<Integer> sendQueue;
        private final BlockingQueue<Integer> recvQueue;

        public PingThread(BlockingQueue<Integer> send, BlockingQueue<Integer> recv) {
            this.sendQueue = send;
            this.recvQueue = recv;
        }

        @Override
        public void run() {
            try {
                for (int i = 0; i < MESSAGES; i++) {
                    sendQueue.put(i);
                    int received = recvQueue.take();
                    // VALIDATE: Must receive echo of what we sent
                    if (received != i) {
                        System.err.println("ERROR: Ping sent " + i + " but got back " + received);
                    }
                }
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
    }

    static class PongThread extends Thread {
        private final BlockingQueue<Integer> sendQueue;
        private final BlockingQueue<Integer> recvQueue;

        public PongThread(BlockingQueue<Integer> send, BlockingQueue<Integer> recv) {
            this.sendQueue = send;
            this.recvQueue = recv;
        }

        @Override
        public void run() {
            try {
                for (int i = 0; i < MESSAGES; i++) {
                    int received = recvQueue.take();
                    // VALIDATE: Must receive expected sequence
                    if (received != i) {
                        System.err.println("ERROR: Pong expected " + i + " but got " + received);
                    }
                    // Echo back what we received
                    sendQueue.put(received);
                }
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
    }

    private static long rdtsc() {
        // On ARM (Apple Silicon), use System.nanoTime()
        return System.nanoTime();
    }

    public static void main(String[] args) throws InterruptedException {
        System.out.println("=== Java Ping-Pong Benchmark ===");
        System.out.println("Messages: " + MESSAGES);
        System.out.println("Using Java threads with ArrayBlockingQueue");
        System.out.println();

        BlockingQueue<Integer> queueA = new ArrayBlockingQueue<>(1);
        BlockingQueue<Integer> queueB = new ArrayBlockingQueue<>(1);

        PingThread ping = new PingThread(queueA, queueB);
        PongThread pong = new PongThread(queueB, queueA);

        long start = rdtsc();

        ping.start();
        pong.start();

        ping.join();
        pong.join();

        long end = rdtsc();
        long totalNs = end - start;

        double nsPerMsg = (double) totalNs / MESSAGES;
        double throughput = 1e9 / nsPerMsg;
        double cyclesPerMsg = nsPerMsg * 3.0; // Approximate at 3GHz

        System.out.printf("Cycles/msg:     %.2f\n", cyclesPerMsg);
        System.out.printf("Throughput:     %.2f M msg/sec\n", throughput / 1e6);
    }
}
