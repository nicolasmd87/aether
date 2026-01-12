// Zig ping-pong benchmark using std.Thread and Mutex
const std = @import("std");
const Thread = std.Thread;
const Mutex = std.Thread.Mutex;
const Condition = std.Thread.Condition;

const MESSAGES = 10_000_000;

const Channel = struct {
    mutex: Mutex = .{},
    cond: Condition = .{},
    ready: bool = false,
    value: i32 = 0,
};

var chan_a: Channel = .{};
var chan_b: Channel = .{};

fn pingThread(_: *anyopaque) u8 {
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
        chan_b.ready = false;
        chan_b.mutex.unlock();
    }
    return 0;
}

fn pongThread(_: *anyopaque) u8 {
    var i: i32 = 0;
    while (i < MESSAGES) : (i += 1) {
        // Wait for A
        chan_a.mutex.lock();
        while (!chan_a.ready) {
            chan_a.cond.wait(&chan_a.mutex);
        }
        chan_a.ready = false;
        chan_a.mutex.unlock();

        // Send to B
        chan_b.mutex.lock();
        chan_b.value = i;
        chan_b.ready = true;
        chan_b.cond.signal();
        chan_b.mutex.unlock();
    }
    return 0;
}

fn rdtsc() u64 {
    // For ARM (Apple Silicon), we'll use timer
    if (@import("builtin").cpu.arch == .aarch64) {
        var ts: std.os.timespec = undefined;
        _ = std.os.clock_gettime(std.os.CLOCK.MONOTONIC, &ts) catch return 0;
        return @as(u64, @intCast(ts.tv_sec)) * 1_000_000_000 + @as(u64, @intCast(ts.tv_nsec));
    }
    // For x86_64, use RDTSC
    return asm volatile ("rdtsc"
        : [ret] "={eax},{edx}" (-> u64),
    );
}

pub fn main() !void {
    const stdout = std.io.getStdOut().writer();

    try stdout.print("=== Zig Ping-Pong Benchmark ===\n", .{});
    try stdout.print("Messages: {}\n", .{MESSAGES});
    try stdout.print("Using std.Thread with Mutex and Condition\n\n", .{});

    const start = rdtsc();

    var thread1 = try Thread.spawn(.{}, pingThread, .{@as(*anyopaque, undefined)});
    var thread2 = try Thread.spawn(.{}, pongThread, .{@as(*anyopaque, undefined)});

    thread1.join();
    thread2.join();

    const end = rdtsc();
    const total_cycles = end - start;

    if (@import("builtin").cpu.arch == .aarch64) {
        // ARM: nanoseconds
        const ns_per_msg = @as(f64, @floatFromInt(total_cycles)) / @as(f64, MESSAGES);
        const throughput = 1e9 / ns_per_msg;
        const cycles_per_msg = ns_per_msg * 3.0; // Approximate at 3GHz

        try stdout.print("Cycles/msg:     {d:.2}\n", .{cycles_per_msg});
        try stdout.print("Throughput:     {d:.2} M msg/sec\n", .{throughput / 1e6});
    } else {
        // x86_64: actual cycles
        const cycles_per_msg = @as(f64, @floatFromInt(total_cycles)) / @as(f64, MESSAGES);
        const freq = 3.0e9; // Approximate
        const throughput = freq / cycles_per_msg;

        try stdout.print("Cycles/msg:     {d:.2}\n", .{cycles_per_msg});
        try stdout.print("Throughput:     {d:.2} M msg/sec\n", .{throughput / 1e6});
    }
}
