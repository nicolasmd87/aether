// Zig Fork-Join Benchmark (Savina-style)
// K worker threads, M messages distributed round-robin
const std = @import("std");
const Thread = std.Thread;
const Mutex = std.Thread.Mutex;
const Condition = std.Thread.Condition;
const Allocator = std.mem.Allocator;
const print = std.debug.print;

const NUM_WORKERS: usize = 8;

fn getMessages() usize {
    if (std.posix.getenv("BENCHMARK_MESSAGES")) |val| {
        return std.fmt.parseInt(usize, val, 10) catch 100_000;
    }
    return 100_000;
}

var TOTAL_MESSAGES: usize = 0;

const Worker = struct {
    mutex: Mutex = .{},
    cond: Condition = .{},
    queue: std.ArrayListUnmanaged(i32) = .{},
    processed: usize = 0,
    done: bool = false,

    fn deinit(self: *Worker, allocator: Allocator) void {
        self.queue.deinit(allocator);
    }

    fn enqueue(self: *Worker, allocator: Allocator, value: i32) void {
        self.mutex.lock();
        self.queue.append(allocator, value) catch {};
        self.cond.signal();
        self.mutex.unlock();
    }
};

var workers: [NUM_WORKERS]Worker = undefined;
var global_allocator: Allocator = undefined;

fn workerThread(ctx: *anyopaque) void {
    const id = @intFromPtr(ctx);
    var self = &workers[id];

    while (true) {
        self.mutex.lock();
        while (self.queue.items.len == 0 and !self.done) {
            self.cond.wait(&self.mutex);
        }

        if (self.done and self.queue.items.len == 0) {
            self.mutex.unlock();
            break;
        }

        // Process all available items
        while (self.queue.items.len > 0) {
            _ = self.queue.orderedRemove(0);
            self.processed += 1;
        }
        self.mutex.unlock();
    }
}

fn getTimeNs() u64 {
    const ts = std.posix.clock_gettime(std.posix.CLOCK.MONOTONIC) catch return 0;
    return @as(u64, @intCast(ts.sec)) * 1_000_000_000 + @as(u64, @intCast(ts.nsec));
}

pub fn main() !void {
    TOTAL_MESSAGES = getMessages();

    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();
    global_allocator = allocator;

    print("=== Zig Fork-Join Throughput Benchmark ===\n", .{});
    print("Workers: {}, Messages: {}\n\n", .{ NUM_WORKERS, TOTAL_MESSAGES });

    // Initialize workers
    for (0..NUM_WORKERS) |i| {
        workers[i] = Worker{};
    }
    defer {
        for (0..NUM_WORKERS) |i| {
            workers[i].deinit(allocator);
        }
    }

    // Start worker threads
    var threads: [NUM_WORKERS]Thread = undefined;
    for (0..NUM_WORKERS) |i| {
        threads[i] = try Thread.spawn(.{}, workerThread, .{@as(*anyopaque, @ptrFromInt(i))});
    }

    const start = getTimeNs();

    // Send messages round-robin
    for (0..TOTAL_MESSAGES) |i| {
        workers[i % NUM_WORKERS].enqueue(allocator, @intCast(i));
    }

    // Signal workers to finish
    for (0..NUM_WORKERS) |i| {
        workers[i].mutex.lock();
        workers[i].done = true;
        workers[i].cond.signal();
        workers[i].mutex.unlock();
    }

    // Wait for all workers
    for (threads) |t| {
        t.join();
    }

    const end = getTimeNs();

    // Collect results
    var total_processed: usize = 0;
    for (0..NUM_WORKERS) |i| {
        total_processed += workers[i].processed;
    }

    if (total_processed != TOTAL_MESSAGES) {
        print("VALIDATION FAILED: expected {}, got {}\n", .{ TOTAL_MESSAGES, total_processed });
    }

    const elapsed_ns = end - start;
    const elapsed_sec = @as(f64, @floatFromInt(elapsed_ns)) / 1e9;
    const throughput = @as(f64, @floatFromInt(TOTAL_MESSAGES)) / elapsed_sec / 1e6;
    const cycles_per_msg = elapsed_sec * 3e9 / @as(f64, @floatFromInt(TOTAL_MESSAGES));

    print("Cycles/msg:     {d:.2}\n", .{cycles_per_msg});
    print("Throughput:     {d:.2} M msg/sec\n", .{throughput});
}
