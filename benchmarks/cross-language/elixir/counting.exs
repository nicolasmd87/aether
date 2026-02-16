# Elixir Counting Actor Benchmark (Savina-style)

defmodule Counting do
  @messages (System.get_env("BENCHMARK_MESSAGES") |> case do
    nil -> 100_000
    val -> String.to_integer(val)
  end)

  def start do
    IO.puts("=== Elixir Counting Actor Benchmark ===")
    IO.puts("Messages: #{@messages}\n")

    parent = self()
    counter = spawn(fn -> counter_loop(0, parent) end)

    start_time = :erlang.monotonic_time(:nanosecond)

    # Send all messages
    Enum.each(1..@messages, fn _ -> send(counter, :increment) end)

    # Signal done
    send(counter, {:done, self()})

    # Wait for count
    receive do
      {:count, count} ->
        end_time = :erlang.monotonic_time(:nanosecond)
        elapsed_ns = end_time - start_time
        elapsed_sec = elapsed_ns / 1_000_000_000.0

        if count != @messages do
          IO.puts("VALIDATION FAILED: expected #{@messages}, got #{count}")
        end

        throughput = @messages / elapsed_sec / 1_000_000
        cycles_per_msg = elapsed_sec * 3.0e9 / @messages

        IO.puts("Cycles/msg:     #{:io_lib.format("~.2f", [cycles_per_msg])}")
        IO.puts("Throughput:     #{:io_lib.format("~.2f", [throughput])} M msg/sec")
    after
      60_000 ->
        IO.puts("Timeout!")
        System.halt(1)
    end
  end

  defp counter_loop(count, parent) do
    receive do
      :increment ->
        counter_loop(count + 1, parent)
      {:done, from} ->
        send(from, {:count, count})
    end
  end
end

Counting.start()
