# std.host.java — Java Sandbox Agent (Panama FFI)

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
./std/host/java/build.sh
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

## Notes

- Requires `--enable-native-access=ALL-UNNAMED` for Panama FFI
- The agent reads grants from shared memory (`AETHER_SANDBOX_SHM`)
  or a grant file (`AETHER_SANDBOX_GRANTS`)
- LD_PRELOAD enforces file/network/exec at the libc level
- The agent provides `AetherSandboxHooks.checkEnv()` for env vars
  (Java's `System.getenv()` is cached and immutable)
- Shared map via `AetherMap.fromSharedMemory()` using Panama FFI
