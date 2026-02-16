// Rust Counting Actor Benchmark (Savina-style)
use std::sync::mpsc::{channel, Sender, Receiver};
use std::thread;
use std::time::Instant;

fn get_messages() -> usize {
    std::env::var("BENCHMARK_MESSAGES")
        .ok()
        .and_then(|s| s.parse().ok())
        .unwrap_or(100_000)
}

fn counter(rx: Receiver<()>) -> usize {
    let mut count = 0;
    while rx.recv().is_ok() {
        count += 1;
    }
    count
}

fn main() {
    let messages = get_messages();
    println!("=== Rust Counting Actor Benchmark ===");
    println!("Messages: {}\n", messages);

    let (tx, rx) = channel();

    let handle = thread::spawn(move || counter(rx));

    let start = Instant::now();

    // Send all messages
    for _ in 0..messages {
        tx.send(()).unwrap();
    }
    drop(tx); // Signal done

    let count = handle.join().unwrap();

    let elapsed = start.elapsed().as_secs_f64();

    if count != messages {
        println!("VALIDATION FAILED: expected {}, got {}", messages, count);
    }

    let throughput = messages as f64 / elapsed / 1e6;
    let cycles_per_msg = elapsed * 3e9 / messages as f64;

    println!("Cycles/msg:     {:.2}", cycles_per_msg);
    println!("Throughput:     {:.2} M msg/sec", throughput);
}
