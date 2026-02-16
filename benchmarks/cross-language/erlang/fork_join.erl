-module(fork_join).
-export([start/0]).

-define(NUM_WORKERS, 8).

get_messages_per_worker() ->
    case os:getenv("BENCHMARK_MESSAGES") of
        false -> 100000;
        Val -> list_to_integer(Val) div ?NUM_WORKERS
    end.

start() ->
    MessagesPerWorker = get_messages_per_worker(),
    io:format("=== Erlang Fork-Join Throughput Benchmark ===~n"),
    Total = ?NUM_WORKERS * MessagesPerWorker,
    io:format("Workers: ~p, Messages: ~p~n~n", [?NUM_WORKERS, Total]),

    Parent = self(),

    % Create workers
    Workers = [spawn(fun() -> worker(0, 0, Parent) end) || _ <- lists:seq(1, ?NUM_WORKERS)],
    WorkerArray = list_to_tuple(Workers),

    Start = erlang:monotonic_time(nanosecond),

    % Send messages round-robin
    send_work(WorkerArray, 0, Total),

    % Signal workers to report
    [W ! {done, self()} || W <- Workers],

    % Collect results
    TotalProcessed = collect_results(?NUM_WORKERS, 0),

    End = erlang:monotonic_time(nanosecond),
    ElapsedNs = End - Start,
    ElapsedSec = ElapsedNs / 1000000000.0,

    if
        TotalProcessed =/= Total ->
            io:format("VALIDATION FAILED: expected ~p, got ~p~n", [Total, TotalProcessed]);
        true -> ok
    end,

    CyclesPerMsg = (ElapsedNs * 3.0) / Total,
    MsgPerSec = Total / ElapsedSec,
    io:format("Cycles/msg:     ~.2f~n", [CyclesPerMsg]),
    io:format("Throughput:     ~.2f M msg/sec~n", [MsgPerSec / 1000000]),

    halt(0).

send_work(_Workers, I, Total) when I >= Total -> ok;
send_work(Workers, I, Total) ->
    WorkerIdx = (I rem ?NUM_WORKERS) + 1,
    element(WorkerIdx, Workers) ! {work, I},
    send_work(Workers, I + 1, Total).

worker(Processed, Sum, Parent) ->
    receive
        {work, Value} ->
            worker(Processed + 1, Sum + Value, Parent);
        {done, From} ->
            From ! {result, Processed}
    end.

collect_results(0, Acc) -> Acc;
collect_results(N, Acc) ->
    receive
        {result, Count} ->
            collect_results(N - 1, Acc + Count)
    end.
