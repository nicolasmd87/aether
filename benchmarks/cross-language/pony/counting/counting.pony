// Pony Counting Actor Benchmark (Savina-style)
// Single actor counting incoming messages

use "time"

actor Counter
  let _main: Main
  var _count: U64 = 0
  let _expected: U64

  new create(main: Main, expected: U64) =>
    _main = main
    _expected = expected

  be increment() =>
    _count = _count + 1
    if _count >= _expected then
      _main.done(_count)
    end

actor Main
  let _env: Env
  var _start: U64 = 0
  var _messages: U64 = 10_000_000

  new create(env: Env) =>
    _env = env

    // Read BENCHMARK_MESSAGES from environment
    for v in env.vars.values() do
      if v.contains("BENCHMARK_MESSAGES=") then
        try
          let parts = v.split("=")
          let val_str = parts(1)?
          _messages = val_str.u64()?
        end
      end
    end

    _env.out.print("=== Pony Counting Actor Benchmark ===")
    _env.out.print("Messages: " + _messages.string() + "\n")
    _start = Time.nanos()

    let counter = Counter(this, _messages)

    // Send all messages
    var i: U64 = 0
    while i < _messages do
      counter.increment()
      i = i + 1
    end

  be done(count: U64) =>
    let finish = Time.nanos()
    let elapsed_ns = finish - _start
    let ns_per_msg = elapsed_ns.f64() / _messages.f64()
    let throughput = 1_000_000_000.0 / ns_per_msg
    let cycles_per_msg = ns_per_msg * 3.0

    _env.out.print("Cycles/msg:     " + cycles_per_msg.string())
    _env.out.print("Throughput:     " + (throughput / 1_000_000.0).string() + " M msg/sec")
