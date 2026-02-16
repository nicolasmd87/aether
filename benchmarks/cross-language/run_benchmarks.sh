#!/usr/bin/env bash
# Main benchmark runner - Runs ALL Savina benchmark patterns
# Patterns: ping_pong, counting, thread_ring, fork_join

# Don't exit on errors - we want to skip failures gracefully
set +e
cd "$(dirname "$0")"

# Source cargo environment if it exists
[ -f "$HOME/.cargo/env" ] && source "$HOME/.cargo/env"

# Source GVM (Go Version Manager) if it exists
[ -s "$HOME/.gvm/scripts/gvm" ] && source "$HOME/.gvm/scripts/gvm"

# Add common tool paths for macOS (Homebrew) and Linux
export PATH="$HOME/.cargo/bin:$PATH"
export PATH="/opt/homebrew/bin:$PATH"
export PATH="/usr/local/bin:$PATH"
export PATH="/usr/local/go/bin:$PATH"
export PATH="$HOME/go/bin:$PATH"
export PATH="$HOME/.local/bin:$PATH"

# GVM Go paths (if gvm is installed but not sourced)
if [ -d "$HOME/.gvm/gos" ]; then
    for go_ver in "$HOME/.gvm/gos"/*/bin; do
        [ -d "$go_ver" ] && export PATH="$go_ver:$PATH"
    done
fi

# Erlang/Elixir paths (macOS Homebrew)
if [ -d "/opt/homebrew/opt/erlang/bin" ]; then
    export PATH="/opt/homebrew/opt/erlang/bin:$PATH"
fi
if [ -d "/opt/homebrew/opt/elixir/bin" ]; then
    export PATH="/opt/homebrew/opt/elixir/bin:$PATH"
fi

# Erlang/Elixir paths (Linux - apt/dnf/asdf)
if [ -d "/usr/lib/erlang/bin" ]; then
    export PATH="/usr/lib/erlang/bin:$PATH"
fi
if [ -d "/usr/local/lib/erlang/bin" ]; then
    export PATH="/usr/local/lib/erlang/bin:$PATH"
fi
# asdf version manager (common on Linux)
if [ -d "$HOME/.asdf/shims" ]; then
    export PATH="$HOME/.asdf/shims:$PATH"
fi

# Zig paths (macOS Homebrew)
if [ -d "/opt/homebrew/opt/zig" ]; then
    export PATH="/opt/homebrew/opt/zig:$PATH"
fi
# Zig paths (Linux - snap/manual install)
if [ -d "/snap/bin" ]; then
    export PATH="/snap/bin:$PATH"
fi

# Pony paths (macOS Homebrew)
if [ -d "/opt/homebrew/opt/ponyc/bin" ]; then
    export PATH="/opt/homebrew/opt/ponyc/bin:$PATH"
fi
# Pony paths (Linux - apt/manual install)
if [ -d "/usr/local/lib/ponyc/bin" ]; then
    export PATH="/usr/local/lib/ponyc/bin:$PATH"
fi

# Auto-detect Java 17+ for Scala (portable across macOS/Linux)
detect_java() {
    if [ -n "$JAVA_HOME" ] && [ -x "$JAVA_HOME/bin/java" ]; then
        local version=$("$JAVA_HOME/bin/java" -version 2>&1 | awk -F '"' '/version/ {print $2}' | cut -d. -f1)
        if [ "$version" -ge 17 ] 2>/dev/null; then
            export PATH="$JAVA_HOME/bin:$PATH"
            return 0
        fi
    fi

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

    if command -v java >/dev/null 2>&1; then
        local version=$(java -version 2>&1 | awk -F '"' '/version/ {print $2}' | cut -d. -f1)
        if [ "$version" -ge 17 ] 2>/dev/null; then
            return 0
        fi
    fi

    return 1
}

# Interactive dependency checking
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
        if [ ! -t 0 ]; then
            echo "Skipping $tool_name benchmark (non-interactive)."
            return 1
        fi
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

# Check dependencies (skip install prompts in non-interactive mode)
detect_java || true
check_and_install "Go" "go" "brew install go" "sudo apt-get install golang" "Required for Go benchmark"
check_and_install "Rust" "cargo" "brew install rust" "curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh" "Required for Rust benchmark"
check_and_install "Zig" "zig" "brew install zig" "sudo snap install zig --classic --beta" "Required for Zig benchmark"
check_and_install "Erlang" "erl" "brew install erlang" "sudo apt-get install erlang" "Required for Erlang benchmark"
check_and_install "Elixir" "elixir" "brew install elixir" "sudo apt-get install elixir" "Required for Elixir benchmark"
check_and_install "Pony" "ponyc" "brew install ponyc" "sudo add-apt-repository ppa:ponylang/ponylang && sudo apt-get update && sudo apt-get install ponyc" "Required for Pony benchmark"

echo ""
echo "Starting benchmarks..."
echo ""

# Load benchmark configuration
CONFIG_FILE="benchmark_config.json"
if [ -f "$CONFIG_FILE" ]; then
    BENCHMARK_MESSAGES=$(python3 -c "import json; print(json.load(open('$CONFIG_FILE'))['messages'])" 2>/dev/null || echo "10000000")
    BENCHMARK_TIMEOUT=$(python3 -c "import json; print(json.load(open('$CONFIG_FILE'))['timeout_seconds'])" 2>/dev/null || echo "60")
else
    BENCHMARK_MESSAGES=10000000
    BENCHMARK_TIMEOUT=60
fi

export BENCHMARK_MESSAGES

# Get pattern description
get_pattern_desc() {
    case "$1" in
        ping_pong) echo "Two actors exchange messages back and forth" ;;
        counting) echo "Single actor counts incoming messages sequentially" ;;
        thread_ring) echo "Token passing through ring of 20 actors" ;;
        fork_join) echo "Parallel worker pool with 8 workers processing messages" ;;
        *) echo "Unknown pattern" ;;
    esac
}

# Parse output from benchmark run
parse_output() {
    local output="$1"
    local cycles throughput msg_per_sec memory_kb memory_mb

    # Use "Cycles/msg:" and "Throughput:" with colon to avoid matching "Throughput Benchmark" in titles
    cycles=$(echo "$output" | grep "Cycles/msg:" | awk '{print $2}' | head -1)
    throughput=$(echo "$output" | grep "Throughput:" | awk '{print $2}' | head -1)

    # Calculate msg/sec from throughput (M msg/sec)
    if [ -n "$throughput" ] && [ "$throughput" != "0" ]; then
        msg_per_sec=$(echo "$throughput * 1000000" | bc 2>/dev/null | awk '{printf "%.0f", $0}')
    else
        msg_per_sec=0
    fi

    # Memory (macOS vs Linux format)
    memory_kb=$(echo "$output" | grep -E "maximum resident set size|Maximum resident" | awk '{print $1}' | head -1)
    if [ -n "$memory_kb" ]; then
        memory_mb=$(echo "scale=2; $memory_kb / 1024 / 1024" | bc 2>/dev/null | awk '{printf "%.2f", ($0 == "" ? 0 : $0)}')
    else
        memory_mb=0
    fi

    echo "${cycles:-0}|${msg_per_sec:-0}|${memory_mb:-0}"
}

# Run all benchmarks for a single pattern
run_pattern() {
    local pattern="$1"
    local RESULTS_FILE="visualize/results_${pattern}.json"
    local desc=$(get_pattern_desc "$pattern")
    local FIRST_RESULT=true

    echo "============================================"
    echo "  Running: $pattern"
    echo "  Messages: $BENCHMARK_MESSAGES"
    echo "============================================"

    # Create results file header
    cat > "$RESULTS_FILE" <<EOF
{
  "timestamp": "$(date -u +"%Y-%m-%dT%H:%M:%SZ")",
  "pattern": "$pattern",
  "description": "$desc",
  "hardware": {
    "cpu": "$(sysctl -n machdep.cpu.brand_string 2>/dev/null || lscpu | grep 'Model name' | cut -d: -f2 | xargs || echo 'Unknown')",
    "cores": $(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo '8'),
    "os": "$(uname -s)"
  },
  "benchmarks": {
EOF

    add_comma() {
        if [ "$FIRST_RESULT" = false ]; then
            echo "," >> "$RESULTS_FILE"
        fi
        FIRST_RESULT=false
    }

    # Aether
    if [ -f "aether/${pattern}.ae" ]; then
        echo -n "  Aether... "
        if (cd aether && make clean &>/dev/null && make $pattern &>/dev/null); then
            output=$(cd aether && BENCHMARK_MESSAGES=$BENCHMARK_MESSAGES /usr/bin/time -l ./$pattern 2>&1 || true)
            parsed=$(parse_output "$output")
            cycles=$(echo "$parsed" | cut -d'|' -f1)
            msg_per_sec=$(echo "$parsed" | cut -d'|' -f2)
            memory_mb=$(echo "$parsed" | cut -d'|' -f3)

            if [ "$msg_per_sec" != "0" ] && [ -n "$msg_per_sec" ]; then
                throughput_m=$(echo "scale=2; $msg_per_sec / 1000000" | bc 2>/dev/null)
                echo "[OK] ${throughput_m}M msg/sec (${memory_mb}MB)"
                add_comma
                cat >> "$RESULTS_FILE" <<EOF
    "Aether": {
      "runtime": "Native (Aether runtime)",
      "msg_per_sec": $msg_per_sec,
      "cycles_per_msg": $cycles,
      "memory_mb": $memory_mb,
      "notes": "SPSC queues, work inlining, direct send"
    }
EOF
            else
                echo "[SKIP] No valid output"
            fi
        else
            echo "[SKIP] Build failed"
        fi
    fi

    # C (pthread)
    if [ -f "c/${pattern}.c" ]; then
        echo -n "  C (pthread)... "
        if (cd c && make clean &>/dev/null && make $pattern &>/dev/null); then
            output=$(cd c && BENCHMARK_MESSAGES=$BENCHMARK_MESSAGES /usr/bin/time -l ./$pattern 2>&1 || true)
            parsed=$(parse_output "$output")
            cycles=$(echo "$parsed" | cut -d'|' -f1)
            msg_per_sec=$(echo "$parsed" | cut -d'|' -f2)
            memory_mb=$(echo "$parsed" | cut -d'|' -f3)

            if [ "$msg_per_sec" != "0" ] && [ -n "$msg_per_sec" ]; then
                throughput_m=$(echo "scale=2; $msg_per_sec / 1000000" | bc 2>/dev/null)
                echo "[OK] ${throughput_m}M msg/sec (${memory_mb}MB)"
                add_comma
                cat >> "$RESULTS_FILE" <<EOF
    "C (pthread)": {
      "runtime": "Native (pthread)",
      "msg_per_sec": $msg_per_sec,
      "cycles_per_msg": $cycles,
      "memory_mb": $memory_mb,
      "notes": "pthread mutex + condvar (baseline)"
    }
EOF
            else
                echo "[SKIP] No valid output"
            fi
        else
            echo "[SKIP] Build failed"
        fi
    fi

    # C++
    if [ -f "cpp/${pattern}.cpp" ]; then
        echo -n "  C++... "
        if (cd cpp && g++ -O3 -std=c++17 -march=native ${pattern}.cpp -o $pattern -pthread &>/dev/null); then
            output=$(cd cpp && BENCHMARK_MESSAGES=$BENCHMARK_MESSAGES /usr/bin/time -l ./$pattern 2>&1 || true)
            parsed=$(parse_output "$output")
            cycles=$(echo "$parsed" | cut -d'|' -f1)
            msg_per_sec=$(echo "$parsed" | cut -d'|' -f2)
            memory_mb=$(echo "$parsed" | cut -d'|' -f3)

            if [ "$msg_per_sec" != "0" ] && [ -n "$msg_per_sec" ]; then
                throughput_m=$(echo "scale=2; $msg_per_sec / 1000000" | bc 2>/dev/null)
                echo "[OK] ${throughput_m}M msg/sec (${memory_mb}MB)"
                add_comma
                cat >> "$RESULTS_FILE" <<EOF
    "C++": {
      "runtime": "Native (std::thread)",
      "msg_per_sec": $msg_per_sec,
      "cycles_per_msg": $cycles,
      "memory_mb": $memory_mb,
      "notes": "std::mutex + std::condition_variable"
    }
EOF
            else
                echo "[SKIP] No valid output"
            fi
        else
            echo "[SKIP] Build failed"
        fi
    fi

    # Go
    if command -v go &>/dev/null && [ -f "go/${pattern}.go" ]; then
        echo -n "  Go... "
        if (cd go && go build -o $pattern ${pattern}.go &>/dev/null); then
            output=$(cd go && BENCHMARK_MESSAGES=$BENCHMARK_MESSAGES /usr/bin/time -l ./$pattern 2>&1 || true)
            parsed=$(parse_output "$output")
            cycles=$(echo "$parsed" | cut -d'|' -f1)
            msg_per_sec=$(echo "$parsed" | cut -d'|' -f2)
            memory_mb=$(echo "$parsed" | cut -d'|' -f3)

            if [ "$msg_per_sec" != "0" ] && [ -n "$msg_per_sec" ]; then
                throughput_m=$(echo "scale=2; $msg_per_sec / 1000000" | bc 2>/dev/null)
                echo "[OK] ${throughput_m}M msg/sec (${memory_mb}MB)"
                add_comma
                cat >> "$RESULTS_FILE" <<EOF
    "Go": {
      "runtime": "Go runtime",
      "msg_per_sec": $msg_per_sec,
      "cycles_per_msg": $cycles,
      "memory_mb": $memory_mb,
      "notes": "Goroutines with channels"
    }
EOF
            else
                echo "[SKIP] No valid output"
            fi
        else
            echo "[SKIP] Build failed"
        fi
    fi

    # Rust
    if command -v cargo &>/dev/null && [ -f "rust/src/${pattern}.rs" ]; then
        echo -n "  Rust... "
        if (cd rust && cargo build --release --bin $pattern &>/dev/null); then
            output=$(cd rust && BENCHMARK_MESSAGES=$BENCHMARK_MESSAGES /usr/bin/time -l ./target/release/$pattern 2>&1 || true)
            parsed=$(parse_output "$output")
            cycles=$(echo "$parsed" | cut -d'|' -f1)
            msg_per_sec=$(echo "$parsed" | cut -d'|' -f2)
            memory_mb=$(echo "$parsed" | cut -d'|' -f3)

            if [ "$msg_per_sec" != "0" ] && [ -n "$msg_per_sec" ]; then
                throughput_m=$(echo "scale=2; $msg_per_sec / 1000000" | bc 2>/dev/null)
                echo "[OK] ${throughput_m}M msg/sec (${memory_mb}MB)"
                add_comma
                cat >> "$RESULTS_FILE" <<EOF
    "Rust": {
      "runtime": "Native (std::sync)",
      "msg_per_sec": $msg_per_sec,
      "cycles_per_msg": $cycles,
      "memory_mb": $memory_mb,
      "notes": "sync_channel (bounded MPSC)"
    }
EOF
            else
                echo "[SKIP] No valid output"
            fi
        else
            echo "[SKIP] Build failed"
        fi
    fi

    # Java - convert pattern to class name (snake_case to PascalCase)
    java_class=$(echo "$pattern" | sed 's/_\([a-z]\)/\U\1/g' | sed 's/^\([a-z]\)/\U\1/')
    if command -v javac &>/dev/null && [ -f "java/${java_class}.java" ]; then
        echo -n "  Java... "
        if (cd java && javac ${java_class}.java &>/dev/null); then
            output=$(cd java && BENCHMARK_MESSAGES=$BENCHMARK_MESSAGES /usr/bin/time -l java $java_class 2>&1 || true)
            parsed=$(parse_output "$output")
            cycles=$(echo "$parsed" | cut -d'|' -f1)
            msg_per_sec=$(echo "$parsed" | cut -d'|' -f2)
            memory_mb=$(echo "$parsed" | cut -d'|' -f3)

            if [ "$msg_per_sec" != "0" ] && [ -n "$msg_per_sec" ]; then
                throughput_m=$(echo "scale=2; $msg_per_sec / 1000000" | bc 2>/dev/null)
                echo "[OK] ${throughput_m}M msg/sec (${memory_mb}MB)"
                add_comma
                cat >> "$RESULTS_FILE" <<EOF
    "Java": {
      "runtime": "JVM",
      "msg_per_sec": $msg_per_sec,
      "cycles_per_msg": $cycles,
      "memory_mb": $memory_mb,
      "notes": "BlockingQueue (bounded)"
    }
EOF
            else
                echo "[SKIP] No valid output"
            fi
        else
            echo "[SKIP] Build failed"
        fi
    fi

    # Zig
    if command -v zig &>/dev/null && [ -f "zig/${pattern}.zig" ]; then
        echo -n "  Zig... "
        if (cd zig && make $pattern &>/dev/null); then
            output=$(cd zig && BENCHMARK_MESSAGES=$BENCHMARK_MESSAGES /usr/bin/time -l ./$pattern 2>&1 || true)
            parsed=$(parse_output "$output")
            cycles=$(echo "$parsed" | cut -d'|' -f1)
            msg_per_sec=$(echo "$parsed" | cut -d'|' -f2)
            memory_mb=$(echo "$parsed" | cut -d'|' -f3)

            if [ "$msg_per_sec" != "0" ] && [ -n "$msg_per_sec" ]; then
                throughput_m=$(echo "scale=2; $msg_per_sec / 1000000" | bc 2>/dev/null)
                echo "[OK] ${throughput_m}M msg/sec (${memory_mb}MB)"
                add_comma
                cat >> "$RESULTS_FILE" <<EOF
    "Zig": {
      "runtime": "Native",
      "msg_per_sec": $msg_per_sec,
      "cycles_per_msg": $cycles,
      "memory_mb": $memory_mb,
      "notes": "Mutex + Condition variable"
    }
EOF
            else
                echo "[SKIP] No valid output"
            fi
        else
            echo "[SKIP] Build failed"
        fi
    fi

    # Elixir
    if command -v elixir &>/dev/null && [ -f "elixir/${pattern}.exs" ]; then
        echo -n "  Elixir... "
        output=$(cd elixir && BENCHMARK_MESSAGES=$BENCHMARK_MESSAGES /usr/bin/time -l elixir ${pattern}.exs 2>&1 || true)
        parsed=$(parse_output "$output")
        cycles=$(echo "$parsed" | cut -d'|' -f1)
        msg_per_sec=$(echo "$parsed" | cut -d'|' -f2)
        memory_mb=$(echo "$parsed" | cut -d'|' -f3)

        if [ "$msg_per_sec" != "0" ] && [ -n "$msg_per_sec" ]; then
            throughput_m=$(echo "scale=2; $msg_per_sec / 1000000" | bc 2>/dev/null)
            echo "[OK] ${throughput_m}M msg/sec (${memory_mb}MB)"
            add_comma
            cat >> "$RESULTS_FILE" <<EOF
    "Elixir": {
      "runtime": "BEAM VM",
      "msg_per_sec": $msg_per_sec,
      "cycles_per_msg": $cycles,
      "memory_mb": $memory_mb,
      "notes": "BEAM lightweight processes"
    }
EOF
        else
            echo "[SKIP] No valid output"
        fi
    fi

    # Erlang
    if command -v erlc &>/dev/null && [ -f "erlang/${pattern}.erl" ]; then
        echo -n "  Erlang... "
        if (cd erlang && erlc ${pattern}.erl &>/dev/null); then
            output=$(cd erlang && BENCHMARK_MESSAGES=$BENCHMARK_MESSAGES /usr/bin/time -l erl -noshell -s ${pattern} start 2>&1 || true)
            parsed=$(parse_output "$output")
            cycles=$(echo "$parsed" | cut -d'|' -f1)
            msg_per_sec=$(echo "$parsed" | cut -d'|' -f2)
            memory_mb=$(echo "$parsed" | cut -d'|' -f3)

            if [ "$msg_per_sec" != "0" ] && [ -n "$msg_per_sec" ]; then
                throughput_m=$(echo "scale=2; $msg_per_sec / 1000000" | bc 2>/dev/null)
                echo "[OK] ${throughput_m}M msg/sec (${memory_mb}MB)"
                add_comma
                cat >> "$RESULTS_FILE" <<EOF
    "Erlang": {
      "runtime": "BEAM VM",
      "msg_per_sec": $msg_per_sec,
      "cycles_per_msg": $cycles,
      "memory_mb": $memory_mb,
      "notes": "BEAM lightweight processes"
    }
EOF
            else
                echo "[SKIP] No valid output"
            fi
        else
            echo "[SKIP] Build failed"
        fi
    fi

    # Pony
    if command -v ponyc &>/dev/null && [ -d "pony/${pattern}" ]; then
        echo -n "  Pony... "
        if (cd pony/${pattern} && ponyc . -o . &>/dev/null); then
            output=$(cd pony/${pattern} && BENCHMARK_MESSAGES=$BENCHMARK_MESSAGES /usr/bin/time -l ./${pattern} 2>&1 || true)
            parsed=$(parse_output "$output")
            cycles=$(echo "$parsed" | cut -d'|' -f1)
            msg_per_sec=$(echo "$parsed" | cut -d'|' -f2)
            memory_mb=$(echo "$parsed" | cut -d'|' -f3)

            if [ "$msg_per_sec" != "0" ] && [ -n "$msg_per_sec" ]; then
                throughput_m=$(echo "scale=2; $msg_per_sec / 1000000" | bc 2>/dev/null)
                echo "[OK] ${throughput_m}M msg/sec (${memory_mb}MB)"
                add_comma
                cat >> "$RESULTS_FILE" <<EOF
    "Pony": {
      "runtime": "Native (Pony runtime)",
      "msg_per_sec": $msg_per_sec,
      "cycles_per_msg": $cycles,
      "memory_mb": $memory_mb,
      "notes": "GC-free actors, ref capabilities"
    }
EOF
            else
                echo "[SKIP] No valid output"
            fi
        else
            echo "[SKIP] Build failed"
        fi
    fi

    # Close JSON
    cat >> "$RESULTS_FILE" <<EOF

  }
}
EOF

    echo ""
    echo "Results saved to: $RESULTS_FILE"
    echo ""
}

# Main: Run all patterns
echo "============================================"
echo "  Cross-Language Actor Benchmark Suite"
echo "  Patterns: ping_pong counting thread_ring fork_join"
echo "  Messages: $BENCHMARK_MESSAGES"
echo "============================================"
echo ""

for pattern in ping_pong counting thread_ring fork_join; do
    run_pattern "$pattern"
done

echo "============================================"
echo "  All benchmarks complete!"
echo "  Results in: visualize/results_*.json"
echo "============================================"
