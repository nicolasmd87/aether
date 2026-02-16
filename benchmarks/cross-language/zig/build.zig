const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // Ping Pong benchmark
    const ping_pong = b.addExecutable(.{
        .name = "ping_pong",
        .root_module = b.createModule(.{
            .root_source_file = b.path("ping_pong.zig"),
            .target = target,
            .optimize = optimize,
        }),
    });
    b.installArtifact(ping_pong);

    // Counting benchmark
    const counting = b.addExecutable(.{
        .name = "counting",
        .root_module = b.createModule(.{
            .root_source_file = b.path("counting.zig"),
            .target = target,
            .optimize = optimize,
        }),
    });
    b.installArtifact(counting);

    // Thread Ring benchmark
    const thread_ring = b.addExecutable(.{
        .name = "thread_ring",
        .root_module = b.createModule(.{
            .root_source_file = b.path("thread_ring.zig"),
            .target = target,
            .optimize = optimize,
        }),
    });
    b.installArtifact(thread_ring);

    // Fork Join benchmark
    const fork_join = b.addExecutable(.{
        .name = "fork_join",
        .root_module = b.createModule(.{
            .root_source_file = b.path("fork_join.zig"),
            .target = target,
            .optimize = optimize,
        }),
    });
    b.installArtifact(fork_join);
}
