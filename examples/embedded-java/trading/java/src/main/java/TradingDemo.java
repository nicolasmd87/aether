/*
 * TradingDemo — a Java host driving an Aether-built trading namespace.
 *
 * Demonstrates the full v2 surface:
 *   - dlopen the namespace .so via the generated SDK class
 *   - introspect via describe()
 *   - register typed event handlers (claim-check pattern: thin
 *     notification with an id, host calls back to look up details)
 *   - call exported script functions
 *
 * Build (from this example's root):
 *   ae build --namespace aether/        # produces libtrading.so + Trading.java
 *   javac -d build java/src/main/java/TradingDemo.java aether/com/example/trading/Trading.java
 *   java --enable-native-access=ALL-UNNAMED \
 *        -cp build TradingDemo aether/libtrading.so
 *
 * The supplied build.sh script automates the above.
 */
import com.example.trading.Trading;

import java.util.HashMap;
import java.util.Map;

public class TradingDemo {

    /* Stand-in for a real TradeService — the host owns trade state and
     * decides what each event listener actually does. The Aether script
     * never sees this; it only emits notify(name, id). */
    private static final Map<Long, String> trades = new HashMap<>();

    public static void main(String[] args) {
        if (args.length < 1) {
            System.err.println("usage: TradingDemo <path-to-libtrading.so>");
            System.exit(2);
        }

        try (Trading t = new Trading(args[0])) {

            // 1. Discovery — confirm the loaded namespace is what we expected.
            Trading.Manifest m = t.describe();
            System.out.println("Loaded " + m + ", java=" + m.javaPackage + "." + m.javaClass);

            // 2. Wire host-supplied input. v1 stores it on the SDK
            // instance; the Aether script doesn't yet read it back
            // because host_call() isn't implemented.
            t.setMaxOrder(100_000);

            // 3. Register event handlers. Each follows the claim-check
            // pattern: notify gives us the id, we look up the rest via
            // our own typed services.
            t.onOrderPlaced(id -> {
                System.out.println("[event] OrderPlaced id=" + id
                    + " (Java now persists trade " + id + ")");
                trades.put(id, "PLACED");
            });
            t.onOrderRejected(id ->
                System.out.println("[event] OrderRejected id=" + id));
            t.onTradeKilled(id -> {
                System.out.println("[event] TradeKilled id=" + id);
                trades.put(id, "KILLED");
            });
            t.onUnknownTicker(id ->
                System.out.println("[event] UnknownTicker id=" + id));

            // 4. Exercise the script functions.
            System.out.println("--- placeTrade(100, 50_000, ticker_known=1) ---");
            int ok = t.placeTrade(100L, 50_000, 1);
            System.out.println("returned " + ok);

            System.out.println("--- placeTrade(101, 999_999_999, ticker_known=1) — over limit ---");
            ok = t.placeTrade(101L, 999_999_999, 1);
            System.out.println("returned " + ok);

            System.out.println("--- placeTrade(102, 1_000, ticker_known=0) — ticker unknown ---");
            ok = t.placeTrade(102L, 1_000, 0);
            System.out.println("returned " + ok);

            System.out.println("--- killTrade(100) ---");
            t.killTrade(100L);

            System.out.println("--- getTicker(\"ACME\") ---");
            String tk = t.getTicker("ACME");
            System.out.println("ticker = " + tk);

            // 5. Show the host-side state we built up from events.
            System.out.println("Final trade book: " + trades);
        }
    }
}
