// Rust Fork-Join Benchmark (Savina-style)
use std::sync::mpsc::{channel, Sender, Receiver};
use std::thread;
use std::time::Instant;

const NUM_WORKERS: usize = 8;

fn get_messages() -> usize {
    std::env::var("BENCHMARK_MESSAGES")
        .ok()
        .and_then(|s| s.parse().ok())
        .unwrap_or(100_000)
}

fn worker(rx: Receiver<i32>) -> usize {
    let mut count = 0;
    while rx.recv().is_ok() {
        count += 1;
    }
    count
}

fn main() {
    let total = get_messages();
    println!("=== Rust Fork-Join Throughput Benchmark ===");
    println!("Workers: {}, Messages: {}\n", NUM_WORKERS, total);

    // Create workers
    let mut senders: Vec<Sender<i32>> = Vec::with_capacity(NUM_WORKERS);
    let mut handles = Vec::with_capacity(NUM_WORKERS);

    for _ in 0..NUM_WORKERS {
        let (tx, rx) = channel();
        senders.push(tx);
        handles.push(thread::spawn(move || worker(rx)));
    }

    let start = Instant::now();

    // Send messages round-robin
    for i in 0..total {
        senders[i % NUM_WORKERS].send(i as i32).unwrap();
    }

    // Signal workers to finish
    drop(senders);

    // Collect results
    let mut total_processed = 0;
    for h in handles {
        total_processed += h.join().unwrap();
    }

    let elapsed = start.elapsed().as_secs_f64();

    if total_processed != total {
        println!("VALIDATION FAILED: expected {}, got {}", total, total_processed);
    }

    let throughput = total as f64 / elapsed / 1e6;
    let cycles_per_msg = elapsed * 3e9 / total as f64;

    println!("Cycles/msg:     {:.2}", cycles_per_msg);
    println!("Throughput:     {:.2} M msg/sec", throughput);
}
