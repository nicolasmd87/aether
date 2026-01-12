-module(ping_pong).
-export([start/0, ping/2, pong/2]).

-define(MESSAGES, 10000000).

ping(Pong, 0) ->
    Pong ! done,
    done;
ping(Pong, N) ->
    Pong ! {ping, self()},
    receive
        pong -> ping(Pong, N - 1)
    end.

pong(0) ->
    receive
        done -> done
    end;
pong(N) ->
    receive
        {ping, Ping} ->
            Ping ! pong,
            pong(N - 1)
    end.

start() ->
    io:format("=== Erlang Ping-Pong Benchmark ===~n"),
    io:format("Messages: ~p~n~n", [?MESSAGES]),

    % Spawn processes
    Pong = spawn(?MODULE, pong, [?MESSAGES]),

    % Start timer
    Start = erlang:system_time(nanosecond),

    % Run ping-pong
    spawn(?MODULE, ping, [Pong, ?MESSAGES]),

    % Wait for completion
    receive
        done -> ok
    after 60000 ->
        io:format("Timeout!~n"),
        halt(1)
    end,

    % Calculate metrics
    End = erlang:system_time(nanosecond),
    ElapsedNs = End - Start,
    ElapsedSec = ElapsedNs / 1000000000.0,

    % Estimate cycles (assuming 3GHz)
    TotalCycles = ElapsedNs * 3.0,
    CyclesPerMsg = TotalCycles / ?MESSAGES,
    MsgPerSec = ?MESSAGES / ElapsedSec,

    io:format("Cycles/msg:     ~.2f~n", [CyclesPerMsg]),
    io:format("Throughput:     ~.0f M msg/sec~n", [MsgPerSec / 1000000]),

    halt(0).
