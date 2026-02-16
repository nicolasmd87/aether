// Pony Thread Ring Benchmark (Savina-style)
// N actors arranged in a ring, passing a token M times

use "time"
use "collections"

actor RingNode
  let _id: USize
  let _main: Main
  var _next: (RingNode | None) = None

  new create(id: USize, main: Main) =>
    _id = id
    _main = main

  be set_next(next: RingNode) =>
    _next = next

  be pass(token: U64) =>
    if token == 0 then
      _main.done()
    else
      match _next
      | let n: RingNode => n.pass(token - 1)
      end
    end

actor Main
  let _env: Env
  var _start: U64 = 0
  let _ring_size: USize = 100
  var _num_hops: U64 = 1_000_000

  new create(env: Env) =>
    _env = env

    // Read BENCHMARK_MESSAGES from environment
    for v in env.vars.values() do
      if v.contains("BENCHMARK_MESSAGES=") then
        try
          let parts = v.split("=")
          let val_str = parts(1)?
          _num_hops = val_str.u64()?
        end
      end
    end

    _env.out.print("=== Pony Thread Ring Benchmark ===")
    _env.out.print("Ring size: " + _ring_size.string() + ", Hops: " + _num_hops.string() + "\n")

    // Create ring of actors
    let nodes = Array[RingNode](_ring_size)
    var i: USize = 0
    while i < _ring_size do
      nodes.push(RingNode(i, this))
      i = i + 1
    end

    // Connect ring
    i = 0
    while i < _ring_size do
      try
        let curr = nodes(i)?
        let next_idx = (i + 1) % _ring_size
        let next_node = nodes(next_idx)?
        curr.set_next(next_node)
      end
      i = i + 1
    end

    _start = Time.nanos()

    // Start the token passing
    try
      nodes(0)?.pass(_num_hops)
    end

  be done() =>
    let finish = Time.nanos()
    let total_messages = _num_hops + 1
    let elapsed_ns = finish - _start
    let ns_per_msg = elapsed_ns.f64() / total_messages.f64()
    let throughput = 1_000_000_000.0 / ns_per_msg
    let cycles_per_msg = ns_per_msg * 3.0

    _env.out.print("Cycles/msg:     " + cycles_per_msg.string())
    _env.out.print("Throughput:     " + (throughput / 1_000_000.0).string() + " M msg/sec")
