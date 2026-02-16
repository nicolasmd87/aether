# Elixir Thread Ring Benchmark (Savina-style)

defmodule ThreadRing do
  @ring_size 100
  @num_hops (System.get_env("BENCHMARK_MESSAGES") |> case do
    nil -> 100_000
    val -> String.to_integer(val)
  end)

  def start do
    IO.puts("=== Elixir Thread Ring Benchmark ===")
    IO.puts("Ring size: #{@ring_size}, Hops: #{@num_hops}\n")

    parent = self()

    # Create ring of processes
    first = spawn(fn -> ring_node(nil, parent, 0) end)
    last = create_ring(first, @ring_size - 1, parent)

    # Close the ring: last -> first (not first -> last!)
    send(last, {:set_next, first})

    start_time = :erlang.monotonic_time(:nanosecond)

    # Send initial token
    send(first, {:token, @num_hops})

    # Wait for completion
    receive do
      {:done, total_received} ->
        end_time = :erlang.monotonic_time(:nanosecond)
        elapsed_ns = end_time - start_time
        elapsed_sec = elapsed_ns / 1_000_000_000.0

        # Each node receives ~(num_hops + 1) / ring_size messages
        expected_per_node = div(@num_hops + 1, @ring_size) + 1
        if total_received < expected_per_node - 10 or total_received > expected_per_node + 10 do
          IO.puts("VALIDATION WARNING: expected ~#{expected_per_node} per node, got #{total_received}")
        end

        throughput = (@num_hops + 1) / elapsed_sec / 1_000_000
        cycles_per_msg = elapsed_sec * 3.0e9 / (@num_hops + 1)

        IO.puts("Cycles/msg:     #{:io_lib.format("~.2f", [cycles_per_msg])}")
        IO.puts("Throughput:     #{:io_lib.format("~.2f", [throughput])} M msg/sec")
    after
      60_000 ->
        IO.puts("Timeout!")
        System.halt(1)
    end
  end

  defp create_ring(prev, 0, _parent), do: prev
  defp create_ring(prev, n, parent) do
    next = spawn(fn -> ring_node(prev, parent, 0) end)
    send(prev, {:set_next, next})
    create_ring(next, n - 1, parent)
  end

  defp ring_node(next, parent, received) do
    receive do
      {:set_next, new_next} ->
        ring_node(new_next, parent, received)
      {:token, 0} ->
        send(parent, {:done, received + 1})
      {:token, hops} ->
        send(next, {:token, hops - 1})
        ring_node(next, parent, received + 1)
    end
  end
end

ThreadRing.start()
