#!/usr/bin/env elixir

# Elixir ping-pong benchmark using processes
defmodule PingPong do
  @messages 10_000_000

  def ping(pong_pid, 0) do
    send(pong_pid, :done)
  end

  def ping(pong_pid, n) do
    send(pong_pid, {:ping, self()})
    receive do
      :pong -> ping(pong_pid, n - 1)
    end
  end

  def pong(ping_pid) do
    receive do
      {:ping, sender} ->
        send(sender, :pong)
        pong(ping_pid)
      :done ->
        :ok
    end
  end

  def rdtsc() do
    # Use monotonic time in nanoseconds
    :erlang.monotonic_time(:nanosecond)
  end

  def run() do
    IO.puts("=== Elixir Ping-Pong Benchmark ===")
    IO.puts("Messages: #{@messages}")
    IO.puts("Using Erlang/OTP processes\n")

    ping_pid = self()
    pong_pid = spawn(fn -> pong(ping_pid) end)

    start = rdtsc()
    ping(pong_pid, @messages)
    finish = rdtsc()

    total_ns = finish - start
    ns_per_msg = total_ns / @messages
    throughput = 1.0e9 / ns_per_msg
    cycles_per_msg = ns_per_msg * 3.0  # Approximate at 3GHz

    IO.puts("Cycles/msg:     #{Float.round(cycles_per_msg, 2)}")
    IO.puts("Throughput:     #{Float.round(throughput / 1.0e6, 2)} M msg/sec")
  end
end

PingPong.run()
