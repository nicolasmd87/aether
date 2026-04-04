package aether.sandbox;

import java.io.*;
import java.net.*;

/**
 * Test program — tries to escape the sandbox.
 * Uses normal Java APIs. The agent + LD_PRELOAD enforce grants.
 */
public class SandboxTest {
    public static void main(String[] args) {
        System.out.println("=== Java Sandbox Test ===\n");

        // Env vars — checked via hooks (System.getenv is cached, so
        // we use the hook's checkEnv which consults the grant checker)
        testEnv("HOME");
        testEnv("USER");
        testEnv("AWS_SECRET_KEY");
        testEnv("TERM");

        // File access — LD_PRELOAD intercepts at libc level
        testRead("/etc/hostname");
        testRead("/etc/shadow");
        testRead("/etc/passwd");

        // Network — LD_PRELOAD intercepts connect()
        testHttp("http://example.com");
        testHttp("http://httpbin.org/get");

        // Shared map test
        String mapShm = System.getenv("AETHER_MAP_SHM");
        if (mapShm != null) {
            System.out.println("\n--- Shared Map ---");
            AetherMap map = AetherMap.fromSharedMemory(mapShm);
            String user = map.get("user");
            String threshold = map.get("threshold");
            System.out.println("  map.get(user) = " + (user != null ? user : "null"));
            System.out.println("  map.get(threshold) = " + (threshold != null ? threshold : "null"));
            System.out.println("  map.get(secret) = " + (map.get("secret") != null ? map.get("secret") : "null"));

            map.put("result", "processed " + user);
            map.put("status", "ok");
            map.put("user", "TAMPERED");  // should be rejected (frozen input)
            System.out.println("  map.put(result, status, user=TAMPERED)");
            map.writeBack();
        }

        System.out.println("\ndone");
    }

    static void testEnv(String name) {
        String val = AetherSandboxHooks.checkEnv(name);
        if (val != null) {
            System.out.println("  [OK]      env " + name + " = " + val);
        } else {
            System.out.println("  [BLOCKED] env " + name);
        }
    }

    static void testRead(String path) {
        try {
            BufferedReader r = new BufferedReader(new FileReader(path));
            String line = r.readLine();
            r.close();
            System.out.println("  [OK]      read " + path + " = " + line.trim());
        } catch (Exception e) {
            System.out.println("  [BLOCKED] read " + path);
        }
    }

    static void testHttp(String urlStr) {
        try {
            URL url = URI.create(urlStr).toURL();
            HttpURLConnection conn = (HttpURLConnection) url.openConnection();
            conn.setConnectTimeout(5000);
            conn.setReadTimeout(5000);
            int code = conn.getResponseCode();
            conn.disconnect();
            System.out.println("  [OK]      http " + urlStr + " = " + code);
        } catch (Exception e) {
            System.out.println("  [BLOCKED] http " + urlStr);
        }
    }
}
