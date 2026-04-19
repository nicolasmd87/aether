# contrib.host.java — Java Sandbox Agent (Panama FFI)

## Prerequisites

```bash
# Debian/Ubuntu — OpenJDK 22+ required for Panama FFI (stable)
# Amazon Corretto 24:
wget https://corretto.aws/downloads/latest/amazon-corretto-24-x64-linux-jdk.deb
sudo dpkg -i amazon-corretto-24-x64-linux-jdk.deb

# Verify
java -version    # must be 22+
javac -version
```

## Build the agent jar

```bash
./contrib/host/java/build.sh
# Creates: build/aether-sandbox.jar
```

## Usage

Java runs as a separate process (JVM), not embedded. Use
`spawn_sandboxed` or run directly with the agent:

```bash
java --enable-native-access=ALL-UNNAMED \
     -javaagent:build/aether-sandbox.jar \
     -cp build/aether-sandbox.jar:your-app.jar \
     com.example.Main
```

### `grant_jvm_runtime()` helper

JVM startup needs ~30 grants for the linker, trust stores, locale, and
`JAVA_*` env vars before any application code runs. Bundling them once
keeps spawn scripts readable:

```aether
import std.list
import contrib.host.java

main() {
    worker = sandbox("my-java-app") {
        java.grant_jvm_runtime()         // JVM bring-up (29 grants)
        grant_fs_read("/app/data/*")      // application-specific
        grant_tcp("api.example.com")
    }
    spawn_sandboxed(worker, "java",
        "--enable-native-access=ALL-UNNAMED",
        "-javaagent:build/aether-sandbox.jar",
        "-jar", "my-app.jar")
    list.free(worker)
}
```

The grant set is conservative — it permits reads the JVM performs
during class loading and TLS init, and nothing more. Source paths were
captured empirically via `strace java -version` on Corretto 24 (Debian)
and Temurin 21 (Ubuntu).

## Notes

- Requires `--enable-native-access=ALL-UNNAMED` for Panama FFI
- The agent reads grants from shared memory (`AETHER_SANDBOX_SHM`)
  or a grant file (`AETHER_SANDBOX_GRANTS`)
- LD_PRELOAD enforces file/network/exec at the libc level
- The agent provides `AetherSandboxHooks.checkEnv()` for env vars
  (Java's `System.getenv()` is cached and immutable)
- Shared map via `AetherMap.fromSharedMemory()` using Panama FFI
