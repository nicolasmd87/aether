// Zig ping-pong benchmark using std.Thread and Mutex
const std = @import("std");
const Thread = std.Thread;
const Mutex = std.Thread.Mutex;
const Condition = std.Thread.Condition;
const print = std.debug.print;

fn getMessages() usize {
    if (std.posix.getenv("BENCHMARK_MESSAGES")) |val| {
        return std.fmt.parseInt(usize, val, 10) catch 100_000;
    }
    return 100_000;
}

var MESSAGES: usize = 0;

const Channel = struct {
    mutex: Mutex = .{},
    cond: Condition = .{},
    ready: bool = false,
    value: i32 = 0,
};

var chan_a: Channel = .{};
var chan_b: Channel = .{};

fn pingThread(_: *anyopaque) void {
    var i: i32 = 0;
    while (i < MESSAGES) : (i += 1) {
        // Send to A
        chan_a.mutex.lock();
        chan_a.value = i;
        chan_a.ready = true;
        chan_a.cond.signal();
        chan_a.mutex.unlock();

        // Wait for B
        chan_b.mutex.lock();
        while (!chan_b.ready) {
            chan_b.cond.wait(&chan_b.mutex);
        }
        const received = chan_b.value;
        chan_b.ready = false;
        chan_b.mutex.unlock();

        // Validate received value matches what was sent
        if (received != i) {
            print("Ping validation error: expected {}, got {}\n", .{ i, received });
        }
    }
}

fn pongThread(_: *anyopaque) void {
    var expected: i32 = 0;
    while (expected < MESSAGES) : (expected += 1) {
        // Wait for A
        chan_a.mutex.lock();
        while (!chan_a.ready) {
            chan_a.cond.wait(&chan_a.mutex);
        }
        const received = chan_a.value;
        chan_a.ready = false;
        chan_a.mutex.unlock();

        // Validate received value matches expected sequence
        if (received != expected) {
            print("Pong validation error: expected {}, got {}\n", .{ expected, received });
        }

        // Send back the EXACT value received to B
        chan_b.mutex.lock();
        chan_b.value = received;
        chan_b.ready = true;
        chan_b.cond.signal();
        chan_b.mutex.unlock();
    }
}

fn getTimeNs() u64 {
    const ts = std.posix.clock_gettime(std.posix.CLOCK.MONOTONIC) catch return 0;
    return @as(u64, @intCast(ts.sec)) * 1_000_000_000 + @as(u64, @intCast(ts.nsec));
}

pub fn main() !void {
    MESSAGES = getMessages();

    print("=== Zig Ping-Pong Benchmark ===\n", .{});
    print("Messages: {}\n", .{MESSAGES});
    print("Using std.Thread with Mutex and Condition\n\n", .{});

    const start = getTimeNs();

    const thread1 = try Thread.spawn(.{}, pingThread, .{@as(*anyopaque, undefined)});
    const thread2 = try Thread.spawn(.{}, pongThread, .{@as(*anyopaque, undefined)});

    thread1.join();
    thread2.join();

    const end = getTimeNs();
    const elapsed_ns = end - start;
    const elapsed_sec = @as(f64, @floatFromInt(elapsed_ns)) / 1e9;
    const cycles_per_msg = elapsed_sec * 3e9 / @as(f64, @floatFromInt(MESSAGES));
    const throughput = @as(f64, @floatFromInt(MESSAGES)) / elapsed_sec / 1e6;

    print("Cycles/msg:     {d:.2}\n", .{cycles_per_msg});
    print("Throughput:     {d:.2} M msg/sec\n", .{throughput});
}
