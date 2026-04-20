#include "aether_panic.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

// ---------------------------------------------------------------------------
// Per-thread jmp frame stack
// ---------------------------------------------------------------------------

typedef struct {
    AetherJmpFrame frames[AETHER_PANIC_MAX_DEPTH];
    int depth;  // number of frames currently live; innermost is frames[depth-1]
} AetherJmpStack;

static AETHER_TLS AetherJmpStack tls_stack = { .depth = 0 };

AETHER_TLS int g_aether_in_actor_step = 0;
AETHER_TLS int g_aether_current_actor_id = -1;

static AetherDeathHook death_hook = NULL;

AetherJmpFrame* aether_try_push(void) {
    if (tls_stack.depth >= AETHER_PANIC_MAX_DEPTH) {
        fprintf(stderr, "aether: try/catch nesting exceeded %d — aborting\n",
                AETHER_PANIC_MAX_DEPTH);
        abort();
    }
    AetherJmpFrame* f = &tls_stack.frames[tls_stack.depth++];
    f->reason = NULL;
    return f;
}

void aether_try_pop(void) {
    if (tls_stack.depth <= 0) {
        // Defensive: popping an empty stack means codegen/runtime mismatch.
        fprintf(stderr, "aether: aether_try_pop on empty stack\n");
        abort();
    }
    tls_stack.depth--;
}

AetherJmpFrame* aether_current_frame(void) {
    if (tls_stack.depth == 0) return NULL;
    return &tls_stack.frames[tls_stack.depth - 1];
}

int aether_try_depth(void) {
    return tls_stack.depth;
}

// ---------------------------------------------------------------------------
// Panic entry point
// ---------------------------------------------------------------------------

void aether_panic(const char* reason) {
    if (!reason) reason = "panic: (null)";

    AetherJmpFrame* f = aether_current_frame();
    if (f) {
        f->reason = reason;
        AETHER_SIGLONGJMP(f->buf, 1);
        // unreachable
    }

    // No user-level try/catch. Print to stderr so the caller sees *something*
    // even in the fallback path; then abort. In an actor context the
    // scheduler's own frame will catch this before we get here — only
    // non-actor threads with no frame reach the fallback.
    fprintf(stderr, "aether: panic outside any try/catch or actor: %s\n", reason);
    abort();
}

// ---------------------------------------------------------------------------
// Signal handlers (opt-in via AETHER_CATCH_SIGNALS=1)
// ---------------------------------------------------------------------------
//
// Caveats documented in panic-recover.md: converting a native fault into a
// panic is best-effort. A SIGSEGV mid-enqueue may leave queue state
// inconsistent. This path exists so the process survives *some* native
// faults during development and testing, not as a production replacement
// for memory safety.
//
// Windows has no sigaction / SA_SIGINFO / SIGBUS — Win32 uses SEH for native
// faults, a different recovery model entirely. We stub the installer out on
// Windows so the rest of the panic path (panic()/try/catch via setjmp) still
// works; only the "convert SIGSEGV into a panic" feature is POSIX-only.
#ifndef _WIN32

static void aether_sig_handler(int sig, siginfo_t* info, void* ucontext) {
    (void)info;
    (void)ucontext;

    // Only attempt recovery if we're in an actor step. Otherwise restore
    // default handler and let the OS take us down — this avoids re-entering
    // the signal handler if the scheduler itself faulted.
    if (!g_aether_in_actor_step) {
        signal(sig, SIG_DFL);
        raise(sig);
        return;
    }

    AetherJmpFrame* f = aether_current_frame();
    if (!f) {
        signal(sig, SIG_DFL);
        raise(sig);
        return;
    }

    const char* reason;
    switch (sig) {
        case SIGSEGV: reason = "signal: SIGSEGV (invalid memory access)"; break;
        case SIGFPE:  reason = "signal: SIGFPE (arithmetic fault)";       break;
        case SIGBUS:  reason = "signal: SIGBUS (bus error)";              break;
        default:      reason = "signal: unknown";                         break;
    }
    f->reason = reason;
    AETHER_SIGLONGJMP(f->buf, 1);
}

void aether_panic_install_signal_handlers(void) {
    const char* env = getenv("AETHER_CATCH_SIGNALS");
    if (!env || strcmp(env, "1") != 0) return;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = aether_sig_handler;
    sa.sa_flags = SA_SIGINFO | SA_NODEFER;
    sigemptyset(&sa.sa_mask);

    // SA_NODEFER is needed because we siglongjmp out of the handler — the
    // normal mask-restore at handler return never happens. sigsetjmp(buf, 1)
    // on the scheduler side saves and restores the signal mask explicitly,
    // so nested signals during recovery behave correctly.

    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGFPE,  &sa, NULL);
    sigaction(SIGBUS,  &sa, NULL);
}

#else  // _WIN32 — Win32 uses SEH for native faults, not POSIX signals.

void aether_panic_install_signal_handlers(void) {
    // Intentional no-op. On Windows the SIGSEGV→panic conversion path
    // would require SEH/__try, which is a separate design. Callers that
    // use plain panic() / try / catch still work unchanged.
}

#endif  // _WIN32

// ---------------------------------------------------------------------------
// Death hook
// ---------------------------------------------------------------------------

void aether_set_on_actor_death(AetherDeathHook fn) {
    death_hook = fn;
}

void aether_fire_death_hook(int actor_id, const char* reason) {
    AetherDeathHook h = death_hook;
    if (h) h(actor_id, reason ? reason : "unknown");
}
