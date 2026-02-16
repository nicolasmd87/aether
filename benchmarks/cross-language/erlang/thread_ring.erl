-module(thread_ring).
-export([start/0]).

-define(RING_SIZE, 100).

get_num_hops() ->
    case os:getenv("BENCHMARK_MESSAGES") of
        false -> 100000;
        Val -> list_to_integer(Val)
    end.

start() ->
    NumHops = get_num_hops(),
    io:format("=== Erlang Thread Ring Benchmark ===~n"),
    io:format("Ring size: ~p, Hops: ~p~n~n", [?RING_SIZE, NumHops]),

    % Create ring of processes
    Parent = self(),
    First = spawn(fun() -> ring_node(undefined, Parent, 0) end),
    Last = create_ring(First, ?RING_SIZE - 1, Parent),

    % Close the ring: Last -> First (not First -> Last!)
    Last ! {set_next, First},

    % Start timer (monotonic_time is correct for benchmarks)
    Start = erlang:monotonic_time(nanosecond),

    % Send initial token
    First ! {token, NumHops},

    % Wait for completion
    receive
        {done, TotalReceived} ->
            End = erlang:monotonic_time(nanosecond),
            ElapsedNs = End - Start,
            ElapsedSec = ElapsedNs / 1000000000.0,

            % Each node receives ~(NumHops + 1) / RING_SIZE messages
            ExpectedPerNode = (NumHops + 1) div ?RING_SIZE + 1,
            if
                TotalReceived < ExpectedPerNode - 10; TotalReceived > ExpectedPerNode + 10 ->
                    io:format("VALIDATION WARNING: expected ~p per node, got ~p~n", [ExpectedPerNode, TotalReceived]);
                true -> ok
            end,

            CyclesPerMsg = (ElapsedNs * 3.0) / (NumHops + 1),
            MsgPerSec = (NumHops + 1) / ElapsedSec,
            io:format("Cycles/msg:     ~.2f~n", [CyclesPerMsg]),
            io:format("Throughput:     ~.2f M msg/sec~n", [MsgPerSec / 1000000])
    after 60000 ->
        io:format("Timeout!~n"),
        halt(1)
    end,

    halt(0).

create_ring(Prev, 0, _Parent) ->
    Prev;
create_ring(Prev, N, Parent) ->
    Next = spawn(fun() -> ring_node(Prev, Parent, 0) end),
    Prev ! {set_next, Next},
    create_ring(Next, N - 1, Parent).

ring_node(Next, Parent, Received) ->
    receive
        {set_next, NewNext} ->
            ring_node(NewNext, Parent, Received);
        {token, 0} ->
            % Token exhausted, report to parent
            Parent ! {done, Received + 1};
        {token, Hops} ->
            Next ! {token, Hops - 1},
            ring_node(Next, Parent, Received + 1)
    end.
