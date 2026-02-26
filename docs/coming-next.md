
[TODO] 1. Formalize actor setup phase
------------------------------------------------------------------------
Issue: Actors are active immediately after spawn. No formal boundary
       between wiring (setting refs) and running (receiving messages).
Fix:   Spawn actors with active=0. Activate on first message send.
       Creates explicit setup -> active phase boundary.
       Longer term: support spawn with initial state args (Erlang style).
Files: runtime/scheduler/multicore_scheduler.c,
       compiler/codegen/codegen_actor.c, codegen_expr.c


[TODO] 2. Fix benchmark fairness
------------------------------------------------------------------------
Issue: C++ benchmarks use std::mutex while Aether uses lock-free SPSC.
       Not apples-to-apples.
Fix:   Add lock-free C++ benchmark variants alongside mutex ones.
       Label clearly. Consider adding CAF-based implementation.
Files: benchmarks/cross-language/cpp/*.cpp, benchmarks/cross-language/README.md

