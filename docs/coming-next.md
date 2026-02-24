

[TODO] 2. Extract module loading from typechecker into orchestrator
------------------------------------------------------------------------
Issue: Module loading lives inside typechecker.c (and is copy-pasted in
       codegen.c). No caching, no dependency graph, no cycle detection.
       Modules re-parsed on every import.
Fix:   Create orchestration phase in aetherc.c that:
       - Scans imports, builds dependency graph (aether_module.c infra exists)
       - Topologically sorts modules
       - Caches parsed ASTs in global_module_registry
       - Detects circular imports
       Typechecker and codegen use module_find() instead of loading.
Files: compiler/aetherc.c, compiler/analysis/typechecker.c,
       compiler/codegen/codegen.c, compiler/aether_module.c


[TODO] 3. Formalize actor setup phase
------------------------------------------------------------------------
Issue: Actors are active immediately after spawn. No formal boundary
       between wiring (setting refs) and running (receiving messages).
Fix:   Spawn actors with active=0. Activate on first message send.
       Creates explicit setup -> active phase boundary.
       Longer term: support spawn with initial state args (Erlang style).
Files: runtime/scheduler/multicore_scheduler.c,
       compiler/codegen/codegen_actor.c, codegen_expr.c


[TODO] 4. Fix benchmark fairness
------------------------------------------------------------------------
Issue: C++ benchmarks use std::mutex while Aether uses lock-free SPSC.
       Not apples-to-apples.
Fix:   Add lock-free C++ benchmark variants alongside mutex ones.
       Label clearly. Consider adding CAF-based implementation.
Files: benchmarks/cross-language/cpp/*.cpp, benchmarks/cross-language/README.md


[DONE] 5. Harden work-stealing / SPSC race
------------------------------------------------------------------------
Issue: Work-stealing can race with same-core mailbox write. Window is
       ~nanoseconds but violates C memory model. The re-route check at
       the top of the incoming_queue drain reads assigned_core as a
       plain int (data race), and the aggressive drain (up to 128 step
       calls) creates a TOCTOU window large enough for the thief to lap
       the victim and process the actor concurrently.
Fix:   Made assigned_core atomic_int. Added assigned_core guard inside
       aggressive drain loop (break on steal) and re-check + re-route
       before mailbox_send. Zero perf impact: atomic_load(relaxed) ==
       plain load on x86/ARM. Processing loop untouched.
Files: runtime/scheduler/multicore_scheduler.h,
       runtime/scheduler/multicore_scheduler.c

