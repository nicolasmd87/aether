package aether.sandbox;

import java.lang.foreign.*;
import java.lang.invoke.MethodHandle;
import java.nio.charset.StandardCharsets;
import java.util.LinkedHashMap;
import java.util.Map;

/**
 * Reads/writes the Aether shared map via POSIX shared memory (Panama FFI).
 * Format: frozen_count(4 bytes) + key\0value\0...key\0value\0\0
 */
public class AetherMap {
    private final Map<String, String> inputs = new LinkedHashMap<>();
    private final Map<String, String> outputs = new LinkedHashMap<>();
    private final String shmName;
    private int frozenCount;

    private AetherMap(String shmName) {
        this.shmName = shmName;
    }

    /** Load map from shared memory */
    public static AetherMap fromSharedMemory(String shmName) {
        AetherMap map = new AetherMap(shmName);
        map.readFromShm();
        return map;
    }

    /** Get an input value */
    public String get(String key) {
        return inputs.get(key);
    }

    /** Put an output value (cannot overwrite inputs) */
    public void put(String key, String value) {
        if (inputs.containsKey(key)) return;  // frozen input
        outputs.put(key, value);
    }

    /** Write outputs back to shared memory */
    public void writeBack() {
        writeToShm();
    }

    private void readFromShm() {
        try {
            Linker linker = Linker.nativeLinker();
            SymbolLookup lookup = linker.defaultLookup();

            MethodHandle shm_open = linker.downcallHandle(
                lookup.find("shm_open").orElseThrow(),
                FunctionDescriptor.of(ValueLayout.JAVA_INT,
                    ValueLayout.ADDRESS, ValueLayout.JAVA_INT, ValueLayout.JAVA_INT));

            MethodHandle read_fn = linker.downcallHandle(
                lookup.find("read").orElseThrow(),
                FunctionDescriptor.of(ValueLayout.JAVA_LONG,
                    ValueLayout.JAVA_INT, ValueLayout.ADDRESS, ValueLayout.JAVA_LONG));

            MethodHandle close_fn = linker.downcallHandle(
                lookup.find("close").orElseThrow(),
                FunctionDescriptor.of(ValueLayout.JAVA_INT, ValueLayout.JAVA_INT));

            try (Arena arena = Arena.ofConfined()) {
                MemorySegment nameSeg = arena.allocateFrom(shmName, StandardCharsets.UTF_8);
                int fd = (int) shm_open.invoke(nameSeg, 0 /* O_RDONLY */, 0);
                if (fd < 0) return;

                MemorySegment buf = arena.allocate(65536);
                long bytesRead = (long) read_fn.invoke(fd, buf, 65536L);
                close_fn.invoke(fd);

                if (bytesRead <= 4) return;

                // Parse frozen_count
                frozenCount = buf.get(ValueLayout.JAVA_BYTE, 0) & 0xff;
                frozenCount |= (buf.get(ValueLayout.JAVA_BYTE, 1) & 0xff) << 8;
                frozenCount |= (buf.get(ValueLayout.JAVA_BYTE, 2) & 0xff) << 16;
                frozenCount |= (buf.get(ValueLayout.JAVA_BYTE, 3) & 0xff) << 24;

                // Parse key\0value\0 pairs
                int pos = 4;
                while (pos < bytesRead - 1) {
                    byte b = buf.get(ValueLayout.JAVA_BYTE, pos);
                    if (b == 0) break;  // double-null terminator

                    // Read key
                    int keyStart = pos;
                    while (pos < bytesRead && buf.get(ValueLayout.JAVA_BYTE, pos) != 0) pos++;
                    String key = readString(buf, keyStart, pos - keyStart);
                    pos++; // skip null

                    // Read value
                    int valStart = pos;
                    while (pos < bytesRead && buf.get(ValueLayout.JAVA_BYTE, pos) != 0) pos++;
                    String value = readString(buf, valStart, pos - valStart);
                    pos++; // skip null

                    inputs.put(key, value);
                }
            }
        } catch (Throwable e) {
            System.err.println("[aether-map-java] read error: " + e.getMessage());
        }
    }

