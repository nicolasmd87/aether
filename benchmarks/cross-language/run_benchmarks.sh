#!/usr/bin/env bash
# Main benchmark runner - ONLY REAL RESULTS, NO SYNTHETIC DATA
# If a benchmark can't run, it's simply skipped

# Don't exit on errors - we want to skip failures gracefully
set +e
cd "$(dirname "$0")"

# Source cargo environment if it exists
[ -f "$HOME/.cargo/env" ] && source "$HOME/.cargo/env"

# Add common tool paths
export PATH="$HOME/.cargo/bin:$PATH"

# Auto-detect Java 17+ for Scala (portable across macOS/Linux)
detect_java() {
    # Check if JAVA_HOME is already set and valid
    if [ -n "$JAVA_HOME" ] && [ -x "$JAVA_HOME/bin/java" ]; then
        local version=$("$JAVA_HOME/bin/java" -version 2>&1 | awk -F '"' '/version/ {print $2}' | cut -d. -f1)
        if [ "$version" -ge 17 ] 2>/dev/null; then
            export PATH="$JAVA_HOME/bin:$PATH"
            return 0
        fi
    fi

    # Try to find Java 17+ in common locations
    for java_path in \
        "/opt/homebrew/opt/openjdk@17" \
        "/opt/homebrew/opt/openjdk@21" \
        "/opt/homebrew/opt/openjdk" \
        "/usr/lib/jvm/java-17-openjdk" \
        "/usr/lib/jvm/java-21-openjdk" \
        "/usr/lib/jvm/java-17" \
        "/usr/lib/jvm/java-21" \
        "/usr/lib/jvm/default-java" \
        "/Library/Java/JavaVirtualMachines/openjdk-17.jdk/Contents/Home" \
        "/Library/Java/JavaVirtualMachines/openjdk-21.jdk/Contents/Home"; do

        if [ -d "$java_path" ] && [ -x "$java_path/bin/java" ]; then
            local version=$("$java_path/bin/java" -version 2>&1 | awk -F '"' '/version/ {print $2}' | cut -d. -f1)
            if [ "$version" -ge 17 ] 2>/dev/null; then
                export JAVA_HOME="$java_path"
                export PATH="$JAVA_HOME/bin:$PATH"
                return 0
            fi
        fi
    done

    # Last resort: check if system java is 17+
    if command -v java >/dev/null 2>&1; then
        local version=$(java -version 2>&1 | awk -F '"' '/version/ {print $2}' | cut -d. -f1)
        if [ "$version" -ge 17 ] 2>/dev/null; then
            return 0
        fi
    fi

    return 1
}

# Interactive dependency checking and installation
check_and_install() {
    local tool_name=$1
    local check_cmd=$2
    local macos_install=$3
    local linux_install=$4
    local description=$5

    if ! command -v "$check_cmd" >/dev/null 2>&1; then
        echo "=================================================================="
        echo "  $tool_name: Not found"
        echo "=================================================================="
        echo ""
        echo "$description"
        echo ""
        echo "Installation command:"
        echo "  macOS:  $macos_install"
        echo "  Linux:  $linux_install"
        echo ""
        read -p "Install now? (y/n): " response

        if [[ "$response" =~ ^[Yy]$ ]]; then
            echo "Installing $tool_name..."
            if [[ "$OSTYPE" == "darwin"* ]]; then
                eval "$macos_install"
            elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
                eval "$linux_install"
            fi

            if command -v "$check_cmd" >/dev/null 2>&1; then
                echo "$tool_name installed successfully."
                return 0
            else
                echo "Installation failed. Skipping $tool_name benchmark."
                return 1
            fi
        else
            echo "Skipping $tool_name benchmark."
            return 1
        fi
    fi
    return 0
}

echo "Checking dependencies..."
echo ""

