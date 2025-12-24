# Comparing Aether to Erlang and Go

## Why This Comparison Matters

Look, we're not trying to reinvent the wheel here. Erlang's been doing lightweight concurrency since the 80s, and Go's goroutines have proven the M:N model works in practice. The question isn't "can we do better?" but rather "what can we learn, and what makes sense for a language that compiles to C?"

## Erlang (BEAM VM)

### What They Do

Erlang runs on the BEAM virtual machine, and honestly, their concurrency story is brilliant:

- **Processes**: Super lightweight (starts at ~2.6KB per process)
- **Preemptive scheduling**: Each process gets a fixed number of "reductions" (think: instructions) before the scheduler forcibly switches to another process
- **Reduction counting**: The VM literally counts operations. After ~2000 reductions, you're preempted whether you like it or not
- **Garbage collection**: Per-process GC. When a process crashes, its memory just... goes away. Beautiful.
- **Message passing**: True isolation - messages are copied between processes

### The Reduction System

Here's what's clever: Erlang doesn't trust your code to yield. Every function call, every operation increments a reduction counter. Hit the limit? Boom, you're suspended mid-function, and another process runs. This means:

1. No single process can hog the CPU
2. Soft real-time behavior (predictable latency)
3. You can't accidentally write a tight loop that freezes everything

**The Cost**: This requires a VM. You're not generating native code - you're running bytecode with overhead on every operation.

### Why We Can't Just Copy This

Aether compiles to C. We don't have a VM to count reductions. Our options:

1. **Inject reduction counters everywhere** (adds overhead to every loop, every call)
2. **Cooperative yielding** (trust the programmer - what we're doing now)
3. **OS threads** (too heavy, we've proven this with pthread baseline)

### What We're Taking From Erlang

- **The insight that actors can be tiny structs** - This is huge. Their 2.6KB processes inspired our 128-byte actors.
- **Message passing as the primary abstraction** - Not shared memory.
- **Process isolation** - Each actor owns its state, period.

### What We're Doing Differently

- **Cooperative instead of preemptive** - We're betting on compiler analysis and programmer discipline
- **Even lighter weight** - 128 bytes vs 2.6KB (though Erlang's includes more features)
- **Native code** - We compile to C, they run bytecode
- **Explicit state machines** - Our actors are explicit C structs, not hidden VM internals

## Go (Goroutines)

### What They Do

Go's concurrency is probably the most successful modern take on lightweight threads:

- **Goroutines**: Start at ~2KB (segmented stacks that grow as needed)
- **M:N scheduling**: M goroutines scheduled on N OS threads
- **Work-stealing scheduler**: Idle threads steal work from busy threads' queues
- **Runtime scheduler**: Built into the Go runtime, not the OS
- **Channel-based communication**: `chan` types for message passing

### The Work-Stealing Algorithm

This is where Go really shines:

```
Each OS thread (M) has:
- A local run queue of goroutines (P)
- When queue is empty, steal from another thread's tail
- When goroutine blocks on I/O, park it until I/O completes
- Goroutines are cheap to create/destroy
```

The work-stealing part is clever: busy threads fill their local queues from the front, idle threads steal from the back. This minimizes lock contention because most operations are on the local queue (no locking needed).

### Why It Works

Go's runtime is tightly integrated with the language:

- Compiler knows which calls can block
- Runtime has a netpoller (epoll/kqueue) for async I/O
- Goroutines can block on channels, and the runtime handles it
- Stack management is automatic (segmented stacks, then contiguous with copying)

Aether compiles to C and links against a small runtime (~few KB). We want:

- Small binaries
- Easy interop with C libraries
- No heavy runtime to port to new platforms

### What We're Taking From Go

- **Work-stealing is the right answer for multi-core** - This is proven tech. Our Experiment 03 is directly inspired by Go.
- **The insight that lightweight tasks + smart scheduling beats OS threads** - They proved it at scale.
- **Channels as a primitive** (on our roadmap) - Better than raw message queues.

### What We're Doing Differently

- **Even lighter goroutines** - Our state machine actors are 128 bytes, Go's goroutines are 2KB
- **No automatic stack growth** - We're doing state machines, not stacks
- **Explicit actor model** - Go has goroutines + channels, we have actors + mailboxes
- **C interop focus** - Go's cgo is notoriously slow, we're native C

## Our Approach: Learning From Both

Here's what we're actually building, and why:

### Phase 1: State Machine Actors (Current)

**What**: Actors as C structs, cooperative scheduling

**Learned from**:
- Erlang: Actors can be tiny data structures
- Go: You don't need OS threads

**Why it works**:
- 128 bytes per actor (lighter than both!)
- 125M messages/second (faster than either on single core)
- No VM or heavy runtime needed

**The catch**:
- No preemption (Erlang has this)
- Single-core only (Go beats us here)
- Blocking calls = disaster (both handle this better)

### Phase 2: Work-Stealing (Planned)

**What**: Go-style M:N scheduler for multi-core

**Learned from**:
- Go: The entire work-stealing algorithm
- Erlang: Per-scheduler run queues

**Why we need it**:
- State machines are fast but single-core
- Work-stealing is proven to scale

**What we're adding**:
- Combine with state machine actors (Go uses stacks)
- Smaller overhead (no stack management)

### Phase 3: Hybrid Model (Long-term)

**What**: Mix state machines + pthreads

**Why neither does this**:
- Erlang: Everything's a BEAM process (consistent but inflexible)
- Go: Everything's a goroutine (same deal)

**Our reasoning**:
- Some actors need blocking I/O (file operations, legacy C libraries)
- Some actors need maximum throughput (state machines)
- Why force one model when you can have both?

**The risk**:
- Complexity. Two models = two sets of bugs.
- But C interop is a first-class requirement for us.


We're saying: **What if we compile the actor model down to simple C code?**

- Actors = C structs (zero runtime cost)
- Message passing = function calls or queues (explicit)
- Scheduling = array iteration or work-stealing (simple)

The bet is that by **giving up** preemption and automatic memory management, we can **gain** simplicity, speed, and C interop.

We're not better at everything. But for **high-throughput, C-interop, minimal-runtime scenarios**, we're competitive.

## What This Means for Aether

We're not trying to replace Erlang or Go. They're solving different problems:

- **Erlang**: Distributed systems with extreme fault tolerance (telecom)
- **Go**: Network services with good balance of simplicity and performance (web servers)
- **Aether**: Systems programming with actor concurrency (game servers, embedded, real-time)

Our niche is:
- Need C-level performance and interop
- Want actor-style concurrency
- Can accept cooperative scheduling
- Don't need/want a large runtime

If you're building the next WhatsApp backend, use Erlang. If you're building a microservice, use Go. If you're building a game engine or embedded system and want actors, try Aether.

## Future Work

Things we need to prove:

1. **Work-stealing works as well as we think** - Experiment 03 will tell us
2. **Hybrid model is practical** - Mixing state machines + pthreads might be a nightmare
3. **Non-blocking I/O is fast enough** - We need to benchmark this
4. **Compiler complexity is manageable** - State machine codegen is hard

If any of these fail, we'll pivot. That's the point of experiments.

## Conclusion

We're standing on the shoulders of giants. Erlang taught us actors can be lightweight. Go taught us work-stealing works. We're asking: **can we compile this model down to bare C and get even lighter weight?**

Early results say yes. But we're honest about the trade-offs: no preemption, cooperative scheduling only, more complexity in the compiler.

Time will tell.