    private void writeToShm() {
        if (outputs.isEmpty()) return;
        try {
            Linker linker = Linker.nativeLinker();
            SymbolLookup lookup = linker.defaultLookup();

            MethodHandle shm_open = linker.downcallHandle(
                lookup.find("shm_open").orElseThrow(),
                FunctionDescriptor.of(ValueLayout.JAVA_INT,
                    ValueLayout.ADDRESS, ValueLayout.JAVA_INT, ValueLayout.JAVA_INT));

            MethodHandle ftruncate = linker.downcallHandle(
                lookup.find("ftruncate").orElseThrow(),
                FunctionDescriptor.of(ValueLayout.JAVA_INT,
                    ValueLayout.JAVA_INT, ValueLayout.JAVA_LONG));

            MethodHandle mmap = linker.downcallHandle(
                lookup.find("mmap").orElseThrow(),
                FunctionDescriptor.of(ValueLayout.ADDRESS,
                    ValueLayout.ADDRESS, ValueLayout.JAVA_LONG, ValueLayout.JAVA_INT,
                    ValueLayout.JAVA_INT, ValueLayout.JAVA_INT, ValueLayout.JAVA_LONG));

            MethodHandle munmap = linker.downcallHandle(
                lookup.find("munmap").orElseThrow(),
                FunctionDescriptor.of(ValueLayout.JAVA_INT,
                    ValueLayout.ADDRESS, ValueLayout.JAVA_LONG));

            MethodHandle close_fn = linker.downcallHandle(
                lookup.find("close").orElseThrow(),
                FunctionDescriptor.of(ValueLayout.JAVA_INT, ValueLayout.JAVA_INT));

            try (Arena arena = Arena.ofConfined()) {
                // Build buffer: frozen_count + inputs + outputs
                byte[] data = new byte[65536];
                int pos = 0;
                data[pos++] = (byte)(frozenCount & 0xff);
                data[pos++] = (byte)((frozenCount >> 8) & 0xff);
                data[pos++] = (byte)((frozenCount >> 16) & 0xff);
                data[pos++] = (byte)((frozenCount >> 24) & 0xff);

                // Write inputs (frozen)
                for (var e : inputs.entrySet()) {
                    byte[] kb = e.getKey().getBytes(StandardCharsets.UTF_8);
                    byte[] vb = e.getValue().getBytes(StandardCharsets.UTF_8);
                    System.arraycopy(kb, 0, data, pos, kb.length); pos += kb.length;
                    data[pos++] = 0;
                    System.arraycopy(vb, 0, data, pos, vb.length); pos += vb.length;
                    data[pos++] = 0;
                }
                // Write outputs
                for (var e : outputs.entrySet()) {
                    byte[] kb = e.getKey().getBytes(StandardCharsets.UTF_8);
                    byte[] vb = e.getValue().getBytes(StandardCharsets.UTF_8);
                    System.arraycopy(kb, 0, data, pos, kb.length); pos += kb.length;
                    data[pos++] = 0;
                    System.arraycopy(vb, 0, data, pos, vb.length); pos += vb.length;
                    data[pos++] = 0;
                }
                data[pos++] = 0; // double-null

                // Write to shm (O_RDWR=2)
                MemorySegment nameSeg = arena.allocateFrom(shmName, StandardCharsets.UTF_8);
                int fd = (int) shm_open.invoke(nameSeg, 2 /* O_RDWR */, 0600);
                if (fd < 0) return;

                ftruncate.invoke(fd, (long) pos);
                MemorySegment mem = (MemorySegment) mmap.invoke(
                    MemorySegment.NULL, (long) pos, 3 /* PROT_READ|PROT_WRITE */,
                    1 /* MAP_SHARED */, fd, 0L);
                mem = mem.reinterpret(pos);
                MemorySegment.copy(MemorySegment.ofArray(data), 0, mem, 0, pos);
                munmap.invoke(mem, (long) pos);
                close_fn.invoke(fd);
            }
        } catch (Throwable e) {
            System.err.println("[aether-map-java] write error: " + e.getMessage());
        }
    }

    private static String readString(MemorySegment seg, int offset, int length) {
        byte[] bytes = new byte[length];
        for (int i = 0; i < length; i++) {
            bytes[i] = seg.get(ValueLayout.JAVA_BYTE, offset + i);
        }
        return new String(bytes, StandardCharsets.UTF_8);
    }
}
