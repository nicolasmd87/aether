-module(counting).
-export([start/0]).

get_messages() ->
    case os:getenv("BENCHMARK_MESSAGES") of
        false -> 100000;
        Val -> list_to_integer(Val)
    end.

start() ->
    Messages = get_messages(),
    io:format("=== Erlang Counting Actor Benchmark ===~n"),
    io:format("Messages: ~p~n~n", [Messages]),

    Parent = self(),
    Counter = spawn(fun() -> counter(0, Parent) end),

    Start = erlang:monotonic_time(nanosecond),

    % Send all messages
    send_messages(Counter, Messages),

    % Signal done and wait for count
    Counter ! {done, self()},
    receive
        {count, Count} ->
            End = erlang:monotonic_time(nanosecond),
            ElapsedNs = End - Start,
            ElapsedSec = ElapsedNs / 1000000000.0,

            if
                Count =/= Messages ->
                    io:format("VALIDATION FAILED: expected ~p, got ~p~n", [Messages, Count]);
                true -> ok
            end,

            CyclesPerMsg = (ElapsedNs * 3.0) / Messages,
            MsgPerSec = Messages / ElapsedSec,
            io:format("Cycles/msg:     ~.2f~n", [CyclesPerMsg]),
            io:format("Throughput:     ~.2f M msg/sec~n", [MsgPerSec / 1000000])
    after 60000 ->
        io:format("Timeout!~n"),
        halt(1)
    end,

    halt(0).

send_messages(_Counter, 0) -> ok;
send_messages(Counter, N) ->
    Counter ! increment,
    send_messages(Counter, N - 1).

counter(Count, Parent) ->
    receive
        increment ->
            counter(Count + 1, Parent);
        {done, From} ->
            From ! {count, Count}
    end.
