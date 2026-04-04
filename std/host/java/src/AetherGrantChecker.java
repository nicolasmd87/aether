package aether.sandbox;

import java.io.*;
import java.lang.foreign.*;
import java.lang.invoke.MethodHandle;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

/**
 * Reads Aether sandbox grants and checks permissions.
 * Uses Panama FFI to read shared memory (no JNI).
 */
public class AetherGrantChecker {
    private final List<String[]> grants; // [category, pattern] pairs

    private AetherGrantChecker(List<String[]> grants) {
        this.grants = grants;
    }

    public boolean check(String category, String resource) {
        for (String[] grant : grants) {
            String cat = grant[0];
            String pat = grant[1];
            // Wildcard: *:*
            if ("*".equals(cat) && "*".equals(pat)) return true;
            if (cat.equals(category)) {
                if (patternMatch(pat, resource)) return true;
            }
        }
        return false;
    }

    private boolean patternMatch(String pat, String resource) {
        // Wildcard
        if ("*".equals(pat)) return true;
        // Prefix: /etc/*
        if (pat.length() > 1 && pat.endsWith("*")) {
            return resource.startsWith(pat.substring(0, pat.length() - 1));
        }
        // Suffix: *.example.com
        if (pat.length() > 1 && pat.startsWith("*")) {
            return resource.endsWith(pat.substring(1));
        }
        // Exact
        return pat.equals(resource);
    }

    /** Read grants from a file (category:pattern per line) */
    public static AetherGrantChecker fromFile(String path) {
        List<String[]> grants = new ArrayList<>();
        try (BufferedReader r = new BufferedReader(new FileReader(path))) {
            String line;
            while ((line = r.readLine()) != null) {
                line = line.trim();
                if (line.isEmpty() || line.startsWith("#")) continue;
                int colon = line.indexOf(':');
                if (colon > 0) {
                    grants.add(new String[]{
                        line.substring(0, colon),
                        line.substring(colon + 1)
                    });
                }
            }
        } catch (IOException e) {
            System.err.println("[aether-sandbox] cannot read grant file: " + e.getMessage());
        }
        return new AetherGrantChecker(grants);
    }

    /** Read grants from POSIX shared memory via Panama FFI */
    public static AetherGrantChecker fromSharedMemory(String shmName) {
        List<String[]> grants = new ArrayList<>();
        try {
            Linker linker = Linker.nativeLinker();
            SymbolLookup lookup = linker.defaultLookup();

            // shm_open(name, O_RDONLY, 0) → fd
            MethodHandle shm_open = linker.downcallHandle(
                lookup.find("shm_open").orElseThrow(),
                FunctionDescriptor.of(ValueLayout.JAVA_INT,
                    ValueLayout.ADDRESS, ValueLayout.JAVA_INT, ValueLayout.JAVA_INT)
            );

            // fstat(fd, &stat) to get size — use read() instead for simplicity
            // read(fd, buf, count) → bytes_read
            MethodHandle read = linker.downcallHandle(
                lookup.find("read").orElseThrow(),
                FunctionDescriptor.of(ValueLayout.JAVA_LONG,
                    ValueLayout.JAVA_INT, ValueLayout.ADDRESS, ValueLayout.JAVA_LONG)
            );

            MethodHandle close = linker.downcallHandle(
                lookup.find("close").orElseThrow(),
                FunctionDescriptor.of(ValueLayout.JAVA_INT, ValueLayout.JAVA_INT)
            );

            try (Arena arena = Arena.ofConfined()) {
                MemorySegment nameSegment = arena.allocateFrom(shmName, StandardCharsets.UTF_8);
                int fd = (int) shm_open.invoke(nameSegment, 0 /* O_RDONLY */, 0);
                if (fd < 0) {
                    System.err.println("[aether-sandbox] cannot open shared memory: " + shmName);
                    return new AetherGrantChecker(grants);
                }

                // Read up to 8KB of grants
                MemorySegment buf = arena.allocate(8192);
                long bytesRead = (long) read.invoke(fd, buf, 8192L);
                close.invoke(fd);

                if (bytesRead > 0) {
                    String data = buf.getString(0, StandardCharsets.UTF_8);
                    for (String line : data.split("\n")) {
                        line = line.trim();
                        if (line.isEmpty() || line.startsWith("#")) continue;
                        int colon = line.indexOf(':');
                        if (colon > 0) {
                            grants.add(new String[]{
                                line.substring(0, colon),
                                line.substring(colon + 1)
                            });
                        }
                    }
                }
            }
        } catch (Throwable e) {
            System.err.println("[aether-sandbox] FFI error reading shared memory: " + e.getMessage());
        }
        return new AetherGrantChecker(grants);
    }
}
