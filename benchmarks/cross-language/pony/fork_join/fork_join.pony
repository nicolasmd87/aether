// Pony Fork-Join Benchmark (Savina-style)
// K worker actors, M messages distributed round-robin

use "time"
use "collections"

actor Worker
  let _id: USize
  let _main: Main
  var _processed: U64 = 0
  let _expected: U64

  new create(id: USize, main: Main, expected: U64) =>
    _id = id
    _main = main
    _expected = expected

  be work(value: U64) =>
    _processed = _processed + 1
    if _processed >= _expected then
      _main.worker_done(_id, _processed)
    end

actor Main
  let _env: Env
  var _start: U64 = 0
  let _num_workers: USize = 8
  var _total_messages: U64 = 8_000_000
  var _workers_done: USize = 0
  var _total_processed: U64 = 0

  new create(env: Env) =>
    _env = env

    // Read BENCHMARK_MESSAGES from environment
    for v in env.vars.values() do
      if v.contains("BENCHMARK_MESSAGES=") then
        try
          let parts = v.split("=")
          let val_str = parts(1)?
          _total_messages = val_str.u64()?
        end
      end
    end

    let messages_per_worker = _total_messages / _num_workers.u64()

    _env.out.print("=== Pony Fork-Join Throughput Benchmark ===")
    _env.out.print("Workers: " + _num_workers.string() + ", Messages: " + _total_messages.string() + "\n")

    // Create workers
    let workers = Array[Worker](_num_workers)
    var i: USize = 0
    while i < _num_workers do
      workers.push(Worker(i, this, messages_per_worker))
      i = i + 1
    end

    _start = Time.nanos()

    // Send messages round-robin
    var msg: U64 = 0
    while msg < _total_messages do
      try
        let worker_idx = (msg.usize() % _num_workers)
        workers(worker_idx)?.work(msg)
      end
      msg = msg + 1
    end

  be worker_done(id: USize, processed: U64) =>
    _workers_done = _workers_done + 1
    _total_processed = _total_processed + processed

    if _workers_done >= _num_workers then
      let finish = Time.nanos()
      let elapsed_ns = finish - _start
      let ns_per_msg = elapsed_ns.f64() / _total_messages.f64()
      let throughput = 1_000_000_000.0 / ns_per_msg
      let cycles_per_msg = ns_per_msg * 3.0

      _env.out.print("Cycles/msg:     " + cycles_per_msg.string())
      _env.out.print("Throughput:     " + (throughput / 1_000_000.0).string() + " M msg/sec")
    end
