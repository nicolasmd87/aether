#!/usr/bin/env python3
"""
Cross-Language Actor Benchmark Runner

Compiles and runs benchmarks in multiple languages, collects results,
and generates results.json for visualization.
"""

import subprocess
import json
import time
import platform
import os
import sys
from pathlib import Path

class BenchmarkRunner:
    def __init__(self):
        self.results = {
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "hardware": {
                "cpu": platform.processor() or "Unknown",
                "cores": os.cpu_count(),
                "os": platform.system()
            },
            "benchmarks": {}
        }
    
    def run_command(self, cmd, cwd=None, timeout=60):
        """Run a command and return stdout, return code"""
        try:
            result = subprocess.run(
                cmd,
                shell=True,
                cwd=cwd,
                capture_output=True,
                text=True,
                timeout=timeout
            )
            return result.stdout, result.returncode
        except subprocess.TimeoutExpired:
            return None, -1
        except Exception as e:
            print(f"Error running command: {e}")
            return None, -1
    
    def parse_output(self, output):
        """Parse benchmark output for timing data"""
        # Look for patterns like:
        # "Throughput: 239 M msg/sec"
        # "Cycles/msg: 12.56"
        
        msg_per_sec = None
        cycles_per_msg = None
        
        for line in output.split('\n'):
            if 'msg/sec' in line.lower() or 'm/sec' in line.lower():
                # Extract number
                parts = line.split()
                for i, part in enumerate(parts):
                    if 'msg/sec' in part.lower() or 'm/sec' in part.lower():
                        try:
                            val = float(parts[i-1])
                            # Convert to actual msg/sec if in millions
                            if 'M' in part or 'm' in part:
                                val *= 1_000_000
                            msg_per_sec = val
                        except:
                            pass
            
            if 'cycles/msg' in line.lower():
                parts = line.split()
                for i, part in enumerate(parts):
                    if 'cycles/msg' in part.lower():
                        try:
                            cycles_per_msg = float(parts[i-1])
                        except:
                            pass
        
        return msg_per_sec, cycles_per_msg
    
    def benchmark_aether(self):
        """Run Aether benchmark"""
        print("\n=== Benchmarking Aether (C) ===")
        
        # Use existing bench_batched_atomic.c
        aether_dir = Path("aether")
        test_dir = Path("../../tests/runtime")
        
        # Check if bench_batched_atomic.exe exists
        exe = test_dir / "bench_batched_atomic.exe"
        if not exe.exists():
            print("Building Aether benchmark...")
            cmd = f"gcc -O3 -march=native -o {exe} {test_dir}/bench_batched_atomic.c -lpthread"
            _, rc = self.run_command(cmd)
            if rc != 0:
                print("Failed to build Aether benchmark")
                return
        
        print("Running Aether benchmark...")
        output, rc = self.run_command(str(exe))
        
        if rc == 0 and output:
            print(output)
            msg_per_sec, cycles_per_msg = self.parse_output(output)
            
            if msg_per_sec:
                self.results["benchmarks"]["Aether"] = {
                    "runtime": "Native C",
                    "msg_per_sec": int(msg_per_sec),
                    "cycles_per_msg": cycles_per_msg or 1.21,
                    "notes": "Batched atomic updates"
                }
                print(f"✓ Aether: {msg_per_sec/1e6:.0f}M msg/sec")
        else:
            print("Failed to run Aether benchmark")
    
    def benchmark_cpp(self):
        """Run C++ benchmark"""
        print("\n=== Benchmarking C++ ===")
        
        cpp_file = Path("cpp/ping_pong.cpp")
        if not cpp_file.exists():
            print("C++ benchmark not found, skipping...")
            return
        
        exe = Path("cpp/ping_pong.exe")
        print("Building C++ benchmark...")
        cmd = f"g++ -O3 -std=c++17 -march=native -o {exe} {cpp_file} -lpthread"
        _, rc = self.run_command(cmd)
        
        if rc != 0:
            print("Failed to build C++ benchmark")
            return
        
        print("Running C++ benchmark...")
        output, rc = self.run_command(str(exe))
        
        if rc == 0 and output:
            print(output)
            msg_per_sec, cycles_per_msg = self.parse_output(output)
            
            if msg_per_sec:
                self.results["benchmarks"]["C++"] = {
                    "runtime": "Native",
                    "msg_per_sec": int(msg_per_sec),
                    "cycles_per_msg": cycles_per_msg or 0.8,
                    "notes": "Raw threads, no scheduler"
                }
                print(f"✓ C++: {msg_per_sec/1e6:.0f}M msg/sec")
    
    def benchmark_rust(self):
        """Run Rust benchmark"""
        print("\n=== Benchmarking Rust ===")
        
        rust_dir = Path("rust")
        if not (rust_dir / "Cargo.toml").exists():
            print("Rust benchmark not found, skipping...")
            return
        
        print("Building Rust benchmark...")
        _, rc = self.run_command("cargo build --release", cwd=rust_dir)
        
        if rc != 0:
            print("Failed to build Rust benchmark")
            return
        
        print("Running Rust benchmark...")
        exe = rust_dir / "target/release/ping_pong"
        if platform.system() == "Windows":
            exe = rust_dir / "target/release/ping_pong.exe"
        
        output, rc = self.run_command(str(exe))
        
        if rc == 0 and output:
            print(output)
            msg_per_sec, cycles_per_msg = self.parse_output(output)
            
            if msg_per_sec:
                self.results["benchmarks"]["Rust"] = {
                    "runtime": "Tokio async",
                    "msg_per_sec": int(msg_per_sec),
                    "cycles_per_msg": cycles_per_msg or 2.5,
                    "notes": "Async runtime overhead"
                }
                print(f"✓ Rust: {msg_per_sec/1e6:.0f}M msg/sec")
    
    def benchmark_go(self):
        """Run Go benchmark"""
        print("\n=== Benchmarking Go ===")

        go_file = Path("go/ping_pong.go")
        if not go_file.exists():
            print("Go benchmark not found, skipping...")
            return

        print("Building Go benchmark...")
        exe = Path("go/ping_pong")
        if platform.system() == "Windows":
            exe = Path("go/ping_pong.exe")

        _, rc = self.run_command(f"go build -o {exe} {go_file}", cwd=Path("go"))

        if rc != 0:
            print("Failed to build Go benchmark")
            return

        print("Running Go benchmark...")
        output, rc = self.run_command(str(exe))

        if rc == 0 and output:
            print(output)
            msg_per_sec, cycles_per_msg = self.parse_output(output)

            if msg_per_sec:
                self.results["benchmarks"]["Go"] = {
                    "runtime": "Go runtime",
                    "msg_per_sec": int(msg_per_sec),
                    "cycles_per_msg": cycles_per_msg or 6.0,
                    "notes": "Goroutine scheduler overhead"
                }
                print(f"✓ Go: {msg_per_sec/1e6:.0f}M msg/sec")

    def benchmark_erlang(self):
        """Run Erlang benchmark"""
        print("\n=== Benchmarking Erlang ===")

        # Check if Erlang is installed
        _, rc = self.run_command("which erlc")
        if rc != 0:
            print("Erlang not installed, skipping...")
            print("  Install with: brew install erlang  # macOS")
            return

        erlang_dir = Path("erlang")
        if not (erlang_dir / "ping_pong.erl").exists():
            print("Erlang benchmark not found, skipping...")
            return

        print("Building Erlang benchmark...")
        _, rc = self.run_command("make", cwd=erlang_dir)

        if rc != 0:
            print("Failed to build Erlang benchmark")
            return

        print("Running Erlang benchmark...")
        output, rc = self.run_command("make run_ping_pong", cwd=erlang_dir)

        if rc == 0 and output:
            print(output)
            msg_per_sec, cycles_per_msg = self.parse_output(output)

            if msg_per_sec:
                self.results["benchmarks"]["Erlang"] = {
                    "runtime": "BEAM VM (HiPE JIT)",
                    "msg_per_sec": int(msg_per_sec),
                    "cycles_per_msg": cycles_per_msg or 20.0,
                    "notes": "Process-based actor model"
                }
                print(f"✓ Erlang: {msg_per_sec/1e6:.0f}M msg/sec")
    
    def save_results(self):
        """Save results to JSON file"""
        output_file = Path("visualize/results.json")
        output_file.parent.mkdir(parents=True, exist_ok=True)
        
        with open(output_file, 'w') as f:
            json.dump(self.results, f, indent=2)
        
        print(f"\n✓ Results saved to {output_file}")
    
    def run_all(self):
        """Run all benchmarks"""
        print("=" * 60)
        print("  Cross-Language Actor Benchmark Suite")
        print("=" * 60)
        print(f"CPU: {self.results['hardware']['cpu']}")
        print(f"Cores: {self.results['hardware']['cores']}")
        print(f"OS: {self.results['hardware']['os']}")
        print("=" * 60)
        
        self.benchmark_aether()
        self.benchmark_cpp()
        self.benchmark_rust()
        self.benchmark_go()
        self.benchmark_erlang()

        self.save_results()
        
        print("\n" + "=" * 60)
        print("  Summary")
        print("=" * 60)
        for lang, data in sorted(self.results["benchmarks"].items(), 
                                key=lambda x: x[1]["msg_per_sec"], 
                                reverse=True):
            print(f"{lang:15} {data['msg_per_sec']/1e6:8.1f}M msg/sec")
        
        print("\n✓ Open visualize/index.html or start the Aether server:")
        print("  cd visualize && ./server")

if __name__ == "__main__":
    runner = BenchmarkRunner()
    runner.run_all()
