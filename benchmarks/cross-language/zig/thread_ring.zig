// Zig Thread Ring Benchmark (Savina-style)
const std = @import("std");
const Thread = std.Thread;
const Mutex = std.Thread.Mutex;
const Condition = std.Thread.Condition;
const Atomic = std.atomic.Value;
const print = std.debug.print;

const RING_SIZE: usize = 100;
fn getMessages() usize {
    if (std.posix.getenv("BENCHMARK_MESSAGES")) |val| {
        return std.fmt.parseInt(usize, val, 10) catch 100_000;
    }
    return 100_000;
}

var NUM_HOPS: usize = 0;

const Node = struct {
    mutex: Mutex = .{},
    cond: Condition = .{},
    token: i32 = -1,
    has_token: bool = false,
    done: bool = false,
    received: usize = 0,
};

var nodes: [RING_SIZE]Node = undefined;
var completion: Atomic(bool) = Atomic(bool).init(false);
var final_received: Atomic(usize) = Atomic(usize).init(0);

fn nodeThread(ctx: *anyopaque) void {
    const id = @intFromPtr(ctx);
    const next_id = (id + 1) % RING_SIZE;
    var node = &nodes[id];
    var next = &nodes[next_id];

    while (true) {
        node.mutex.lock();
        while (!node.has_token and !node.done) {
            node.cond.wait(&node.mutex);
        }

        if (node.done) {
            node.mutex.unlock();
            break;
        }

        const t = node.token;
        node.has_token = false;
        node.received += 1;
        node.mutex.unlock();

        if (t == 0) {
            final_received.store(node.received, .seq_cst);
            completion.store(true, .seq_cst);
            return;
        }

        next.mutex.lock();
        next.token = t - 1;
        next.has_token = true;
        next.cond.signal();
        next.mutex.unlock();
    }
}

fn getTimeNs() u64 {
    const ts = std.posix.clock_gettime(std.posix.CLOCK.MONOTONIC) catch return 0;
    return @as(u64, @intCast(ts.sec)) * 1_000_000_000 + @as(u64, @intCast(ts.nsec));
}

pub fn main() !void {
    NUM_HOPS = getMessages();

    print("=== Zig Thread Ring Benchmark ===\n", .{});
    print("Ring size: {}, Hops: {}\n\n", .{ RING_SIZE, NUM_HOPS });

    // Initialize nodes
    for (0..RING_SIZE) |i| {
        nodes[i] = Node{};
    }

    // Start threads
    var threads: [RING_SIZE]Thread = undefined;
    for (0..RING_SIZE) |i| {
        threads[i] = try Thread.spawn(.{}, nodeThread, .{@as(*anyopaque, @ptrFromInt(i))});
    }

    const start = getTimeNs();

    // Inject token
    nodes[0].mutex.lock();
    nodes[0].token = @intCast(NUM_HOPS);
    nodes[0].has_token = true;
    nodes[0].cond.signal();
    nodes[0].mutex.unlock();

    // Wait for completion
    while (!completion.load(.seq_cst)) {
        std.Thread.yield() catch {};
    }

    const end = getTimeNs();

    // Stop all threads
    for (0..RING_SIZE) |i| {
        nodes[i].mutex.lock();
        nodes[i].done = true;
        nodes[i].cond.signal();
        nodes[i].mutex.unlock();
    }

    for (threads) |t| {
        t.join();
    }

    const total_messages = NUM_HOPS + 1;
    const elapsed_ns = end - start;
    const elapsed_sec = @as(f64, @floatFromInt(elapsed_ns)) / 1e9;
    const throughput = @as(f64, @floatFromInt(total_messages)) / elapsed_sec / 1e6;
    const cycles_per_msg = elapsed_sec * 3e9 / @as(f64, @floatFromInt(total_messages));

    print("Cycles/msg:     {d:.2}\n", .{cycles_per_msg});
    print("Throughput:     {d:.2} M msg/sec\n", .{throughput});
}