# Check Java first (already has detect function)
if ! detect_java; then
    echo "=================================================================="
    echo "  Java 17+: Not found"
    echo "=================================================================="
    echo ""
    echo "Required for Java and Scala benchmarks."
    echo ""
    echo "Installation command:"
    echo "  macOS:  brew install openjdk@17"
    echo "  Linux:  sudo apt-get install openjdk-17-jdk"
    echo ""
    read -p "Install now? (y/n): " response
    if [[ "$response" =~ ^[Yy]$ ]]; then
        echo "Installing Java 17..."
        if [[ "$OSTYPE" == "darwin"* ]]; then
            brew install openjdk@17
        elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
            sudo apt-get install openjdk-17-jdk
        fi
        if detect_java; then
            echo "Java 17+ installed successfully."
        else
            echo "Installation failed. Skipping Java and Scala benchmarks."
        fi
    else
        echo "Skipping Java and Scala benchmarks."
    fi
fi

# Check other languages
check_and_install "Go" "go" "brew install go" "sudo apt-get install golang" "Required for Go benchmark"
check_and_install "Rust" "cargo" "brew install rust" "curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh" "Required for Rust benchmark"
check_and_install "Zig" "zig" "brew install zig" "sudo snap install zig --classic --beta" "Required for Zig benchmark"
check_and_install "Erlang" "erl" "brew install erlang" "sudo apt-get install erlang" "Required for Erlang benchmark"
check_and_install "Elixir" "elixir" "brew install elixir" "sudo apt-get install elixir" "Required for Elixir benchmark"
check_and_install "Pony" "ponyc" "brew install ponyc" "sudo add-apt-repository ppa:ponylang/ponylang && sudo apt-get update && sudo apt-get install ponyc" "Required for Pony benchmark"

echo ""
echo "Starting benchmarks..."
echo ""

# Load benchmark configuration from JSON
CONFIG_FILE="benchmark_config.json"
if [ -f "$CONFIG_FILE" ]; then
    # Parse JSON using python (available on macOS/Linux)
    BENCHMARK_MESSAGES=$(python3 -c "import json; print(json.load(open('$CONFIG_FILE'))['messages'])" 2>/dev/null || echo "10000000")
    BENCHMARK_TIMEOUT=$(python3 -c "import json; print(json.load(open('$CONFIG_FILE'))['timeout_seconds'])" 2>/dev/null || echo "60")
else
    BENCHMARK_MESSAGES=10000000
    BENCHMARK_TIMEOUT=60
fi

# Apply configuration to all source files dynamically
apply_config() {
    local file=$1
    local pattern=$2
    local replacement=$3
    if [[ "$OSTYPE" == "darwin"* ]]; then
        sed -i '' "s/$pattern/$replacement/g" "$file"
    else
        sed -i "s/$pattern/$replacement/g" "$file"
    fi
}

echo "============================================"
echo "  Running Cross-Language Benchmarks"
echo "  Messages: $BENCHMARK_MESSAGES"
echo "============================================"
echo ""

# Update all language implementations with current config
if [ "$BENCHMARK_MESSAGES" != "10000000" ]; then
    echo "Applying custom message count: $BENCHMARK_MESSAGES"
    apply_config "aether/ping_pong.ae" "total_messages = [0-9]*" "total_messages = $BENCHMARK_MESSAGES"
    apply_config "c/ping_pong.c" "#define MESSAGES [0-9]*" "#define MESSAGES $BENCHMARK_MESSAGES"
    apply_config "cpp/ping_pong.cpp" "constexpr size_t MESSAGES = [0-9]*" "constexpr size_t MESSAGES = $BENCHMARK_MESSAGES"
    apply_config "go/ping_pong.go" "const messages = [0-9]*" "const messages = $BENCHMARK_MESSAGES"
    apply_config "rust/src/main.rs" "const MESSAGES: usize = [0-9_]*" "const MESSAGES: usize = $BENCHMARK_MESSAGES"
    apply_config "java/PingPong.java" "private static final int MESSAGES = [0-9]*" "private static final int MESSAGES = $BENCHMARK_MESSAGES"
    apply_config "zig/ping_pong.zig" "const messages = [0-9]*" "const messages = $BENCHMARK_MESSAGES"
    apply_config "elixir/ping_pong.exs" "@messages [0-9]*" "@messages $BENCHMARK_MESSAGES"
    apply_config "erlang/ping_pong.erl" "-define(MESSAGES, [0-9]*)." "-define(MESSAGES, $BENCHMARK_MESSAGES)."
    apply_config "pony/ping_pong.pony" "if _count < [0-9_]* then" "if _count < $BENCHMARK_MESSAGES then"
    apply_config "pony/ping_pong.pony" "let ns_per_msg = elapsed_ns.f64.. / [0-9_]*.0" "let ns_per_msg = elapsed_ns.f64() / ${BENCHMARK_MESSAGES}.0"
    apply_config "scala/ping_pong.scala" "val maxCount = [0-9]*L" "val maxCount = ${BENCHMARK_MESSAGES}L"
    apply_config "scala/ping_pong.scala" 'println."Messages: [0-9]*' "println(\"Messages: $BENCHMARK_MESSAGES"
    apply_config "scala/ping_pong.scala" "val cyclesPerMsg = .elapsed.toDouble / [0-9_]*.0. . 3.0" "val cyclesPerMsg = (elapsed.toDouble / ${BENCHMARK_MESSAGES}.0) * 3.0"
    apply_config "scala/ping_pong.scala" "val throughput = [0-9_]*.0 / elapsedSec / 1e6" "val throughput = ${BENCHMARK_MESSAGES}.0 / elapsedSec / 1e6"
    echo ""
