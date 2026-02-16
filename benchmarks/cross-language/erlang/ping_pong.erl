-module(ping_pong).
-export([start/0, ping/3, pong/2]).

get_messages() ->
    case os:getenv("BENCHMARK_MESSAGES") of
        false -> 100000;
        Val -> list_to_integer(Val)
    end.

ping(Pong, N, _I) when N =< 0 ->
    Pong ! done,
    done;
ping(Pong, N, I) ->
    Pong ! {ping, self(), I},
    receive
        {pong, Value} ->
            % Validate received value matches what was sent
            if
                Value =/= I ->
                    io:format(standard_error, "Ping validation error: expected ~p, got ~p~n", [I, Value]);
                true -> ok
            end,
            ping(Pong, N - 1, I + 1)
    end.

pong(Expected, Messages) when Expected >= Messages ->
    receive
        done -> done
    end;
pong(Expected, Messages) ->
    receive
        {ping, Ping, Value} ->
            % Validate received value matches expected sequence
            if
                Value =/= Expected ->
                    io:format(standard_error, "Pong validation error: expected ~p, got ~p~n", [Expected, Value]);
                true -> ok
            end,
            % Echo back the EXACT value received
            Ping ! {pong, Value},
            pong(Expected + 1, Messages)
    end.

start() ->
    Messages = get_messages(),
    io:format("=== Erlang Ping-Pong Benchmark ===~n"),
    io:format("Messages: ~p~n~n", [Messages]),

    % Spawn processes
    Parent = self(),
    Pong = spawn(fun() -> pong(0, Messages) end),

    % Start timer (monotonic_time is correct for benchmarks per Erlang docs)
    Start = erlang:monotonic_time(nanosecond),

    % Run ping-pong - spawn returns the PID, we need to link it to receive done
    spawn_link(fun() ->
        ping(Pong, Messages, 0),
        Parent ! done
    end),

    % Wait for completion
    receive
        done -> ok
    after 60000 ->
        io:format("Timeout!~n"),
        halt(1)
    end,

    % Calculate metrics
    End = erlang:monotonic_time(nanosecond),
    ElapsedNs = End - Start,
    ElapsedSec = ElapsedNs / 1000000000.0,

    % Estimate cycles (assuming 3GHz)
    TotalCycles = ElapsedNs * 3.0,
    CyclesPerMsg = TotalCycles / Messages,
    MsgPerSec = Messages / ElapsedSec,

    io:format("Cycles/msg:     ~.2f~n", [CyclesPerMsg]),
    io:format("Throughput:     ~.2f M msg/sec~n", [MsgPerSec / 1000000]),

    halt(0).
