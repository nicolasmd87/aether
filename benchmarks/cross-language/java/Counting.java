// Java Counting Actor Benchmark (Savina-style)
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;

public class Counting {
    private static int getMessages() {
        String env = System.getenv("BENCHMARK_MESSAGES");
        return env != null ? Integer.parseInt(env) : 100000;
    }
    private static final int MESSAGES = getMessages();

    public static void main(String[] args) throws InterruptedException {
        System.out.println("=== Java Counting Actor Benchmark ===");
        System.out.println("Messages: " + MESSAGES + "\n");

        BlockingQueue<Integer> queue = new ArrayBlockingQueue<>(1024);
        int[] count = {0};
        boolean[] done = {false};

        Thread counter = new Thread(() -> {
            try {
                while (true) {
                    Integer msg = queue.take();
                    if (msg == -1) break;
                    count[0]++;
                }
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        });
        counter.start();

        long start = System.nanoTime();

        // Send all messages
        for (int i = 0; i < MESSAGES; i++) {
            queue.put(i);
        }
        queue.put(-1); // Signal done

        counter.join();

        long end = System.nanoTime();
        double elapsedSec = (end - start) / 1e9;

        if (count[0] != MESSAGES) {
            System.out.println("VALIDATION FAILED: expected " + MESSAGES + ", got " + count[0]);
        }

        double throughput = MESSAGES / elapsedSec / 1e6;
        double cyclesPerMsg = elapsedSec * 3e9 / MESSAGES;

        System.out.printf("Cycles/msg:     %.2f%n", cyclesPerMsg);
        System.out.printf("Throughput:     %.2f M msg/sec%n", throughput);
    }
}
