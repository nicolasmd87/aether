package aether.sandbox;

import java.io.*;
import java.net.*;

/**
 * Installs sandbox hooks by replacing System.getenv behavior and
 * setting a custom SecurityManager-like check via System properties.
 *
 * Since Java 24 removed SecurityManager, we use a different approach:
 * a global checker that application code calls, and we scrub System
 * env/properties at install time. For file/network, the LD_PRELOAD
 * layer handles interception at the libc level.
 */
public class AetherSandboxHooks {
    private static AetherGrantChecker checker;

    public static void install(AetherGrantChecker c) {
        checker = c;
        scrubEnvironment();
        System.err.println("[aether-sandbox-java] active with " +
            (c != null ? "grants" : "no grants"));
    }

    /**
     * Scrub environment: Java caches env vars in an unmodifiable map.
     * We can't modify it directly. But with LD_PRELOAD intercepting
     * getenv(), new lookups via ProcessBuilder or Runtime.exec will
     * see the filtered values. For System.getenv(), we print a
     * warning — the cached map is read-only post-Java 9.
     *
     * The real enforcement for env vars is at the libc level via
     * LD_PRELOAD. Java's System.getenv() returns cached values from
     * JVM startup, but any subprocess or native code goes through libc.
     */
    private static void scrubEnvironment() {
        // Nothing to scrub in Java — System.getenv() is immutable.
        // LD_PRELOAD handles the actual enforcement.
    }

    /** Check if an operation is allowed. Called from instrumented code. */
    public static boolean allowed(String category, String resource) {
        if (checker == null) return true;
        return checker.check(category, resource);
    }

    /** Convenience: check file read */
    public static void checkFileRead(String path) throws SecurityException {
        if (checker != null && !checker.check("fs_read", path)) {
            throw new SecurityException("[aether-sandbox] DENY fs_read " + path);
        }
    }

    /** Convenience: check file write */
    public static void checkFileWrite(String path) throws SecurityException {
        if (checker != null && !checker.check("fs_write", path)) {
            throw new SecurityException("[aether-sandbox] DENY fs_write " + path);
        }
    }

    /** Convenience: check network connect */
    public static void checkConnect(String host) throws SecurityException {
        if (checker != null && !checker.check("tcp", host)) {
            throw new SecurityException("[aether-sandbox] DENY tcp " + host);
        }
    }

    /** Convenience: check exec */
    public static void checkExec(String cmd) throws SecurityException {
        if (checker != null && !checker.check("exec", cmd)) {
            throw new SecurityException("[aether-sandbox] DENY exec " + cmd);
        }
    }

    /** Convenience: check env — returns null if denied */
    public static String checkEnv(String name) {
        if (checker != null && !checker.check("env", name)) {
            return null;
        }
        return System.getenv(name);
    }
}
