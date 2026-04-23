// AetherUIDriver — shared HTTP test-server interface.
//
// Each backend (GTK4, AppKit, Win32) fills in a small set of hooks that
// describe how to introspect and mutate widgets, and then calls
// aether_ui_test_server_start() to spawn the accept loop. The HTTP
// parsing, URL routing, JSON formatting, sealed-widget bookkeeping, and
// cross-platform socket setup live once in aether_ui_test_server.c.
//
// The hooks are intentionally tiny and synchronous: each action struct
// is filled by the HTTP thread, handed to dispatch_action() which must
// marshal it onto the UI thread, and the HTTP thread reads the result
// back. GTK uses g_idle_add, AppKit uses dispatch_async to the main
// queue, Win32 uses SendMessage to a hidden AE_WM_DRIVER window.

#ifndef AETHER_UI_TEST_SERVER_H
#define AETHER_UI_TEST_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AETHER_DRV_CLICK     = 0,
    AETHER_DRV_SET_TEXT  = 1,
    AETHER_DRV_TOGGLE    = 2,
    AETHER_DRV_SET_VALUE = 3,
    AETHER_DRV_SET_STATE = 4,
} AetherDriverActionKind;

typedef struct {
    AetherDriverActionKind action;
    int    handle;
    double dval;
    char   sval[512];
    // Output: 0=ok, 1=sealed, 2=banner, 3=not_found
    int    result;
    // Set by dispatch_action → 1 when the UI thread has finished. Used by
    // the HTTP thread to poll the result; the server wrapper does this
    // for you, backends don't need to touch it.
    volatile int done;
} AetherDriverActionCtx;

typedef struct {
    // Introspection — called on the HTTP server thread.
    // These must be thread-safe against the UI thread. In practice, GTK,
    // AppKit, and Win32 all tolerate read-only queries about widget state
    // from off-thread; the only true races would be widget registry
    // growth under concurrent creation, which test harnesses don't do.
    int         (*widget_count)(void);
    const char* (*widget_type)(int handle);
    void        (*widget_text_into)(int handle, char* buf, int bufsize);
    int         (*widget_visible)(int handle);
    int         (*widget_parent)(int handle);
    int         (*toggle_active)(int handle);
    double      (*slider_value)(int handle);
    double      (*progressbar_fraction)(int handle);

    // Marshal the action onto the UI thread and block until it completes.
    // The server reads ctx->result after return.
    void (*dispatch_action)(AetherDriverActionCtx* ctx);
} AetherDriverHooks;

// Spawn the accept-loop thread. Returns immediately. The server stays up
// for the life of the process; there is currently no shutdown hook (the
// inherited test_server_thread in each backend never exposed one either).
void aether_ui_test_server_start(int port, const AetherDriverHooks* hooks);

// Banner + seal management. Implemented in aether_ui_test_server.c and
// callable from backends that need to query sealed state (e.g., when
// walking a subtree to seal it recursively).
void aether_ui_test_server_set_banner(int handle);
int  aether_ui_test_server_banner_handle(void);
void aether_ui_test_server_seal_widget(int handle);
int  aether_ui_test_server_is_sealed(int handle);

#ifdef __cplusplus
}
#endif

#endif
