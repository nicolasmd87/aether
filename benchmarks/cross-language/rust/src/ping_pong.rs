use std::sync::mpsc::sync_channel;
use std::thread;
use std::time::Instant;

fn get_messages() -> usize {
    std::env::var("BENCHMARK_MESSAGES")
        .ok()
        .and_then(|s| s.parse().ok())
        .unwrap_or(100_000)
}

#[cfg(target_arch = "x86_64")]
fn rdtsc() -> u64 {
    unsafe { std::arch::x86_64::_rdtsc() }
}

#[cfg(target_arch = "aarch64")]
fn rdtsc() -> u64 {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_nanos() as u64
}

fn main() {
    let messages = get_messages();
    println!("=== Rust Ping-Pong Benchmark ===");
    println!("Messages: {}", messages);
    println!("Using std::thread with sync_channel\n");

    let (ping_tx, ping_rx) = sync_channel::<i32>(1);
    let (pong_tx, pong_rx) = sync_channel::<i32>(1);

    let start = rdtsc();
    let time_start = Instant::now();

    let ping_thread = thread::spawn(move || {
        for i in 0..messages {
            let val = i as i32;
            ping_tx.send(val).unwrap();
            let received = pong_rx.recv().unwrap();
            // VALIDATE: Must receive echo of what we sent
            if received != val {
                eprintln!("ERROR: Ping sent {} but got back {}", val, received);
            }
        }
    });

    let pong_thread = thread::spawn(move || {
        for i in 0..messages {
            let expected = i as i32;
            let received = ping_rx.recv().unwrap();
            // VALIDATE: Must receive expected sequence
            if received != expected {
                eprintln!("ERROR: Pong expected {} but got {}", expected, received);
            }
            // Echo back what we received
            pong_tx.send(received).unwrap();
        }
    });

    ping_thread.join().unwrap();
    pong_thread.join().unwrap();

    let cycles = rdtsc() - start;
    let elapsed = time_start.elapsed();

    #[cfg(target_arch = "x86_64")]
    let cycles_per_msg = cycles as f64 / messages as f64;

    #[cfg(target_arch = "aarch64")]
    let cycles_per_msg = (cycles as f64 / messages as f64) * 3.0; // Convert ns to cycles at ~3GHz

    let msg_per_sec = messages as f64 / elapsed.as_secs_f64();

    println!("Cycles/msg:     {:.2}", cycles_per_msg);
    println!("Throughput:     {:.2} M msg/sec", msg_per_sec / 1_000_000.0);
}
