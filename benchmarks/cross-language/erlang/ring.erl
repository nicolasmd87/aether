-module(ring).
-export([start/0, ring_actor/2]).

-define(RING_SIZE, 100).
-define(ROUNDS, 100000).
-define(MESSAGES, ?RING_SIZE * ?ROUNDS).

ring_actor(Next, Main) ->
    receive
        {token, 0} ->
            Main ! done;
        {token, Count} ->
            Next ! {token, Count - 1},
            ring_actor(Next, Main)
    end.

create_ring(0, First, Main) ->
    First ! {next, self(), Main},
    receive
        {next, Next, Main} -> Next
    end;
create_ring(N, First, Main) ->
    Next = create_ring(N - 1, First, Main),
    spawn(?MODULE, ring_actor, [Next, Main]).

start() ->
    io:format("=== Erlang Ring Benchmark ===~n"),
    io:format("Ring size: ~p~n", [?RING_SIZE]),
    io:format("Rounds: ~p~n", [?ROUNDS]),
    io:format("Total messages: ~p~n~n", [?MESSAGES]),

    Main = self(),

    % Create ring
    First = spawn(fun() ->
        receive
            {next, Next, M} ->
                ring_actor(Next, M)
        end
    end),

    _Last = create_ring(?RING_SIZE - 1, First, Main),

    % Start timer
    Start = erlang:system_time(nanosecond),

    % Send token
    First ! {token, ?MESSAGES},

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
