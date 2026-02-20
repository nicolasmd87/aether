use std::sync::mpsc::sync_channel;
use std::thread;
use std::time::Instant;

fn get_messages() -> usize {
    std::env::var("BENCHMARK_MESSAGES")
        .ok()
        .and_then(|s| s.parse().ok())
        .unwrap_or(100_000)
}

fn main() {
    let messages = get_messages();
    println!("=== Rust Ping-Pong Benchmark ===");
    println!("Messages: {}", messages);
    println!("Using std::thread with sync_channel\n");

    let (ping_tx, ping_rx) = sync_channel::<i32>(1);
    let (pong_tx, pong_rx) = sync_channel::<i32>(1);

    let start = Instant::now();

    let ping_thread = thread::spawn(move || {
        for i in 0..messages {
            let val = i as i32;
            ping_tx.send(val).unwrap();
            let received = pong_rx.recv().unwrap();
            if received != val {
                eprintln!("ERROR: Ping sent {} but got back {}", val, received);
            }
        }
    });

    let pong_thread = thread::spawn(move || {
        for i in 0..messages {
            let expected = i as i32;
            let received = ping_rx.recv().unwrap();
            if received != expected {
                eprintln!("ERROR: Pong expected {} but got {}", expected, received);
            }
            pong_tx.send(received).unwrap();
        }
    });

    ping_thread.join().unwrap();
    pong_thread.join().unwrap();

    let elapsed = start.elapsed().as_secs_f64();
    let ns_per_msg = elapsed * 1e9 / messages as f64;
    let msg_per_sec = messages as f64 / elapsed;

    println!("ns/msg:         {:.2}", ns_per_msg);
    println!("Throughput:     {:.2} M msg/sec", msg_per_sec / 1_000_000.0);
}
