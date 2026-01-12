-module(skynet).
-export([start/0, skynet/4]).

-define(BASE, 10).

skynet(Level, Num, Parent, Count) ->
    if
        Level >= 4 ->
            Parent ! {result, Num + Count};
        true ->
            NewLevel = Level + 1,
            spawn_children(NewLevel, Num, self(), Count, ?BASE, 0)
    end.

spawn_children(_, _, Parent, _Count, 0, Sum) ->
    Parent ! {result, Sum};
spawn_children(Level, Num, Parent, Count, Remaining, Sum) ->
    Child = Remaining - 1,
    spawn(?MODULE, skynet, [Level, Num * ?BASE + Child, Parent, Count]),
    receive
        {result, ChildSum} ->
            spawn_children(Level, Num, Parent, Count, Child, Sum + ChildSum)
    end.

collect_results(0, Sum) ->
    Sum;
collect_results(Remaining, Sum) ->
    receive
        {result, ChildSum} ->
            collect_results(Remaining - 1, Sum + ChildSum)
    end.

start() ->
    io:format("=== Erlang Skynet Benchmark ===~n"),
    io:format("Base: ~p~n", [?BASE]),
    io:format("Depth: 4~n~n", []),

    Count = 1000,
    ExpectedActors = 1 + ?BASE + (?BASE * ?BASE) + (?BASE * ?BASE * ?BASE),
    TotalMessages = ExpectedActors * Count,

    io:format("Expected actors: ~p~n", [ExpectedActors]),
    io:format("Messages per actor: ~p~n", [Count]),
    io:format("Total messages: ~p~n~n", [TotalMessages]),

    % Start timer
    Start = erlang:system_time(nanosecond),

    % Spawn root actors
    Parent = self(),
    lists:foreach(fun(I) ->
        spawn(?MODULE, skynet, [0, I, Parent, Count])
    end, lists:seq(0, ?BASE - 1)),

    % Collect results
    _Sum = collect_results(?BASE, 0),

    % Calculate metrics
    End = erlang:system_time(nanosecond),
    ElapsedNs = End - Start,
    ElapsedSec = ElapsedNs / 1000000000.0,

    % Estimate cycles (assuming 3GHz)
    TotalCycles = ElapsedNs * 3.0,
    CyclesPerMsg = TotalCycles / TotalMessages,
    MsgPerSec = TotalMessages / ElapsedSec,

    io:format("Cycles/msg:     ~.2f~n", [CyclesPerMsg]),
    io:format("Throughput:     ~.0f M msg/sec~n", [MsgPerSec / 1000000]),

    halt(0).
