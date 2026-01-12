use tokio::sync::mpsc;
use tokio::task;
use std::time::Instant;

const BASE: usize = 10;
const COUNT: i64 = 1000;

async fn skynet(level: usize, num: usize, tx: mpsc::Sender<i64>) {
    if level >= 4 {
        let _ = tx.send(num as i64 + COUNT).await;
    } else {
        let (result_tx, mut result_rx) = mpsc::channel::<i64>(BASE);

        for i in 0..BASE {
            let child_tx = result_tx.clone();
            task::spawn(skynet(level + 1, num * BASE + i, child_tx));
        }
        drop(result_tx);

        let mut sum = 0;
        while let Some(val) = result_rx.recv().await {
            sum += val;
        }

        let _ = tx.send(sum).await;
    }
}

#[tokio::main]
async fn main() {
    println!("=== Rust Skynet Benchmark ===");
    println!("Base: {}", BASE);
    println!("Depth: 4");

    let expected_actors = 1 + BASE + (BASE * BASE) + (BASE * BASE * BASE);
    let total_messages = expected_actors * COUNT as usize;

    println!("\nExpected actors: {}", expected_actors);
    println!("Messages per actor: {}", COUNT);
    println!("Total messages: {}\n", total_messages);

    let (tx, mut rx) = mpsc::channel::<i64>(BASE);

    let start = Instant::now();

    for i in 0..BASE {
        let child_tx = tx.clone();
        task::spawn(skynet(0, i, child_tx));
    }
    drop(tx);

    let mut _sum = 0;
    while let Some(val) = rx.recv().await {
        _sum += val;
    }

    let elapsed = start.elapsed();

    let cycles = (elapsed.as_secs_f64() * 3e9) as u64;
    let cycles_per_msg = cycles as f64 / total_messages as f64;
    let msg_per_sec = total_messages as f64 / elapsed.as_secs_f64();

    println!("Cycles/msg:     {:.2}", cycles_per_msg);
    println!("Throughput:     {:.0} M msg/sec", msg_per_sec / 1_000_000.0);
}
