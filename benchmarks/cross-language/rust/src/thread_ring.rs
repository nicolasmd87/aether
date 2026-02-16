// Rust Thread Ring Benchmark (Savina-style)
use std::sync::mpsc::{channel, Sender, Receiver};
use std::thread;
use std::time::Instant;

const RING_SIZE: usize = 100;

fn get_messages() -> usize {
    std::env::var("BENCHMARK_MESSAGES")
        .ok()
        .and_then(|s| s.parse().ok())
        .unwrap_or(100_000)
}

fn ring_node(rx: Receiver<usize>, next_tx: Sender<usize>, done_tx: Sender<usize>) {
    let mut received = 0;
    while let Ok(token) = rx.recv() {
        received += 1;
        if token == 0 {
            done_tx.send(received).unwrap();
            return;
        }
        next_tx.send(token - 1).unwrap();
    }
}

fn main() {
    let num_hops = get_messages();
    println!("=== Rust Thread Ring Benchmark ===");
    println!("Ring size: {}, Hops: {}\n", RING_SIZE, num_hops);

    let (done_tx, done_rx) = channel();

    // Create channels for the ring
    let mut senders: Vec<Sender<usize>> = Vec::with_capacity(RING_SIZE);
    let mut receivers: Vec<Receiver<usize>> = Vec::with_capacity(RING_SIZE);

    for _ in 0..RING_SIZE {
        let (tx, rx) = channel();
        senders.push(tx);
        receivers.push(rx);
    }

    // Start threads
    let mut handles = Vec::with_capacity(RING_SIZE);
    for i in 0..RING_SIZE {
        let rx = receivers.remove(0);
        let next_tx = senders[(i + 1) % RING_SIZE].clone();
        let done = done_tx.clone();
        handles.push(thread::spawn(move || ring_node(rx, next_tx, done)));
    }
    drop(done_tx);

    let start = Instant::now();

    // Inject token
    senders[0].send(num_hops).unwrap();

    // Wait for completion
    let received = done_rx.recv().unwrap();

    let elapsed = start.elapsed().as_secs_f64();

    // Cleanup - drop senders to unblock threads
    drop(senders);
    for h in handles {
        let _ = h.join();
    }

    let total_messages = num_hops + 1;
    // Each node receives total_messages / ring_size messages
    let expected_per_node = total_messages / RING_SIZE + 1;
    if received < expected_per_node - 10 || received > expected_per_node + 10 {
        println!("VALIDATION WARNING: expected ~{} per node, got {}", expected_per_node, received);
    }

    let throughput = total_messages as f64 / elapsed / 1e6;
    let cycles_per_msg = elapsed * 3e9 / total_messages as f64;

    println!("Cycles/msg:     {:.2}", cycles_per_msg);
    println!("Throughput:     {:.2} M msg/sec", throughput);
}
