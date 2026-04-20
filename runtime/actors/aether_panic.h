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

// sigjmp_buf / sigsetjmp / siglongjmp are POSIX, not C. Several of our
// targets don't expose them:
//
//   - Windows: lacks them entirely; Win32 signals don't have the
//     arbitrary-thread delivery semantics the signal-mask variants
//     protect against.
//   - Emscripten wasm32: supports plain setjmp/longjmp via
//     -enable-emscripten-sjlj but not sigsetjmp; compiling it triggers
//     a backend "relocations for function or section offsets are only
//     supported in metadata sections" error.
//   - Freestanding / bare-metal (e.g. arm-none-eabi newlib-nano under
//     -ffreestanding): only the minimal C setjmp interface exists;
//     sigjmp_buf is not declared at all.
//
// All three fall back to the signal-mask-unaware variants. Callers use
// AETHER_SIGSETJMP / AETHER_SIGLONGJMP so the signal-mask argument is
// dropped on targets that don't accept it.
#if defined(_WIN32) || defined(__EMSCRIPTEN__) || (defined(__STDC_HOSTED__) && __STDC_HOSTED__ == 0)
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
