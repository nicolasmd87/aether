package aether.sandbox;

import java.lang.instrument.Instrumentation;

/**
 * Java agent entry point. Installed via -javaagent:aether-sandbox.jar.
 * Reads grants from Aether's shared memory and installs runtime checks.
 */
public class AetherSandboxAgent {
    static AetherGrantChecker checker;

    public static void premain(String args, Instrumentation inst) {
        String shmName = System.getenv("AETHER_SANDBOX_SHM");
        if (shmName != null) {
            checker = AetherGrantChecker.fromSharedMemory(shmName);
        } else {
            // Fallback: read from grant file
            String grantFile = System.getenv("AETHER_SANDBOX_GRANTS");
            if (grantFile != null) {
                checker = AetherGrantChecker.fromFile(grantFile);
            }
        }
        if (checker != null) {
            AetherSandboxHooks.install(checker);
        }

        // Map support: load shared map if AETHER_MAP_SHM is set
        // (the map is accessed via AetherMap in user code)
    }
}