fi

RESULTS_FILE="visualize/results_ping_pong.json"
FIRST_RESULT=true

# Create results file header
cat > "$RESULTS_FILE" <<EOF
{
  "timestamp": "$(date -u +"%Y-%m-%dT%H:%M:%SZ")",
  "pattern": "ping_pong",
  "hardware": {
    "cpu": "$(sysctl -n machdep.cpu.brand_string 2>/dev/null || lscpu | grep 'Model name' | cut -d: -f2 | xargs || echo 'Unknown')",
    "cores": $(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo '8'),
    "os": "$(uname -s)"
  },
  "benchmarks": {
EOF

# Helper to add comma before result (except first one)
add_result() {
    if [ "$FIRST_RESULT" = false ]; then
        echo "," >> "$RESULTS_FILE"
    fi
    FIRST_RESULT=false
}

# 0. Aether
echo "Building Aether ping-pong..."
if [ -f "aether/ping_pong.ae" ]; then
    if (cd aether && make clean &>/dev/null && make ping_pong &>/dev/null); then
        AETHER_OUTPUT=$(cd aether && /usr/bin/time -l ./ping_pong 2>&1 || time -v ./ping_pong 2>&1)
        AETHER_CYCLES=$(echo "$AETHER_OUTPUT" | grep "Cycles/msg" | awk '{print $2}')
        AETHER_THROUGHPUT=$(echo "$AETHER_OUTPUT" | grep "Throughput" | awk '{print $2}')
        AETHER_MSG_PER_SEC=$(echo "$AETHER_THROUGHPUT * 1000000" | bc 2>/dev/null | awk '{printf "%.0f", $0}')

        AETHER_MEMORY=$(echo "$AETHER_OUTPUT" | grep -E "maximum resident set size|Maximum resident" | awk '{print $1}' | head -1)
        AETHER_MEMORY_MB=$(echo "scale=2; $AETHER_MEMORY / 1024 / 1024" | bc 2>/dev/null | awk '{printf "%.2f", ($0 == "" ? 0 : $0)}')

        add_result
        cat >> "$RESULTS_FILE" <<EOF
    "Aether": {
      "runtime": "Native (Aether runtime)",
      "msg_per_sec": $AETHER_MSG_PER_SEC,
      "cycles_per_msg": $AETHER_CYCLES,
      "memory_mb": $AETHER_MEMORY_MB,
      "notes": "Lock-free SPSC queues, batched sends"
    }
EOF
        echo "[OK] Aether: ${AETHER_THROUGHPUT}M msg/sec (${AETHER_MEMORY_MB}MB)"
    else
        echo "[SKIP] Aether: Build failed (skipped)"
    fi
else
    echo "[SKIP] Aether: Not found (skipped)"
fi

# 1. C (pthread) - Baseline
echo "Building C ping-pong..."
if (cd c && make clean &>/dev/null && make ping_pong &>/dev/null); then
    C_OUTPUT=$(cd c && /usr/bin/time -l ./ping_pong 2>&1 || time -v ./ping_pong 2>&1)
    C_CYCLES=$(echo "$C_OUTPUT" | grep "Cycles/msg" | awk '{print $2}')
    C_THROUGHPUT=$(echo "$C_OUTPUT" | grep "Throughput" | awk '{print $2}')
    C_MSG_PER_SEC=$(echo "$C_THROUGHPUT * 1000000" | bc 2>/dev/null | awk '{printf "%.0f", $0}')

    # Memory (macOS vs Linux)
    C_MEMORY=$(echo "$C_OUTPUT" | grep -E "maximum resident set size|Maximum resident" | awk '{print $1}' | head -1)
    C_MEMORY_MB=$(echo "scale=2; $C_MEMORY / 1024 / 1024" | bc 2>/dev/null | awk '{printf "%.2f", ($0 == "" ? 0 : $0)}')

    add_result
    cat >> "$RESULTS_FILE" <<EOF
    "C (pthread)": {
      "runtime": "Native (pthread)",
      "msg_per_sec": $C_MSG_PER_SEC,
      "cycles_per_msg": $C_CYCLES,
      "memory_mb": $C_MEMORY_MB,
      "notes": "pthread mutex + condvar (baseline)"
    }
EOF
    echo "[OK] C (pthread): ${C_THROUGHPUT}M msg/sec (${C_MEMORY_MB}MB)"
else
    echo "[SKIP] C (pthread): Build failed (skipped)"
fi

# 2. C++ (std::mutex + condition_variable)
echo "Building C++ ping-pong..."
if (cd cpp && g++ -O3 -std=c++17 -march=native ping_pong.cpp -o ping_pong -pthread &>/dev/null); then
    CPP_OUTPUT=$(cd cpp && /usr/bin/time -l ./ping_pong 2>&1 || time -v ./ping_pong 2>&1)
    CPP_CYCLES=$(echo "$CPP_OUTPUT" | grep "Cycles/msg" | awk '{print $2}')
    CPP_THROUGHPUT=$(echo "$CPP_OUTPUT" | grep "Throughput" | awk '{print $2}')
    CPP_MSG_PER_SEC=$(echo "$CPP_THROUGHPUT * 1000000" | bc 2>/dev/null | awk '{printf "%.0f", $0}')

    CPP_MEMORY=$(echo "$CPP_OUTPUT" | grep -E "maximum resident set size|Maximum resident" | awk '{print $1}' | head -1)
    CPP_MEMORY_MB=$(echo "scale=2; $CPP_MEMORY / 1024 / 1024" | bc 2>/dev/null | awk '{printf "%.2f", ($0 == "" ? 0 : $0)}')

    add_result
    cat >> "$RESULTS_FILE" <<EOF
    "C++": {
      "runtime": "Native (std::thread)",
      "msg_per_sec": $CPP_MSG_PER_SEC,
      "cycles_per_msg": $CPP_CYCLES,
      "memory_mb": $CPP_MEMORY_MB,
      "notes": "std::mutex + std::condition_variable (fair)"
    }
EOF
    echo "[OK] C++: ${CPP_THROUGHPUT}M msg/sec (${CPP_MEMORY_MB}MB)"
else
    echo "[SKIP] C++: Build failed (skipped)"
fi

# 3. Go
echo "Running Go ping-pong..."
if command -v go &> /dev/null; then
    GO_OUTPUT=$(cd go && /usr/bin/time -l go run ping_pong.go 2>&1 || (go run ping_pong.go 2>&1))
    GO_CYCLES=$(echo "$GO_OUTPUT" | grep "Cycles/msg" | awk '{print $2}')
    GO_THROUGHPUT=$(echo "$GO_OUTPUT" | grep "Throughput" | awk '{print $2}')
    GO_MSG_PER_SEC=$(echo "$GO_THROUGHPUT * 1000000" | bc 2>/dev/null | awk '{printf "%.0f", $0}')

    GO_MEMORY=$(echo "$GO_OUTPUT" | grep -E "maximum resident set size|Maximum resident" | awk '{print $1}' | head -1)
    GO_MEMORY_MB=$(echo "scale=2; $GO_MEMORY / 1024 / 1024" | bc 2>/dev/null | awk '{printf "%.2f", ($0 == "" ? 0 : $0)}')

    add_result
    cat >> "$RESULTS_FILE" <<EOF
    "Go": {
      "runtime": "Go runtime",
      "msg_per_sec": $GO_MSG_PER_SEC,
      "cycles_per_msg": $GO_CYCLES,
      "memory_mb": $GO_MEMORY_MB,
      "notes": "Goroutines with channels"
    }
EOF
    echo "[OK] Go: ${GO_THROUGHPUT}M msg/sec (${GO_MEMORY_MB}MB)"
else
    echo "[SKIP] Go: Not installed (skipped)"
fi

# 4. Rust
if command -v cargo &> /dev/null; then
    echo "Building Rust ping-pong..."
    if (cd rust && cargo build --release --bin ping_pong &>/dev/null); then
        RUST_OUTPUT=$(cd rust && /usr/bin/time -l ./target/release/ping_pong 2>&1 || time -v ./target/release/ping_pong 2>&1)
        RUST_CYCLES=$(echo "$RUST_OUTPUT" | grep "Cycles/msg" | awk '{print $2}')
        RUST_THROUGHPUT=$(echo "$RUST_OUTPUT" | grep "Throughput" | awk '{print $2}')
        RUST_MSG_PER_SEC=$(echo "$RUST_THROUGHPUT * 1000000" | bc 2>/dev/null | awk '{printf "%.0f", $0}')

        RUST_MEMORY=$(echo "$RUST_OUTPUT" | grep -E "maximum resident set size|Maximum resident" | awk '{print $1}' | head -1)
        RUST_MEMORY_MB=$(echo "scale=2; $RUST_MEMORY / 1024 / 1024" | bc 2>/dev/null | awk '{printf "%.2f", ($0 == "" ? 0 : $0)}')

        add_result
        cat >> "$RESULTS_FILE" <<EOF
    "Rust": {
      "runtime": "Native (Tokio async)",
      "msg_per_sec": $RUST_MSG_PER_SEC,
      "cycles_per_msg": $RUST_CYCLES,
      "memory_mb": $RUST_MEMORY_MB,
      "notes": "std::sync::mpsc::sync_channel"
    }
EOF
        echo "[OK] Rust: ${RUST_THROUGHPUT}M msg/sec (${RUST_MEMORY_MB}MB)"
    else
        echo "[SKIP] Rust: Build failed (skipped)"
    fi
else
    echo "[SKIP] Rust: Cargo not installed (skipped)"
fi

# 5. Java
if command -v javac &> /dev/null && command -v java &> /dev/null; then
    echo "Building Java ping-pong..."
    if (cd java && javac PingPong.java &>/dev/null); then
        JAVA_OUTPUT=$(cd java && /usr/bin/time -l java PingPong 2>&1 || time -v java PingPong 2>&1)
        JAVA_CYCLES=$(echo "$JAVA_OUTPUT" | grep "Cycles/msg" | awk '{print $2}')
        JAVA_THROUGHPUT=$(echo "$JAVA_OUTPUT" | grep "Throughput" | awk '{print $2}')
        JAVA_MSG_PER_SEC=$(echo "$JAVA_THROUGHPUT * 1000000" | bc 2>/dev/null | awk '{printf "%.0f", $0}')

        JAVA_MEMORY=$(echo "$JAVA_OUTPUT" | grep -E "maximum resident set size|Maximum resident" | awk '{print $1}' | head -1)
        JAVA_MEMORY_MB=$(echo "scale=2; $JAVA_MEMORY / 1024 / 1024" | bc 2>/dev/null | awk '{printf "%.2f", ($0 == "" ? 0 : $0)}')

        add_result
        cat >> "$RESULTS_FILE" <<EOF
    "Java": {
      "runtime": "JVM",
      "msg_per_sec": $JAVA_MSG_PER_SEC,
      "cycles_per_msg": $JAVA_CYCLES,
      "memory_mb": $JAVA_MEMORY_MB,
      "notes": "ArrayBlockingQueue"
    }
EOF
        echo "[OK] Java: ${JAVA_THROUGHPUT}M msg/sec (${JAVA_MEMORY_MB}MB)"
    else
        echo "[SKIP] Java: Build failed (skipped)"
    fi
else
    echo "[SKIP] Java: Not installed (skipped)"
fi

# 6. Zig (if available)
if command -v zig &> /dev/null; then
    echo "Building Zig ping-pong..."
    if [ -f "zig/Makefile" ] && (cd zig && make ping_pong &>/dev/null); then
        ZIG_OUTPUT=$(cd zig && /usr/bin/time -l ./ping_pong 2>&1 || time -v ./ping_pong 2>&1 || true)
        ZIG_CYCLES=$(echo "$ZIG_OUTPUT" | grep "Cycles/msg" | awk '{print $2}')
        ZIG_THROUGHPUT=$(echo "$ZIG_OUTPUT" | grep "Throughput" | awk '{print $2}')
        
        # Only add result if we got valid output
        if [ -n "$ZIG_THROUGHPUT" ] && [ "$ZIG_THROUGHPUT" != "0" ]; then
            ZIG_MSG_PER_SEC=$(echo "$ZIG_THROUGHPUT * 1000000" | bc 2>/dev/null | awk '{printf "%.0f", $0}')
            
            add_result
            cat >> "$RESULTS_FILE" <<EOF
    "Zig": {
      "runtime": "Native",
      "msg_per_sec": $ZIG_MSG_PER_SEC,
      "cycles_per_msg": ${ZIG_CYCLES:-0},
      "notes": "std::Thread with Mutex"
    }
EOF
            echo "[OK] Zig: ${ZIG_THROUGHPUT}M msg/sec"
        else
            echo "[SKIP] Zig: Benchmark failed (no valid output)"
        fi
    else
        echo "[SKIP] Zig: Build failed (skipped)"
    fi
else
    echo "[SKIP] Zig: Not installed (skipped)"
fi

# 7. Elixir (if available)
if command -v elixir &> /dev/null; then
    echo "Running Elixir ping-pong..."
    if [ -f "elixir/ping_pong.exs" ]; then
        ELIXIR_OUTPUT=$(cd elixir && elixir ping_pong.exs 2>&1 || true)
        ELIXIR_CYCLES=$(echo "$ELIXIR_OUTPUT" | grep "Cycles/msg" | awk '{print $2}')
        ELIXIR_THROUGHPUT=$(echo "$ELIXIR_OUTPUT" | grep "Throughput" | awk '{print $2}')
        
        # Only add result if we got valid output
        if [ -n "$ELIXIR_THROUGHPUT" ] && [ "$ELIXIR_THROUGHPUT" != "0" ]; then
            ELIXIR_MSG_PER_SEC=$(echo "$ELIXIR_THROUGHPUT * 1000000" | bc 2>/dev/null | awk '{printf "%.0f", $0}')
            
            add_result
            cat >> "$RESULTS_FILE" <<EOF
    "Elixir": {
      "runtime": "BEAM VM",
      "msg_per_sec": $ELIXIR_MSG_PER_SEC,
      "cycles_per_msg": ${ELIXIR_CYCLES:-0},
      "notes": "Native process messaging"
    }
EOF
            echo "[OK] Elixir: ${ELIXIR_THROUGHPUT}M msg/sec"
        else
            echo "[SKIP] Elixir: Benchmark failed (no valid output)"
        fi
    else
        echo "[SKIP] Elixir: Benchmark not found (skipped)"
    fi
else
    echo "[SKIP] Elixir: Not installed (skipped)"
fi

# 8. Erlang (if available)
if command -v erlc &> /dev/null; then
    echo "Building Erlang ping-pong..."
    if [ -f "erlang/ping_pong.erl" ] && (cd erlang && erlc ping_pong.erl 2>&1); then
        ERLANG_OUTPUT=$(cd erlang && erl -noshell -s ping_pong start 2>&1 || true)
        ERLANG_CYCLES=$(echo "$ERLANG_OUTPUT" | grep "Cycles/msg" | awk '{print $2}')
        ERLANG_THROUGHPUT=$(echo "$ERLANG_OUTPUT" | grep "Throughput" | awk '{print $2}')
        
        # Only add result if we got valid output
        if [ -n "$ERLANG_THROUGHPUT" ] && [ "$ERLANG_THROUGHPUT" != "0" ]; then
            ERLANG_MSG_PER_SEC=$(echo "$ERLANG_THROUGHPUT * 1000000" | bc 2>/dev/null | awk '{printf "%.0f", $0}')
            
            add_result
            cat >> "$RESULTS_FILE" <<EOF
    "Erlang": {
      "runtime": "BEAM VM",
      "msg_per_sec": $ERLANG_MSG_PER_SEC,
      "cycles_per_msg": ${ERLANG_CYCLES:-0},
      "notes": "Native process messaging"
    }
EOF
            echo "[OK] Erlang: ${ERLANG_THROUGHPUT}M msg/sec"
        else
            echo "[SKIP] Erlang: Benchmark failed (no valid output)"
        fi
    else
        echo "[SKIP] Erlang: Build failed (skipped)"
    fi
else
    echo "[SKIP] Erlang: Not installed (skipped)"
fi

# 9. Pony (if available)
if command -v ponyc &> /dev/null; then
    echo "Building Pony ping-pong..."
    if [ -f "pony/ping_pong.pony" ] && (cd pony && ponyc . &>/dev/null); then
        PONY_OUTPUT=$(cd pony && ./pony 2>&1 || true)
        PONY_CYCLES=$(echo "$PONY_OUTPUT" | grep "Cycles/msg" | awk '{print $2}')
        PONY_THROUGHPUT=$(echo "$PONY_OUTPUT" | grep "Throughput" | awk '{print $2}')
        
        # Only add result if we got valid output
        if [ -n "$PONY_THROUGHPUT" ] && [ "$PONY_THROUGHPUT" != "0" ]; then
            PONY_MSG_PER_SEC=$(echo "$PONY_THROUGHPUT * 1000000" | bc 2>/dev/null | awk '{printf "%.0f", $0}')
            
            add_result
            cat >> "$RESULTS_FILE" <<EOF
    "Pony": {
      "runtime": "Native (Pony runtime)",
      "msg_per_sec": $PONY_MSG_PER_SEC,
      "cycles_per_msg": ${PONY_CYCLES:-0},
      "notes": "Actor model language"
    }
EOF
            echo "[OK] Pony: $(printf "%.2f" $PONY_THROUGHPUT)M msg/sec"
        else
            echo "[SKIP] Pony: Benchmark failed (no valid output)"
        fi
    else
        echo "[SKIP] Pony: Build failed (skipped)"
    fi
else
    echo "[SKIP] Pony: Not installed (skipped)"
fi

# 10. Scala (if available)
if command -v sbt &> /dev/null; then
    echo "Building Scala ping-pong..."
    if [ -f "scala/ping_pong.scala" ] && (cd scala && sbt compile &>/dev/null && sbt "runMain PingPongBenchmark" 2>&1 > /tmp/scala_output.txt); then
        SCALA_OUTPUT=$(cat /tmp/scala_output.txt 2>&1 || true)
        SCALA_CYCLES=$(echo "$SCALA_OUTPUT" | grep "Cycles/msg" | awk '{print $2}')
        SCALA_THROUGHPUT=$(echo "$SCALA_OUTPUT" | grep "Throughput" | awk '{print $2}')
        
        # Only add result if we got valid output
        if [ -n "$SCALA_THROUGHPUT" ] && [ "$SCALA_THROUGHPUT" != "0" ]; then
            SCALA_MSG_PER_SEC=$(echo "$SCALA_THROUGHPUT * 1000000" | bc 2>/dev/null | awk '{printf "%.0f", $0}')
            
            add_result
            cat >> "$RESULTS_FILE" <<EOF
    "Scala": {
      "runtime": "JVM (Scala)",
      "msg_per_sec": $SCALA_MSG_PER_SEC,
      "cycles_per_msg": ${SCALA_CYCLES:-0},
      "notes": "Akka actors"
    }
EOF
            echo "[OK] Scala: $(printf "%.2f" $SCALA_THROUGHPUT)M msg/sec"
        else
            echo "[SKIP] Scala: Benchmark failed (no valid output)"
        fi
    else
        echo "[SKIP] Scala: Build failed (skipped)"
    fi
else
    echo "[SKIP] Scala: Not installed (skipped)"
fi

# Close JSON
cat >> "$RESULTS_FILE" <<EOF

  }
}
EOF

echo ""
echo "============================================"
echo "Results saved to: $RESULTS_FILE"
echo "============================================"
echo ""
echo "Note: Only real benchmark results included."
echo "Languages that couldn't run were skipped."
