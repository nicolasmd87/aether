use tokio::sync::mpsc;
use tokio::task;
use std::time::Instant;

const RING_SIZE: usize = 100;
const ROUNDS: i64 = 100_000;
const TOTAL_MESSAGES: i64 = RING_SIZE as i64 * ROUNDS;

async fn ring_actor(mut rx: mpsc::Receiver<i64>, tx: mpsc::Sender<i64>) {
    while let Some(token) = rx.recv().await {
        if token > 0 {
            let _ = tx.send(token - 1).await;
        } else {
            break;
        }
    }
}

#[tokio::main]
async fn main() {
    println!("=== Rust Ring Benchmark ===");
    println!("Ring size: {} actors", RING_SIZE);
    println!("Rounds: {}", ROUNDS);
    println!("Total messages: {}\n", TOTAL_MESSAGES);

    // Create ring of channels
    let (first_tx, mut first_rx) = mpsc::channel::<i64>(1000);
    let mut prev_tx = first_tx.clone();

    // Spawn ring actors
    for _ in 1..RING_SIZE {
        let (tx, rx) = mpsc::channel::<i64>(1000);
        let next_tx = tx.clone();
        task::spawn(ring_actor(rx, prev_tx));
        prev_tx = next_tx;
    }

    let start = Instant::now();

    // Send initial token
    let _ = prev_tx.send(TOTAL_MESSAGES).await;

    // Close loop: forward first_rx to prev_tx
    task::spawn(async move {
        while let Some(token) = first_rx.recv().await {
            if token > 0 {
                let _ = prev_tx.send(token - 1).await;
            }
        }
    });

    // Wait for completion
    tokio::time::sleep(tokio::time::Duration::from_secs(30)).await;

    let elapsed = start.elapsed();

    // Calculate metrics
    let cycles = (elapsed.as_secs_f64() * 3e9) as u64;
    let cycles_per_msg = cycles as f64 / TOTAL_MESSAGES as f64;
    let msg_per_sec = TOTAL_MESSAGES as f64 / elapsed.as_secs_f64();

    println!("Cycles/msg:     {:.2}", cycles_per_msg);
    println!("Throughput:     {:.0} M msg/sec", msg_per_sec / 1_000_000.0);
}
