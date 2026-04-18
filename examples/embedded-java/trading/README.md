# Trading: an Aether namespace embedded in a Java host

This example demonstrates the v2 embedded-namespaces flow end to end.

```
trading/
  aether/
    manifest.ae         declares the namespace, inputs, events, bindings
    placeTrade.ae       script: place_trade(order_id, amount, ticker_known)
    killTrade.ae        script: kill_trade(trade_id)
    getTicker.ae        script: get_ticker(symbol)
  java/
    src/main/java/TradingDemo.java   the host application
  build.sh              one-shot build + run
```

## Run it

```sh
./build.sh
```

You'll need:

- The Aether toolchain (`ae`) on `PATH` — see the top-level `README.md`
- JDK 22 or later (`javac --version`)

## What happens

`ae build --namespace aether/`:

1. Reads `aether/manifest.ae`, captures the namespace declaration.
2. Walks `aether/` for sibling `*.ae` scripts.
3. Concatenates them into one synthetic translation unit, compiles
   via `--emit=lib` to `aether/libtrading.so`.
4. Embeds the manifest as a static struct, exports `aether_describe()`
   so the Java side can introspect at runtime.
5. Generates `aether/com/example/trading/Trading.java` — a typed Java
   class that hides Panama Foreign Function API plumbing behind methods
   like `setMaxOrder`, `onOrderPlaced`, `placeTrade`, `getTicker`, and
   `describe`.

`TradingDemo`:

1. Constructs `new Trading("aether/libtrading.so")`.
2. Calls `describe()` to verify the loaded namespace.
3. Wires inputs (`setMaxOrder`) and event handlers (`onOrderPlaced`,
   `onOrderRejected`, `onTradeKilled`, `onUnknownTicker`).
4. Calls each script function. Events fire back to Java synchronously
   following the EAI / Hohpe **claim-check** pattern: `notify(name, id)`
   only — the host looks up details via its own `TradeService` (here
   stubbed as a `HashMap`) using the id.
5. Prints the resulting trade book.

## What you should see

```
Loaded Manifest(namespace="trading", inputs=1, events=4), java=com.example.trading.Trading
--- placeTrade(100, 50_000, ticker_known=1) ---
[event] OrderPlaced id=100 (Java now persists trade 100)
returned 1
--- placeTrade(101, 999_999_999, ticker_known=1) — over limit ---
[event] OrderRejected id=101
returned 0
--- placeTrade(102, 1_000, ticker_known=0) — ticker unknown ---
[event] UnknownTicker id=102
returned 0
--- killTrade(100) ---
[event] TradeKilled id=100
Final trade book: {100=KILLED}
```

## What's different from the design doc

The design doc sketched `setOrder(orderMap)` and `setCatalogHas(fn)` as
host-supplied inputs the script can read back. v1 stores these on the
SDK instance but doesn't yet surface them to the running script —
that's the **host-call** layer (Shape B), not yet built. This example
takes the same data via explicit function arguments instead, which is
what works today.
