// Zig Counting Actor Benchmark (Savina-style)
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

const Counter = struct {
    mutex: Mutex = .{},
    cond: Condition = .{},
    pending: usize = 0,
    done: bool = false,
    processed: usize = 0,
};

var counter: Counter = .{};

fn counterThread(_: *anyopaque) void {
    while (true) {
        counter.mutex.lock();
        while (counter.pending == 0 and !counter.done) {
            counter.cond.wait(&counter.mutex);
        }
        if (counter.done and counter.pending == 0) {
            counter.mutex.unlock();
            break;
        }
        while (counter.pending > 0) {
            counter.pending -= 1;
            counter.processed += 1;
        }
        counter.mutex.unlock();
    }
}

fn getTimeNs() u64 {
    const ts = std.posix.clock_gettime(std.posix.CLOCK.MONOTONIC) catch return 0;
    return @as(u64, @intCast(ts.sec)) * 1_000_000_000 + @as(u64, @intCast(ts.nsec));
}

pub fn main() !void {
    MESSAGES = getMessages();

    print("=== Zig Counting Actor Benchmark ===\n", .{});
    print("Messages: {}\n\n", .{MESSAGES});

    const thread = try Thread.spawn(.{}, counterThread, .{@as(*anyopaque, undefined)});

    const start = getTimeNs();

    // Send all messages
    for (0..MESSAGES) |_| {
        counter.mutex.lock();
        counter.pending += 1;
        counter.cond.signal();
        counter.mutex.unlock();
    }

    // Signal done
    counter.mutex.lock();
    counter.done = true;
    counter.cond.signal();
    counter.mutex.unlock();

    thread.join();

    const end = getTimeNs();

    if (counter.processed != MESSAGES) {
        print("VALIDATION FAILED: expected {}, got {}\n", .{ MESSAGES, counter.processed });
    }

    const elapsed_ns = end - start;
    const elapsed_sec = @as(f64, @floatFromInt(elapsed_ns)) / 1e9;
    const throughput = @as(f64, @floatFromInt(MESSAGES)) / elapsed_sec / 1e6;
    const cycles_per_msg = elapsed_sec * 3e9 / @as(f64, @floatFromInt(MESSAGES));

    print("Cycles/msg:     {d:.2}\n", .{cycles_per_msg});
    print("Throughput:     {d:.2} M msg/sec\n", .{throughput});
}
