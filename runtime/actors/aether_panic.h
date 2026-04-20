#ifndef AETHER_PANIC_H
#define AETHER_PANIC_H

// Go-style panic / recover for Aether actors.
//
// Surface:
//   - User code:   panic("reason") unwinds via aether_panic().
//                  try { ... } catch e { ... } codegens to a push + sigsetjmp
//                  pair; the catch body runs with `e` bound to the reason.
//   - Scheduler:   wraps each actor step() in a sigsetjmp barrier using the
//                  same push/pop primitives. On longjmp, the actor is marked
//                  dead and the on_actor_death hook fires. Other actors
//                  keep running.
//   - Signals:     AETHER_CATCH_SIGNALS=1 at process start installs
//                  SIGSEGV/SIGFPE/SIGBUS handlers that convert native faults
//                  into aether_panic(). Off by default because a native
//                  fault mid-enqueue can leave runtime state inconsistent.
//
// Nested try is supported via a per-thread stack of frames. Each try / each
// scheduler step() wrapper pushes one frame on entry and pops on normal exit
// or after catching.
//
// The scheduler and codegen both construct the pattern manually at the call
// site — sigsetjmp must lexically enclose the work being guarded, so we
// can't wrap it in a helper function. aether_try_push() returns a pointer
// to the newly-allocated frame; the caller then calls sigsetjmp(frame->buf, 1).

#include <setjmp.h>
#include <stdatomic.h>
#include "../utils/aether_compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

// Windows lacks POSIX sigjmp_buf / sigsetjmp / siglongjmp. Fall back to
// the unsafe-for-signal-handlers variants, which is fine because Windows
// doesn't deliver signals to arbitrary threads like POSIX does — the
// signal-mask semantics the `sig*` family preserves aren't meaningful on
// Win32. Callers that used sigsetjmp(buf, 1) should use AETHER_SIGSETJMP
// so the second argument is dropped on Windows.
#ifdef _WIN32
  typedef jmp_buf aether_sigjmp_buf;
  #define AETHER_SIGSETJMP(buf, savemask) setjmp(buf)
  #define AETHER_SIGLONGJMP(buf, val)     longjmp((buf), (val))
#else
  typedef sigjmp_buf aether_sigjmp_buf;
  #define AETHER_SIGSETJMP(buf, savemask) sigsetjmp((buf), (savemask))
  #define AETHER_SIGLONGJMP(buf, val)     siglongjmp((buf), (val))
#endif

// Maximum nesting depth of try/catch (and scheduler step barriers) per
// thread. 32 covers any realistic handler; the limit lets the stack live
// in TLS without dynamic allocation.
#define AETHER_PANIC_MAX_DEPTH 32

typedef struct AetherJmpFrame {
    aether_sigjmp_buf buf;
    const char* reason;   // written by aether_panic() just before siglongjmp
} AetherJmpFrame;

// Push a new frame onto the current thread's stack and return it. The
// caller is expected to immediately call sigsetjmp(frame->buf, 1). If
// sigsetjmp returns non-zero, a panic has unwound to this frame; the
// reason is in frame->reason.
AetherJmpFrame* aether_try_push(void);

// Pop the innermost frame. Call this on both normal exit (no panic) and
// after handling a caught panic — the frame is consumed either way.
void aether_try_pop(void);

// Innermost live frame on this thread, or NULL. Used by aether_panic() to
// find its target, and by signal handlers.
AetherJmpFrame* aether_current_frame(void);

// Current nesting depth. Exposed for tests.
int aether_try_depth(void);

// User-visible panic entry point. Siglongjmps to the innermost frame with
// reason attached. If no frame exists, prints to stderr and aborts
// (protects non-actor threads that haven't set up a barrier).
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noreturn))
#endif
void aether_panic(const char* reason);

// Install SIGSEGV/SIGFPE/SIGBUS handlers that convert native faults into
// panics. No-op unless AETHER_CATCH_SIGNALS=1 is set in the environment.
// Call once at process init.
void aether_panic_install_signal_handlers(void);

// Death notification. Fn is invoked with (actor_id, reason) after an actor
// step() unwinds. NULL clears. Single global slot — if you need fan-out,
// dispatch yourself.
typedef void (*AetherDeathHook)(int actor_id, const char* reason);
void aether_set_on_actor_death(AetherDeathHook fn);

// Called by the scheduler after catching a panic. Fires the on_actor_death
// hook if one is set. Separated from the push/pop primitives so scheduler
// code can decide its own ordering (e.g. mark actor dead first, then fire
// the hook).
void aether_fire_death_hook(int actor_id, const char* reason);

// TLS: 1 while executing inside a scheduler-wrapped step, 0 otherwise.
// Signal handlers check this before deciding whether to recover or let
// the signal propagate with SIG_DFL.
extern AETHER_TLS int g_aether_in_actor_step;
extern AETHER_TLS int g_aether_current_actor_id;

#ifdef __cplusplus
}
#endif

#endif
