#!/usr/bin/env elixir

# Elixir ping-pong benchmark using processes
defmodule PingPong do
  @messages (System.get_env("BENCHMARK_MESSAGES") |> case do
    nil -> 100_000
    val -> String.to_integer(val)
  end)

  def ping(pong_pid, 0, _) do
    send(pong_pid, :done)
  end

  def ping(pong_pid, n, i) do
    send(pong_pid, {:ping, self(), i})
    receive do
      {:pong, value} ->
        # Validate received value matches what was sent
        if value != i do
          IO.puts(:stderr, "Ping validation error: expected #{i}, got #{value}")
        end
        ping(pong_pid, n - 1, i + 1)
    end
  end

  def pong(expected) do
    receive do
      {:ping, sender, value} ->
        # Validate received value matches expected sequence
        if value != expected do
          IO.puts(:stderr, "Pong validation error: expected #{expected}, got #{value}")
        end
        # Echo back the EXACT value received
        send(sender, {:pong, value})
        pong(expected + 1)
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
    pong_pid = spawn(fn -> pong(0) end)

    start = rdtsc()
    ping(pong_pid, @messages, 0)
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
