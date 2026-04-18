/*
 * End-to-end check for the generated Java SDK.
 *
 * Loads the SDK class that `ae build --namespace` emitted, exercises
 * every input setter, every event handler, and every script function,
 * asserts the round-trip behavior. Errors are printed to stderr and
 * exit code is non-zero so the shell driver can detect failure.
 */
import com.example.calc.Calc;
import java.util.ArrayList;
import java.util.List;

public class Check {
    static void fail(String msg) {
        System.err.println("FAIL: " + msg);
        System.exit(1);
    }

    public static void main(String[] args) throws Exception {
        try (Calc ns = new Calc(args[0])) {

            // --- discovery ---
            Calc.Manifest m = ns.describe();
            if (!"calc".equals(m.namespaceName))
                fail("namespace=" + m.namespaceName);
            if (m.inputs.size() != 2 || !"limit".equals(m.inputs.get(0)[0])
                                     || !"name".equals(m.inputs.get(1)[0]))
                fail("inputs=" + m.inputs);
            if (m.events.size() != 2 || !"Computed".equals(m.events.get(0)[0])
                                     || !"Overflow".equals(m.events.get(1)[0]))
                fail("events=" + m.events);
            if (!"com.example.calc".equals(m.javaPackage))
                fail("javaPackage=" + m.javaPackage);
            if (!"Calc".equals(m.javaClass))
                fail("javaClass=" + m.javaClass);

            // --- setters (v1: stored only) ---
            ns.setLimit(100);
            ns.setName("paul");
            if (ns.limit != 100 || !"paul".equals(ns.name))
                fail("setters didn't stash");

            // --- event handlers ---
            List<Long> computedIds = new ArrayList<>();
            List<Long> overflowIds = new ArrayList<>();
            ns.onComputed(id -> computedIds.add(id));
            ns.onOverflow(id -> overflowIds.add(id));

            // --- function calls ---
            int r = ns.doubleIt(7);
            if (r != 14) fail("doubleIt(7)=" + r);
            if (computedIds.size() != 1 || computedIds.get(0) != 7L)
                fail("computedIds after doubleIt: " + computedIds);

            r = ns.doubleIt(2_000_000);
            if (r != -1) fail("doubleIt(2_000_000)=" + r);
            if (overflowIds.size() != 1 || overflowIds.get(0) != 2_000_000L)
                fail("overflowIds: " + overflowIds);

            String s = ns.label("hello", 42);
            if (!"hello".equals(s)) fail("label('hello',42)=" + s);
            if (computedIds.size() != 2 || computedIds.get(1) != 42L)
                fail("label didn't fire Computed: " + computedIds);

            r = ns.isPositive(5);
            if (r != 1) fail("isPositive(5)=" + r);
            r = ns.isPositive(-5);
            if (r != 0) fail("isPositive(-5)=" + r);

            System.out.println("OK: Java SDK round-trip — discovery, setters, events, functions");
        }
    }
}
