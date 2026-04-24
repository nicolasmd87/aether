// Aether UI — Win32 backend
//
// Native Windows implementation of the backend ABI declared in
// aether_ui_backend.h. Uses USER32 (windows + controls), GDI+ (custom paint),
// COMCTL32 (slider, progressbar, tooltip), COMDLG32 (file dialogs),
// DWMAPI (dark mode, accent color), SHELL32 (open URL), WS2_32 (test server).
//
// Toolchain: MinGW-w64 (gcc). No MSVC-specific extensions — only POSIX-ish C
// plus Win32 headers. Wide-char APIs are used throughout; UTF-8 conversion
// happens at the API boundary.
//
// This file is paired with aether_ui_backend.h. The Aether DSL layer
// (contrib/aether_ui/module.ae) declares matching externs.

#include "aether_ui_backend.h"

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

// Windows 10 1809+ for immersive dark mode, per-monitor DPI v2.
// WINVER + _WIN32_WINNT + NTDDI_VERSION must all be set for MinGW's headers
// to expose GetDpiForSystem / AdjustWindowRectExForDpi / DwmSetWindowAttribute.
#ifndef WINVER
#define WINVER 0x0A00
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x0A000007  /* NTDDI_WIN10_RS3 — covers per-monitor-v2 APIs */
#endif

// Winsock must be included before windows.h.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <shlobj.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>

// Library linkage is declared in build.sh (MinGW does not honor #pragma comment).
// Required libs: user32, gdi32, comctl32, comdlg32, shell32, dwmapi, uxtheme,
// ole32, uuid, ws2_32.

// ---------------------------------------------------------------------------
// Closure struct — must match Aether codegen's _AeClosure layout.
// ---------------------------------------------------------------------------
typedef struct {
    void (*fn)(void);
    void* env;
} AeClosure;

static inline void invoke_closure(AeClosure* c) {
    if (c && c->fn) ((void (*)(void*))c->fn)(c->env);
}

// ---------------------------------------------------------------------------
// UTF-8 ↔ UTF-16 helpers.
//
// Windows uses UTF-16 internally (`wchar_t`). Aether uses UTF-8 (`char*`).
// Converted strings live in static rotating buffers to avoid caller cleanup.
// Callers may hold the result across one call, not indefinitely.
// ---------------------------------------------------------------------------
#define UTF_BUFS 8
#define UTF_BUF_SIZE 4096

static wchar_t* utf8_to_wide(const char* s) {
    static wchar_t bufs[UTF_BUFS][UTF_BUF_SIZE];
    static int idx = 0;
    if (!s) s = "";
    wchar_t* buf = bufs[idx];
    idx = (idx + 1) % UTF_BUFS;
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, buf, UTF_BUF_SIZE);
    if (n <= 0) buf[0] = L'\0';
    return buf;
}

static char* wide_to_utf8(const wchar_t* s) {
    static char bufs[UTF_BUFS][UTF_BUF_SIZE];
    static int idx = 0;
    if (!s) s = L"";
    char* buf = bufs[idx];
    idx = (idx + 1) % UTF_BUFS;
    int n = WideCharToMultiByte(CP_UTF8, 0, s, -1, buf, UTF_BUF_SIZE, NULL, NULL);
    if (n <= 0) buf[0] = '\0';
    return buf;
}

// ---------------------------------------------------------------------------
// Widget kind enum + metadata.
//
// Every HWND we create is tagged with a kind + per-widget metadata (style
// overrides, attached closures, child-layout info for stacks). Stored in the
// global widgets[] array so the test-server and styling APIs can introspect.
// ---------------------------------------------------------------------------
typedef enum {
    WK_NULL = 0,
    WK_TEXT,
    WK_BUTTON,
    WK_TEXTFIELD,
    WK_SECUREFIELD,
    WK_TEXTAREA,
    WK_TOGGLE,
    WK_SLIDER,
    WK_PICKER,
    WK_PROGRESSBAR,
    WK_IMAGE,
    WK_VSTACK,
    WK_HSTACK,
    WK_ZSTACK,
    WK_FORM,
    WK_FORM_SECTION,
    WK_NAVSTACK,
    WK_SCROLLVIEW,
    WK_SPACER,
    WK_DIVIDER,
    WK_CANVAS,
    WK_WINDOW,
    WK_SHEET,
    WK_GRID,
} WidgetKind;

typedef struct {
    COLORREF color;
    int has_value;
} ColorOverride;

typedef struct {
    // 0 = horizontal (HStack), 1 = vertical (VStack), 2 = ZStack
    int orientation;
    int spacing;
    int padding_top, padding_right, padding_bottom, padding_left;
    int alignment;    // 0=start, 1=center, 2=end
    int distribution; // 0=fill, 1=equal, 2=trailing
} StackLayout;

typedef struct {
    WidgetKind kind;
    HWND hwnd;
    HWND parent_hwnd;

    // Appearance overrides (applied via subclass paint hooks)
    ColorOverride bg;
    ColorOverride fg;
    int gradient_enabled;
    COLORREF grad_a, grad_b;
    int grad_vertical;
    int corner_radius;
    double opacity; // 0.0–1.0; <0 = no override

    // Fonts
    HFONT custom_font;
    double font_size;
    int font_bold;

    // Fixed sizing (0 = auto)
    int pref_width;
    int pref_height;
    int min_width;
    int min_height;

    // Margins (apply during parent layout)
    int margin_top, margin_right, margin_bottom, margin_left;

    // Stack container layout
    StackLayout stack;

    // Attached event closures
    AeClosure* on_click;
    AeClosure* on_hover;
    AeClosure* on_double_click;
    AeClosure* on_change; // text/value change for input widgets

    // Per-widget data (union over kind)
    union {
        struct { int timer_id; } button;
        struct { int canvas_id; } canvas;
        struct { double min_v, max_v, cur_v; } slider;
        struct { double fraction; } progressbar;
    } u;

    // Tooltip text (owned)
    wchar_t* tooltip;

    // Sealed flag (test server)
    int sealed;
} Widget;

static Widget** widgets = NULL;
static int widget_count = 0;
static int widget_capacity = 0;

// Open-addressed reverse map HWND → 1-based handle.
//
// Before this, handle_for_hwnd() was an O(n) linear scan called from every
// WM_COMMAND, every stack layout, and every /widgets JSON emit — so a
// 500-widget app paid ~500 pointer compares per button click. Replacing it
// with a flat probe table keeps lookup O(1) amortized with predictable
// memory (8 bytes per live widget + load-factor slack).
//
// Keys are HWND (64-bit on x64); we use a 32-bit multiplicative Fibonacci
// hash and linear probing. Never shrinks — widgets are never unregistered
// in the current lifecycle, so stale tombstones aren't needed.
typedef struct { HWND hwnd; int handle; } WidgetHashEntry;
static WidgetHashEntry* widget_hash = NULL;
static int widget_hash_mask = 0;  // capacity - 1; capacity is always power of 2
static int widget_hash_count = 0;

static uint32_t hash_hwnd(HWND h) {
    // Fibonacci hash: multiply by the golden ratio constant, keep high bits.
    uint64_t k = (uint64_t)(uintptr_t)h;
    return (uint32_t)((k * 0x9E3779B97F4A7C15ULL) >> 32);
}

static void widget_hash_grow(int new_cap_pow2) {
    WidgetHashEntry* old = widget_hash;
    int old_cap = widget_hash_mask + 1;
    widget_hash = (WidgetHashEntry*)calloc((size_t)new_cap_pow2,
                                            sizeof(WidgetHashEntry));
    widget_hash_mask = new_cap_pow2 - 1;
    widget_hash_count = 0;
    if (old) {
        for (int i = 0; i < old_cap; i++) {
            if (old[i].hwnd) {
                uint32_t slot = hash_hwnd(old[i].hwnd) & widget_hash_mask;
                while (widget_hash[slot].hwnd) slot = (slot + 1) & widget_hash_mask;
                widget_hash[slot] = old[i];
                widget_hash_count++;
            }
        }
        free(old);
    }
}

static void widget_hash_insert(HWND h, int handle) {
    if (!h) return;
    // Keep load factor < 0.5 for good probe distance. The `!widget_hash`
    // guard handles the initial empty-state correctly — otherwise the
    // `count * 2 >= mask + 1` arithmetic evaluates 0 >= 1 (false) and
    // the slot write dereferences a NULL table.
    if (!widget_hash || widget_hash_count * 2 >= (widget_hash_mask + 1)) {
        int new_cap = widget_hash ? (widget_hash_mask + 1) * 2 : 64;
        widget_hash_grow(new_cap);
    }
    uint32_t slot = hash_hwnd(h) & widget_hash_mask;
    while (widget_hash[slot].hwnd && widget_hash[slot].hwnd != h)
        slot = (slot + 1) & widget_hash_mask;
    if (!widget_hash[slot].hwnd) widget_hash_count++;
    widget_hash[slot].hwnd = h;
    widget_hash[slot].handle = handle;
}

static Widget* widget_at(int handle) {
    if (handle < 1 || handle > widget_count) return NULL;
    return widgets[handle - 1];
}

int aether_ui_register_widget(void* hwnd) {
    if (widget_count >= widget_capacity) {
        widget_capacity = widget_capacity == 0 ? 64 : widget_capacity * 2;
        widgets = (Widget**)realloc(widgets, sizeof(Widget*) * widget_capacity);
    }
    Widget* w = (Widget*)calloc(1, sizeof(Widget));
    w->hwnd = (HWND)hwnd;
    w->opacity = -1.0;
    w->font_size = 0.0;
    widgets[widget_count++] = w;
    widget_hash_insert((HWND)hwnd, widget_count);
    return widget_count; // 1-based
}

static int register_widget_typed(HWND hwnd, WidgetKind kind) {
    int h = aether_ui_register_widget(hwnd);
    if (h > 0) widgets[h - 1]->kind = kind;
    return h;
}

void* aether_ui_get_widget(int handle) {
    Widget* w = widget_at(handle);
    return w ? (void*)w->hwnd : NULL;
}

// O(1) average reverse lookup via the HWND hash. Falls back to linear scan
// for HWNDs that were never registered (the hash would miss anyway).
static int handle_for_hwnd(HWND h) {
    if (!h || !widget_hash) return 0;
    uint32_t slot = hash_hwnd(h) & widget_hash_mask;
    while (widget_hash[slot].hwnd) {
        if (widget_hash[slot].hwnd == h) return widget_hash[slot].handle;
        slot = (slot + 1) & widget_hash_mask;
    }
    return 0;
}

// Public wrapper matching the backend ABI.
int aether_ui_handle_for_widget(void* widget) {
    return handle_for_hwnd((HWND)widget);
}

// ---------------------------------------------------------------------------
// Reactive state — ported verbatim from the GTK4 backend (platform-neutral).
// ---------------------------------------------------------------------------
typedef struct {
    int state_handle;
    int text_handle;
    char* prefix;
    char* suffix;
} TextBinding;

static double* state_values = NULL;
static int state_count = 0;
static int state_capacity = 0;

static TextBinding* text_bindings = NULL;
static int text_binding_count = 0;
static int text_binding_capacity = 0;

int aether_ui_state_create(double initial) {
    if (state_count >= state_capacity) {
        state_capacity = state_capacity == 0 ? 32 : state_capacity * 2;
        state_values = (double*)realloc(state_values, sizeof(double) * state_capacity);
    }
    state_values[state_count++] = initial;
    return state_count;
}

double aether_ui_state_get(int handle) {
    if (handle < 1 || handle > state_count) return 0.0;
    return state_values[handle - 1];
}

static void update_text_bindings(int state_handle) {
    double val = aether_ui_state_get(state_handle);
    for (int i = 0; i < text_binding_count; i++) {
        TextBinding* b = &text_bindings[i];
        if (b->state_handle != state_handle) continue;
        char buf[256];
        if (val == (int)val) {
            snprintf(buf, sizeof(buf), "%s%d%s", b->prefix, (int)val, b->suffix);
        } else {
            snprintf(buf, sizeof(buf), "%s%.2f%s", b->prefix, val, b->suffix);
        }
        aether_ui_text_set_string(b->text_handle, buf);
    }
}

void aether_ui_state_set(int handle, double value) {
    if (handle < 1 || handle > state_count) return;
    state_values[handle - 1] = value;
    update_text_bindings(handle);
}

void aether_ui_state_bind_text(int state_handle, int text_handle,
                               const char* prefix, const char* suffix) {
    if (text_binding_count >= text_binding_capacity) {
        text_binding_capacity = text_binding_capacity == 0 ? 32 : text_binding_capacity * 2;
        text_bindings = (TextBinding*)realloc(text_bindings,
            sizeof(TextBinding) * text_binding_capacity);
    }
    TextBinding* b = &text_bindings[text_binding_count++];
    b->state_handle = state_handle;
    b->text_handle = text_handle;
    b->prefix = strdup(prefix ? prefix : "");
    b->suffix = strdup(suffix ? suffix : "");

    double val = aether_ui_state_get(state_handle);
    char buf[256];
    if (val == (int)val) {
        snprintf(buf, sizeof(buf), "%s%d%s", b->prefix, (int)val, b->suffix);
    } else {
        snprintf(buf, sizeof(buf), "%s%.2f%s", b->prefix, val, b->suffix);
    }
    aether_ui_text_set_string(text_handle, buf);
}

// ---------------------------------------------------------------------------
// Stack container — custom window class.
//
// A container that lays out its children left-to-right (HStack), top-to-bottom
// (VStack), or over-each-other (ZStack). Mirrors the semantics of GtkBox and
// NSStackView. Handles WM_SIZE by querying each child's preferred size and
// laying them out with spacing + padding.
//
// Children that are themselves stacks are recursed. Spacers consume leftover
// flex space along the primary axis.
// ---------------------------------------------------------------------------
static const wchar_t* STACK_CLASS = L"AetherUIStack";

typedef struct {
    int measured_w;
    int measured_h;
    int is_spacer;
    int margin_t, margin_r, margin_b, margin_l;
} MeasuredChild;

// Measure a single widget's intrinsic size. STATIC/BUTTON use a minimal
// heuristic (text extents + padding); custom widgets honor pref_width/height.
static void measure_widget(Widget* w, int* out_w, int* out_h) {
    if (w->pref_width > 0 && w->pref_height > 0) {
        *out_w = w->pref_width;
        *out_h = w->pref_height;
        return;
    }
    RECT r;
    if (GetWindowRect(w->hwnd, &r)) {
        int cur_w = r.right - r.left;
        int cur_h = r.bottom - r.top;
        // Ask the control for its natural size via GetTextExtentPoint where it
        // makes sense, else fall back to current size.
        if (w->kind == WK_TEXT || w->kind == WK_BUTTON) {
            HDC hdc = GetDC(w->hwnd);
            HFONT font = w->custom_font ? w->custom_font
                         : (HFONT)SendMessageW(w->hwnd, WM_GETFONT, 0, 0);
            HFONT old = font ? (HFONT)SelectObject(hdc, font) : NULL;
            wchar_t text[1024];
            int tlen = GetWindowTextW(w->hwnd, text, 1024);
            SIZE sz;
            GetTextExtentPoint32W(hdc, text, tlen, &sz);
            if (old) SelectObject(hdc, old);
            ReleaseDC(w->hwnd, hdc);
            int pad_x = w->kind == WK_BUTTON ? 24 : 4;
            int pad_y = w->kind == WK_BUTTON ? 10 : 4;
            *out_w = sz.cx + pad_x;
            *out_h = sz.cy + pad_y;
            if (w->pref_width > 0) *out_w = w->pref_width;
            if (w->pref_height > 0) *out_h = w->pref_height;
            return;
        }
        *out_w = w->pref_width > 0 ? w->pref_width : cur_w;
        *out_h = w->pref_height > 0 ? w->pref_height : cur_h;
        return;
    }
    *out_w = w->pref_width > 0 ? w->pref_width : 100;
    *out_h = w->pref_height > 0 ? w->pref_height : 24;
}

// Layout all direct child windows of the stack.
static void stack_do_layout(HWND stack_hwnd) {
    int h = handle_for_hwnd(stack_hwnd);
    if (h == 0) return;
    Widget* sw = widget_at(h);
    if (!sw) return;
    StackLayout* sl = &sw->stack;
    int orientation = sl->orientation;

    RECT client;
    GetClientRect(stack_hwnd, &client);
    int avail_w = (client.right - client.left) - sl->padding_left - sl->padding_right;
    int avail_h = (client.bottom - client.top) - sl->padding_top - sl->padding_bottom;

    // Collect children in z-order.
    HWND* children = NULL;
    int nchildren = 0, cap = 0;
    for (HWND c = GetWindow(stack_hwnd, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
        if (nchildren >= cap) {
            cap = cap == 0 ? 16 : cap * 2;
            children = (HWND*)realloc(children, sizeof(HWND) * cap);
        }
        children[nchildren++] = c;
    }
    if (nchildren == 0) { free(children); return; }

    // ZStack: overlay every child filling the client area.
    if (orientation == 2) {
        for (int i = 0; i < nchildren; i++) {
            SetWindowPos(children[i], NULL,
                         sl->padding_left, sl->padding_top,
                         avail_w, avail_h, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        free(children);
        return;
    }

    // Measure + identify spacers.
    MeasuredChild* mc = (MeasuredChild*)calloc(nchildren, sizeof(MeasuredChild));
    int total_primary = 0;
    int spacer_count = 0;
    for (int i = 0; i < nchildren; i++) {
        int ch = handle_for_hwnd(children[i]);
        Widget* cw = widget_at(ch);
        if (cw && cw->kind == WK_SPACER) {
            mc[i].is_spacer = 1;
            spacer_count++;
            continue;
        }
        int cw_w = 0, ch_h = 0;
        if (cw) {
            measure_widget(cw, &cw_w, &ch_h);
            mc[i].margin_t = cw->margin_top;
            mc[i].margin_r = cw->margin_right;
            mc[i].margin_b = cw->margin_bottom;
            mc[i].margin_l = cw->margin_left;
        } else {
            cw_w = 100; ch_h = 24;
        }
        mc[i].measured_w = cw_w;
        mc[i].measured_h = ch_h;
        total_primary += (orientation == 1) ? (ch_h + mc[i].margin_t + mc[i].margin_b)
                                            : (cw_w + mc[i].margin_l + mc[i].margin_r);
    }
    int spacing_total = sl->spacing * (nchildren - 1);
    int primary_avail = (orientation == 1) ? avail_h : avail_w;
    int flex = primary_avail - total_primary - spacing_total;
    if (flex < 0) flex = 0;
    int per_spacer = spacer_count > 0 ? (flex / spacer_count) : 0;

    // Lay out.
    int cur = (orientation == 1) ? sl->padding_top : sl->padding_left;
    for (int i = 0; i < nchildren; i++) {
        int x, y, w, h;
        if (orientation == 1) { // VStack
            int ch_size = mc[i].is_spacer ? per_spacer : mc[i].measured_h;
            h = ch_size;
            w = avail_w - mc[i].margin_l - mc[i].margin_r;
            if (mc[i].measured_w > 0 && mc[i].measured_w < w && !mc[i].is_spacer) {
                if (sl->alignment == 1)
                    x = sl->padding_left + mc[i].margin_l + (w - mc[i].measured_w) / 2;
                else if (sl->alignment == 2)
                    x = sl->padding_left + mc[i].margin_l + (w - mc[i].measured_w);
                else
                    x = sl->padding_left + mc[i].margin_l;
                w = mc[i].measured_w;
            } else {
                x = sl->padding_left + mc[i].margin_l;
            }
            y = cur + mc[i].margin_t;
            cur = y + h + mc[i].margin_b + sl->spacing;
        } else { // HStack
            int ch_size = mc[i].is_spacer ? per_spacer : mc[i].measured_w;
            w = ch_size;
            h = avail_h - mc[i].margin_t - mc[i].margin_b;
            if (mc[i].measured_h > 0 && mc[i].measured_h < h && !mc[i].is_spacer) {
                if (sl->alignment == 1)
                    y = sl->padding_top + mc[i].margin_t + (h - mc[i].measured_h) / 2;
                else if (sl->alignment == 2)
                    y = sl->padding_top + mc[i].margin_t + (h - mc[i].measured_h);
                else
                    y = sl->padding_top + mc[i].margin_t;
                h = mc[i].measured_h;
            } else {
                y = sl->padding_top + mc[i].margin_t;
            }
            x = cur + mc[i].margin_l;
            cur = x + w + mc[i].margin_r + sl->spacing;
        }
        SetWindowPos(children[i], NULL, x, y, w, h,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }

    free(mc);
    free(children);
}

static LRESULT CALLBACK stack_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_SIZE:
            stack_do_layout(hwnd);
            return 0;

        case WM_ERASEBKGND: {
            int h = handle_for_hwnd(hwnd);
            Widget* w = widget_at(h);
            if (w && w->bg.has_value) {
                HDC hdc = (HDC)wp;
                RECT r;
                GetClientRect(hwnd, &r);
                HBRUSH br = CreateSolidBrush(w->bg.color);
                FillRect(hdc, &r, br);
                DeleteObject(br);
                return 1;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        }

        case WM_COMMAND: {
            // Forward control notifications from children to their registered
            // on_click closures (buttons, checkboxes).
            HWND child = (HWND)lp;
            WORD code = HIWORD(wp);
            int ch = handle_for_hwnd(child);
            Widget* cw = widget_at(ch);
            if (cw) {
                if (cw->kind == WK_BUTTON && (code == BN_CLICKED || code == 0)) {
                    if (!cw->sealed) invoke_closure(cw->on_click);
                } else if (cw->kind == WK_TOGGLE && code == BN_CLICKED) {
                    if (!cw->sealed) invoke_closure(cw->on_change);
                } else if ((cw->kind == WK_TEXTFIELD || cw->kind == WK_SECUREFIELD
                            || cw->kind == WK_TEXTAREA) && code == EN_CHANGE) {
                    if (!cw->sealed) invoke_closure(cw->on_change);
                } else if (cw->kind == WK_PICKER && code == CBN_SELCHANGE) {
                    if (!cw->sealed) invoke_closure(cw->on_change);
                }
            }
            return 0;
        }

        case WM_HSCROLL:
        case WM_VSCROLL: {
            HWND child = (HWND)lp;
            int ch = handle_for_hwnd(child);
            Widget* cw = widget_at(ch);
            if (cw && cw->kind == WK_SLIDER) {
                int pos = (int)SendMessageW(child, TBM_GETPOS, 0, 0);
                // Map pos back to the slider's min/max range.
                double min_v = cw->u.slider.min_v;
                double max_v = cw->u.slider.max_v;
                double val = min_v + (max_v - min_v) * (pos / 1000.0);
                cw->u.slider.cur_v = val;
                if (!cw->sealed) invoke_closure(cw->on_change);
            }
            return 0;
        }

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORBTN: {
            HWND child = (HWND)lp;
            HDC hdc = (HDC)wp;
            int ch = handle_for_hwnd(child);
            Widget* cw = widget_at(ch);
            if (cw) {
                if (cw->fg.has_value) SetTextColor(hdc, cw->fg.color);
                if (cw->bg.has_value) {
                    SetBkColor(hdc, cw->bg.color);
                    // Keep a cached brush per-widget (leaked for now; cleaned
                    // in WM_DESTROY via DeleteObject for widgets we know of).
                    static HBRUSH last_brush = NULL;
                    static COLORREF last_color = 0;
                    if (last_brush && last_color != cw->bg.color) {
                        DeleteObject(last_brush);
                        last_brush = NULL;
                    }
                    if (!last_brush) {
                        last_brush = CreateSolidBrush(cw->bg.color);
                        last_color = cw->bg.color;
                    }
                    return (LRESULT)last_brush;
                }
                SetBkMode(hdc, TRANSPARENT);
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        }

        case WM_DESTROY:
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------------------
// Main app window class + DPI awareness.
// ---------------------------------------------------------------------------
static const wchar_t* APP_CLASS = L"AetherUIAppWindow";
static const wchar_t* DIVIDER_CLASS = L"AetherUIDivider";
static const wchar_t* SPACER_CLASS = L"AetherUISpacer";
static const wchar_t* CANVAS_CLASS = L"AetherUICanvas";
static int win_classes_registered = 0;
static int gdiplus_started = 0;
static ULONG_PTR gdiplus_token = 0;

// GDI+ flat API — declared here to avoid pulling in <gdiplus.h> (C++ only).
typedef struct {
    UINT32 GdiplusVersion;
    void* DebugEventCallback;
    BOOL SuppressBackgroundThread;
    BOOL SuppressExternalCodecs;
} GdiplusStartupInput;

__declspec(dllimport) int __stdcall GdiplusStartup(ULONG_PTR* token,
    const GdiplusStartupInput* input, void* output);
__declspec(dllimport) void __stdcall GdiplusShutdown(ULONG_PTR token);

static void ensure_gdiplus(void) {
    if (gdiplus_started) return;
    GdiplusStartupInput in = { 1, NULL, FALSE, FALSE };
    if (GdiplusStartup(&gdiplus_token, &in, NULL) == 0) gdiplus_started = 1;
}

static LRESULT CALLBACK divider_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT r;
        GetClientRect(hwnd, &r);
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
        HPEN old = (HPEN)SelectObject(hdc, pen);
        int my = r.top + (r.bottom - r.top) / 2;
        MoveToEx(hdc, r.left, my, NULL);
        LineTo(hdc, r.right, my);
        SelectObject(hdc, old);
        DeleteObject(pen);
        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_ERASEBKGND) return 1;
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static LRESULT CALLBACK spacer_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_ERASEBKGND) return 1;
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// Canvas drawing backend lives farther down; the window proc forwards to it.
static LRESULT CALLBACK canvas_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
static LRESULT CALLBACK grid_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
static int menu_dispatch_command(UINT id);

// Menu command IDs start here to avoid collision with button control IDs.
#define AE_MENU_ID_BASE 0x8000

// Window class name for grid containers; the class is registered in
// register_window_classes() alongside STACK / DIVIDER / SPACER / CANVAS.
static const wchar_t* GRID_CLASS = L"AetherUIGrid";

static LRESULT CALLBACK app_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_SIZE: {
            // Resize the single root child to fill the client area.
            HWND child = GetWindow(hwnd, GW_CHILD);
            if (child) {
                RECT r;
                GetClientRect(hwnd, &r);
                SetWindowPos(child, NULL, 0, 0,
                             r.right - r.left, r.bottom - r.top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
            }
            return 0;
        }
        case WM_DPICHANGED: {
            RECT* suggested = (RECT*)lp;
            SetWindowPos(hwnd, NULL,
                         suggested->left, suggested->top,
                         suggested->right - suggested->left,
                         suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_COMMAND: {
            // WM_COMMAND with a menu ID (no control HWND) → look up the
            // registered closure. Otherwise forward to the stack proc,
            // which handles WM_COMMAND from child controls.
            WORD id = LOWORD(wp);
            if (lp == 0 && HIWORD(wp) == 0 && id >= AE_MENU_ID_BASE) {
                if (menu_dispatch_command(id)) return 0;
            }
            return stack_wnd_proc(hwnd, msg, wp, lp);
        }
        case WM_HSCROLL:
        case WM_VSCROLL:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORBTN:
            // App window forwards control notifications to the stack proc
            // by calling it directly; the root widget is always a stack.
            return stack_wnd_proc(hwnd, msg, wp, lp);
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void register_window_classes(HINSTANCE inst) {
    if (win_classes_registered) return;
    WNDCLASSEXW wc;

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = app_wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = APP_CLASS;
    RegisterClassExW(&wc);

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = stack_wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = STACK_CLASS;
    RegisterClassExW(&wc);

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = divider_wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = DIVIDER_CLASS;
    RegisterClassExW(&wc);

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = spacer_wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = SPACER_CLASS;
    RegisterClassExW(&wc);

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = canvas_wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = CANVAS_CLASS;
    RegisterClassExW(&wc);

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = grid_wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = GRID_CLASS;
    RegisterClassExW(&wc);

    win_classes_registered = 1;
}

// DPI awareness setup — try the newest API first, fall back for older Windows.
typedef BOOL (WINAPI *SetProcessDpiAwarenessContextFn)(DPI_AWARENESS_CONTEXT);

static void init_dpi_awareness(void) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        FARPROC raw = GetProcAddress(user32, "SetProcessDpiAwarenessContext");
        SetProcessDpiAwarenessContextFn fn = (SetProcessDpiAwarenessContextFn)(void(*)(void))raw;
        if (fn && fn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) return;
    }
    // Fallback: per-monitor aware (Windows 8.1+)
    HMODULE shcore = LoadLibraryW(L"shcore.dll");
    if (shcore) {
        typedef HRESULT (WINAPI *SetProcessDpiAwarenessFn)(int);
        FARPROC raw = GetProcAddress(shcore, "SetProcessDpiAwareness");
        SetProcessDpiAwarenessFn fn = (SetProcessDpiAwarenessFn)(void(*)(void))raw;
        if (fn) fn(2);
        FreeLibrary(shcore);
    }
}

// Hidden holder window — serves as the initial parent for newly-created
// widgets. Win32 refuses to create WS_CHILD windows with a NULL parent, so
// every widget starts life parented here; it is reparented to its real
// container by aether_ui_widget_add_child_ctx / aether_ui_app_set_body.
static HWND widget_holder = NULL;

static int init_done = 0;
static void ensure_win_init(void) {
    if (init_done) return;
    init_done = 1;
    init_dpi_awareness();
    INITCOMMONCONTROLSEX icc = { sizeof(icc),
        ICC_STANDARD_CLASSES | ICC_BAR_CLASSES | ICC_PROGRESS_CLASS
            | ICC_DATE_CLASSES | ICC_UPDOWN_CLASS };
    InitCommonControlsEx(&icc);
    ensure_gdiplus();
    register_window_classes(GetModuleHandleW(NULL));
    // Create a hidden, message-only-ish holder. Not WS_OVERLAPPEDWINDOW
    // (that would show) — a plain popup window with no WS_VISIBLE.
    widget_holder = CreateWindowExW(
        0, STACK_CLASS, L"AetherUIHolder",
        WS_POPUP, 0, 0, 0, 0,
        NULL, NULL, GetModuleHandleW(NULL), NULL);
}

// ---------------------------------------------------------------------------
// App lifecycle
// ---------------------------------------------------------------------------
typedef struct {
    wchar_t* title;
    int width;
    int height;
    int root_handle;
    HWND hwnd;
    HMENU pending_menu;  // menu bar to SetMenu() on this app's window
} AppEntry;

static AppEntry* apps = NULL;
static int app_count = 0;
static int app_capacity = 0;

int aether_ui_app_create(const char* title, int width, int height) {
    ensure_win_init();
    if (app_count >= app_capacity) {
        app_capacity = app_capacity == 0 ? 4 : app_capacity * 2;
        apps = (AppEntry*)realloc(apps, sizeof(AppEntry) * app_capacity);
    }
    AppEntry* e = &apps[app_count];
    e->title = _wcsdup(utf8_to_wide(title));
    e->width = width;
    e->height = height;
    e->root_handle = 0;
    e->hwnd = NULL;
    e->pending_menu = NULL;
    app_count++;
    return app_count;
}

void aether_ui_app_set_body(int app_handle, int root_handle) {
    if (app_handle < 1 || app_handle > app_count) return;
    apps[app_handle - 1].root_handle = root_handle;
}

// Apply immersive dark mode to a window if the system is in dark mode.
static void apply_window_theme(HWND hwnd) {
    BOOL dark = FALSE;
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0, KEY_READ, &key) == ERROR_SUCCESS) {
        DWORD val = 0, sz = sizeof(val);
        if (RegQueryValueExW(key, L"AppsUseLightTheme", NULL, NULL,
            (LPBYTE)&val, &sz) == ERROR_SUCCESS) {
            dark = (val == 0);
        }
        RegCloseKey(key);
    }
    if (dark) {
        BOOL v = TRUE;
        DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */,
                              &v, sizeof(v));
    }
}

void aether_ui_app_run_raw(int app_handle) {
    if (app_handle < 1 || app_handle > app_count) return;
    AppEntry* e = &apps[app_handle - 1];
    ensure_win_init();

    // DPI-scaled size
    UINT dpi = GetDpiForSystem();
    int w = MulDiv(e->width, dpi, 96);
    int h = MulDiv(e->height, dpi, 96);

    // Account for non-client area so client ≈ requested size.
    RECT rc = { 0, 0, w, h };
    AdjustWindowRectExForDpi(&rc, WS_OVERLAPPEDWINDOW, FALSE,
                              WS_EX_APPWINDOW, dpi);
    int win_w = rc.right - rc.left;
    int win_h = rc.bottom - rc.top;

    e->hwnd = CreateWindowExW(
        WS_EX_APPWINDOW, APP_CLASS, e->title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, win_w, win_h,
        NULL, NULL, GetModuleHandleW(NULL), NULL);
    if (!e->hwnd) return;
    apply_window_theme(e->hwnd);

    // Reparent the root widget into the app window.
    if (e->root_handle > 0) {
        Widget* rw = widget_at(e->root_handle);
        if (rw) {
            SetParent(rw->hwnd, e->hwnd);
            LONG_PTR st = GetWindowLongPtrW(rw->hwnd, GWL_STYLE);
            SetWindowLongPtrW(rw->hwnd, GWL_STYLE, st | WS_CHILD | WS_VISIBLE);
            SetWindowLongPtrW(rw->hwnd, GWL_EXSTYLE,
                GetWindowLongPtrW(rw->hwnd, GWL_EXSTYLE) & ~WS_EX_APPWINDOW);
            RECT cr;
            GetClientRect(e->hwnd, &cr);
            SetWindowPos(rw->hwnd, NULL, 0, 0,
                         cr.right - cr.left, cr.bottom - cr.top,
                         SWP_NOZORDER | SWP_SHOWWINDOW);
        }
    }

    // Attach the menu bar (if one was registered via aether_ui_menu_bar_attach).
    // We defer this until here so the window exists.
    if (e->pending_menu) {
        SetMenu(e->hwnd, e->pending_menu);
    }

    // Honor AETHER_UI_HEADLESS for CI and unattended scenarios. The window
    // is still created, the message loop still pumps, and the test server
    // still responds — but nothing is ever rendered to the visible desktop.
    // This keeps GitHub Actions `windows-latest` runs clean (no taskbar
    // icons, no UAC/SmartScreen visibility, no chance of a stuck window).
    const char* headless = getenv("AETHER_UI_HEADLESS");
    int show_mode = (headless && headless[0] && headless[0] != '0') ? SW_HIDE : SW_SHOW;
    ShowWindow(e->hwnd, show_mode);
    if (show_mode == SW_SHOW) UpdateWindow(e->hwnd);

    // Check AETHER_UI_TEST_PORT and launch test server if set.
    const char* test_port_env = getenv("AETHER_UI_TEST_PORT");
    if (test_port_env) {
        int port = atoi(test_port_env);
        if (port > 0 && e->root_handle > 0) {
            aether_ui_enable_test_server_impl(port, e->root_handle);
        }
    }

    // Message loop with Tab/Enter/Esc dialog-navigation support.
    // IsDialogMessageW routes Tab between WS_TABSTOP controls, Shift+Tab
    // reverses, Enter activates the default button, Esc cancels. Without
    // this wrap, those keys would fall through to the plain
    // TranslateMessage/DispatchMessage path and be ignored by child
    // controls (no focus traversal at all).
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        // Only the top-level app window participates in dialog nav; child
        // popups created via aether_ui_window_create are independent.
        if (!IsDialogMessageW(e->hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
}

// ---------------------------------------------------------------------------
// Core widgets: text, button, vstack, hstack, spacer, divider.
// ---------------------------------------------------------------------------
int aether_ui_text_create(const char* text) {
    ensure_win_init();
    HWND h = CreateWindowExW(0, L"STATIC", utf8_to_wide(text),
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
        0, 0, 0, 0, widget_holder, NULL, GetModuleHandleW(NULL), NULL);
    if (!h) return 0;
    SendMessageW(h, WM_SETFONT,
        (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    return register_widget_typed(h, WK_TEXT);
}

void aether_ui_text_set_string(int handle, const char* text) {
    Widget* w = widget_at(handle);
    if (w && w->hwnd) SetWindowTextW(w->hwnd, utf8_to_wide(text));
}

int aether_ui_button_create_plain(const char* label) {
    ensure_win_init();
    HWND h = CreateWindowExW(0, L"BUTTON", utf8_to_wide(label),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        0, 0, 0, 0, widget_holder, NULL, GetModuleHandleW(NULL), NULL);
    if (!h) return 0;
    SendMessageW(h, WM_SETFONT,
        (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    return register_widget_typed(h, WK_BUTTON);
}

int aether_ui_button_create(const char* label, void* boxed_closure) {
    int handle = aether_ui_button_create_plain(label);
    Widget* w = widget_at(handle);
    if (w) w->on_click = (AeClosure*)boxed_closure;
    return handle;
}

void aether_ui_set_onclick_ctx(void* ctx, void* boxed_closure) {
    int handle = (int)(intptr_t)ctx;
    Widget* w = widget_at(handle);
    if (w) w->on_click = (AeClosure*)boxed_closure;
}

static int create_stack(int orientation, int spacing) {
    ensure_win_init();
    HWND h = CreateWindowExW(0, STACK_CLASS, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 0, 0, 0, widget_holder, NULL, GetModuleHandleW(NULL), NULL);
    if (!h) return 0;
    WidgetKind kind = orientation == 0 ? WK_HSTACK
                   : orientation == 1 ? WK_VSTACK : WK_ZSTACK;
    int handle = register_widget_typed(h, kind);
    Widget* w = widget_at(handle);
    if (w) {
        w->stack.orientation = orientation;
        w->stack.spacing = spacing;
    }
    return handle;
}

int aether_ui_vstack_create(int spacing) { return create_stack(1, spacing); }
int aether_ui_hstack_create(int spacing) { return create_stack(0, spacing); }
int aether_ui_zstack_create(void) { return create_stack(2, 0); }

int aether_ui_spacer_create(void) {
    ensure_win_init();
    HWND h = CreateWindowExW(0, SPACER_CLASS, L"",
        WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
        widget_holder, NULL, GetModuleHandleW(NULL), NULL);
    return register_widget_typed(h, WK_SPACER);
}

int aether_ui_divider_create(void) {
    ensure_win_init();
    HWND h = CreateWindowExW(0, DIVIDER_CLASS, L"",
        WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
        widget_holder, NULL, GetModuleHandleW(NULL), NULL);
    int handle = register_widget_typed(h, WK_DIVIDER);
    Widget* w = widget_at(handle);
    if (w) { w->pref_width = 1; w->pref_height = 12; }
    return handle;
}

// ---------------------------------------------------------------------------
// Parent → child wiring. Unlike GTK's "add_child", Win32 parents children
// at creation time OR reparents via SetParent. Our DSL calls this after
// creation, so we reparent here.
// ---------------------------------------------------------------------------
void aether_ui_widget_add_child_ctx(void* parent_ctx, int child_handle) {
    int parent_handle = (int)(intptr_t)parent_ctx;
    Widget* p = widget_at(parent_handle);
    Widget* c = widget_at(child_handle);
    if (!p || !c) return;
    SetParent(c->hwnd, p->hwnd);
    LONG_PTR st = GetWindowLongPtrW(c->hwnd, GWL_STYLE);
    SetWindowLongPtrW(c->hwnd, GWL_STYLE, st | WS_CHILD | WS_VISIBLE);
    ShowWindow(c->hwnd, SW_SHOW);
    if (p->kind == WK_VSTACK || p->kind == WK_HSTACK || p->kind == WK_ZSTACK) {
        stack_do_layout(p->hwnd);
    }
}

void aether_ui_widget_set_hidden(int handle, int hidden) {
    Widget* w = widget_at(handle);
    if (w) ShowWindow(w->hwnd, hidden ? SW_HIDE : SW_SHOW);
}

// ---------------------------------------------------------------------------
// Input widgets: textfield, securefield, textarea, toggle, slider, picker,
// progressbar. All register change closures dispatched via the stack proc's
// WM_COMMAND handler.
// ---------------------------------------------------------------------------
int aether_ui_textfield_create(const char* placeholder, void* boxed_closure) {
    ensure_win_init();
    HWND h = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT | ES_AUTOHSCROLL,
        0, 0, 0, 0, widget_holder, NULL, GetModuleHandleW(NULL), NULL);
    if (!h) return 0;
    SendMessageW(h, WM_SETFONT,
        (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    // Windows has no native placeholder on EDIT — use EM_SETCUEBANNER (comctl32 6+)
    if (placeholder && *placeholder) {
        SendMessageW(h, 0x1501 /* EM_SETCUEBANNER */, TRUE,
                     (LPARAM)utf8_to_wide(placeholder));
    }
    int handle = register_widget_typed(h, WK_TEXTFIELD);
    Widget* w = widget_at(handle);
    if (w) { w->on_change = (AeClosure*)boxed_closure; w->pref_height = 24; }
    return handle;
}

int aether_ui_securefield_create(const char* placeholder, void* boxed_closure) {
    ensure_win_init();
    HWND h = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT | ES_PASSWORD | ES_AUTOHSCROLL,
        0, 0, 0, 0, widget_holder, NULL, GetModuleHandleW(NULL), NULL);
    if (!h) return 0;
    SendMessageW(h, WM_SETFONT,
        (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    if (placeholder && *placeholder) {
        SendMessageW(h, 0x1501, TRUE, (LPARAM)utf8_to_wide(placeholder));
    }
    int handle = register_widget_typed(h, WK_SECUREFIELD);
    Widget* w = widget_at(handle);
    if (w) { w->on_change = (AeClosure*)boxed_closure; w->pref_height = 24; }
    return handle;
}

void aether_ui_textfield_set_text(int handle, const char* text) {
    Widget* w = widget_at(handle);
    if (w) SetWindowTextW(w->hwnd, utf8_to_wide(text));
}

const char* aether_ui_textfield_get_text(int handle) {
    Widget* w = widget_at(handle);
    if (!w) return "";
    wchar_t buf[4096];
    GetWindowTextW(w->hwnd, buf, 4096);
    return wide_to_utf8(buf);
}

int aether_ui_toggle_create(const char* label, void* boxed_closure) {
    ensure_win_init();
    HWND h = CreateWindowExW(0, L"BUTTON", utf8_to_wide(label),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        0, 0, 0, 0, widget_holder, NULL, GetModuleHandleW(NULL), NULL);
    if (!h) return 0;
    SendMessageW(h, WM_SETFONT,
        (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    int handle = register_widget_typed(h, WK_TOGGLE);
    Widget* w = widget_at(handle);
    if (w) w->on_change = (AeClosure*)boxed_closure;
    return handle;
}

void aether_ui_toggle_set_active(int handle, int active) {
    Widget* w = widget_at(handle);
    if (w) SendMessageW(w->hwnd, BM_SETCHECK,
        active ? BST_CHECKED : BST_UNCHECKED, 0);
}

int aether_ui_toggle_get_active(int handle) {
    Widget* w = widget_at(handle);
    if (!w) return 0;
    return SendMessageW(w->hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0;
}

int aether_ui_slider_create(double min_val, double max_val,
                            double initial, void* boxed_closure) {
    ensure_win_init();
    HWND h = CreateWindowExW(0, TRACKBAR_CLASSW, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_NOTICKS,
        0, 0, 0, 0, widget_holder, NULL, GetModuleHandleW(NULL), NULL);
    if (!h) return 0;
    SendMessageW(h, TBM_SETRANGE, TRUE, MAKELPARAM(0, 1000));
    double frac = (max_val > min_val) ? (initial - min_val) / (max_val - min_val) : 0;
    SendMessageW(h, TBM_SETPOS, TRUE, (LPARAM)(int)(frac * 1000));
    int handle = register_widget_typed(h, WK_SLIDER);
    Widget* w = widget_at(handle);
    if (w) {
        w->u.slider.min_v = min_val;
        w->u.slider.max_v = max_val;
        w->u.slider.cur_v = initial;
        w->on_change = (AeClosure*)boxed_closure;
        w->pref_height = 28;
    }
    return handle;
}

void aether_ui_slider_set_value(int handle, double value) {
    Widget* w = widget_at(handle);
    if (!w) return;
    double frac = (w->u.slider.max_v > w->u.slider.min_v)
        ? (value - w->u.slider.min_v) / (w->u.slider.max_v - w->u.slider.min_v) : 0;
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    SendMessageW(w->hwnd, TBM_SETPOS, TRUE, (LPARAM)(int)(frac * 1000));
    w->u.slider.cur_v = value;
}

double aether_ui_slider_get_value(int handle) {
    Widget* w = widget_at(handle);
    return w ? w->u.slider.cur_v : 0.0;
}

int aether_ui_picker_create(void* boxed_closure) {
    ensure_win_init();
    HWND h = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        0, 0, 0, 0, widget_holder, NULL, GetModuleHandleW(NULL), NULL);
    if (!h) return 0;
    SendMessageW(h, WM_SETFONT,
        (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    int handle = register_widget_typed(h, WK_PICKER);
    Widget* w = widget_at(handle);
    if (w) { w->on_change = (AeClosure*)boxed_closure; w->pref_height = 200; }
    return handle;
}

void aether_ui_picker_add_item(int handle, const char* item) {
    Widget* w = widget_at(handle);
    if (w) SendMessageW(w->hwnd, CB_ADDSTRING, 0, (LPARAM)utf8_to_wide(item));
}

void aether_ui_picker_set_selected(int handle, int index) {
    Widget* w = widget_at(handle);
    if (w) SendMessageW(w->hwnd, CB_SETCURSEL, (WPARAM)index, 0);
}

int aether_ui_picker_get_selected(int handle) {
    Widget* w = widget_at(handle);
    if (!w) return -1;
    return (int)SendMessageW(w->hwnd, CB_GETCURSEL, 0, 0);
}

int aether_ui_textarea_create(const char* placeholder, void* boxed_closure) {
    ensure_win_init();
    HWND h = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL
            | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
        0, 0, 0, 0, widget_holder, NULL, GetModuleHandleW(NULL), NULL);
    if (!h) return 0;
    SendMessageW(h, WM_SETFONT,
        (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    if (placeholder && *placeholder) {
        SendMessageW(h, 0x1501, TRUE, (LPARAM)utf8_to_wide(placeholder));
    }
    int handle = register_widget_typed(h, WK_TEXTAREA);
    Widget* w = widget_at(handle);
    if (w) { w->on_change = (AeClosure*)boxed_closure; w->pref_height = 120; }
    return handle;
}

void aether_ui_textarea_set_text(int handle, const char* text) {
    Widget* w = widget_at(handle);
    if (w) SetWindowTextW(w->hwnd, utf8_to_wide(text));
}

char* aether_ui_textarea_get_text(int handle) {
    Widget* w = widget_at(handle);
    if (!w) return strdup("");
    int len = GetWindowTextLengthW(w->hwnd);
    wchar_t* buf = (wchar_t*)malloc(sizeof(wchar_t) * (len + 1));
    GetWindowTextW(w->hwnd, buf, len + 1);
    char* out = strdup(wide_to_utf8(buf));
    free(buf);
    return out;
}

int aether_ui_scrollview_create(void) {
    // Approximation: a stack with scroll styles. Full virtualization would
    // need a custom class; for now we lean on the stack child hosting + the
    // parent window's scrollbars if needed.
    int handle = create_stack(1, 0);
    Widget* w = widget_at(handle);
    if (w) {
        LONG_PTR st = GetWindowLongPtrW(w->hwnd, GWL_STYLE);
        SetWindowLongPtrW(w->hwnd, GWL_STYLE, st | WS_VSCROLL | WS_HSCROLL);
        w->kind = WK_SCROLLVIEW;
    }
    return handle;
}

int aether_ui_progressbar_create(double fraction) {
    ensure_win_init();
    HWND h = CreateWindowExW(0, PROGRESS_CLASSW, L"",
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        0, 0, 0, 0, widget_holder, NULL, GetModuleHandleW(NULL), NULL);
    if (!h) return 0;
    SendMessageW(h, PBM_SETRANGE32, 0, 1000);
    int v = (int)(fraction * 1000);
    if (v < 0) v = 0;
    if (v > 1000) v = 1000;
    SendMessageW(h, PBM_SETPOS, (WPARAM)v, 0);
    int handle = register_widget_typed(h, WK_PROGRESSBAR);
    Widget* w = widget_at(handle);
    if (w) { w->u.progressbar.fraction = fraction; w->pref_height = 20; }
    return handle;
}

void aether_ui_progressbar_set_fraction(int handle, double fraction) {
    Widget* w = widget_at(handle);
    if (!w) return;
    int v = (int)(fraction * 1000);
    if (v < 0) v = 0;
    if (v > 1000) v = 1000;
    SendMessageW(w->hwnd, PBM_SETPOS, (WPARAM)v, 0);
    w->u.progressbar.fraction = fraction;
}

// ---------------------------------------------------------------------------
// Form / FormSection / NavStack — thin wrappers over VStack.
// ---------------------------------------------------------------------------
int aether_ui_form_create(void) {
    int handle = create_stack(1, 12);
    Widget* w = widget_at(handle);
    if (w) {
        w->kind = WK_FORM;
        w->stack.padding_top = w->stack.padding_bottom = 12;
        w->stack.padding_left = w->stack.padding_right = 16;
    }
    return handle;
}

int aether_ui_form_section_create(const char* title) {
    int handle = create_stack(1, 6);
    Widget* w = widget_at(handle);
    if (w) w->kind = WK_FORM_SECTION;
    if (title && *title) {
        int th = aether_ui_text_create(title);
        Widget* tw = widget_at(th);
        if (tw) {
            // Bold section header
            HFONT base = (HFONT)SendMessageW(tw->hwnd, WM_GETFONT, 0, 0);
            LOGFONTW lf;
            GetObjectW(base, sizeof(lf), &lf);
            lf.lfWeight = FW_BOLD;
            HFONT bold = CreateFontIndirectW(&lf);
            SendMessageW(tw->hwnd, WM_SETFONT, (WPARAM)bold, TRUE);
            tw->custom_font = bold;
        }
        aether_ui_widget_add_child_ctx((void*)(intptr_t)handle, th);
    }
    return handle;
}

int aether_ui_navstack_create(void) {
    int handle = create_stack(1, 0);
    Widget* w = widget_at(handle);
    if (w) w->kind = WK_NAVSTACK;
    return handle;
}

void aether_ui_navstack_push(int handle, const char* title, int body_handle) {
    (void)title;
    Widget* host = widget_at(handle);
    Widget* body = widget_at(body_handle);
    if (!host || !body) return;
    // Hide previous children, show this one.
    for (HWND c = GetWindow(host->hwnd, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
        ShowWindow(c, SW_HIDE);
    }
    SetParent(body->hwnd, host->hwnd);
    LONG_PTR st = GetWindowLongPtrW(body->hwnd, GWL_STYLE);
    SetWindowLongPtrW(body->hwnd, GWL_STYLE, st | WS_CHILD | WS_VISIBLE);
    ShowWindow(body->hwnd, SW_SHOW);
    stack_do_layout(host->hwnd);
}

void aether_ui_navstack_pop(int handle) {
    Widget* host = widget_at(handle);
    if (!host) return;
    HWND top = NULL;
    for (HWND c = GetWindow(host->hwnd, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
        if (IsWindowVisible(c)) top = c;
    }
    if (top) {
        DestroyWindow(top);
        // Re-show the new top child if any.
        for (HWND c = GetWindow(host->hwnd, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
            ShowWindow(c, SW_SHOW);
        }
    }
    stack_do_layout(host->hwnd);
}

// ---------------------------------------------------------------------------
// Styling + theming.
// ---------------------------------------------------------------------------
static inline COLORREF rgb_from_doubles(double r, double g, double b) {
    int ri = (int)(r * 255); if (ri < 0) ri = 0; if (ri > 255) ri = 255;
    int gi = (int)(g * 255); if (gi < 0) gi = 0; if (gi > 255) gi = 255;
    int bi = (int)(b * 255); if (bi < 0) bi = 0; if (bi > 255) bi = 255;
    return RGB(ri, gi, bi);
}

void aether_ui_set_bg_color(int handle, double r, double g, double b, double a) {
    (void)a; // Solid colors only; alpha requires layered-window composition.
    Widget* w = widget_at(handle);
    if (!w) return;
    w->bg.has_value = 1;
    w->bg.color = rgb_from_doubles(r, g, b);
    InvalidateRect(w->hwnd, NULL, TRUE);
}

void aether_ui_set_bg_gradient(int handle,
                               double r1, double g1, double b1,
                               double r2, double g2, double b2, int vertical) {
    Widget* w = widget_at(handle);
    if (!w) return;
    w->gradient_enabled = 1;
    w->grad_a = rgb_from_doubles(r1, g1, b1);
    w->grad_b = rgb_from_doubles(r2, g2, b2);
    w->grad_vertical = vertical;
    InvalidateRect(w->hwnd, NULL, TRUE);
}

void aether_ui_set_text_color(int handle, double r, double g, double b) {
    Widget* w = widget_at(handle);
    if (!w) return;
    w->fg.has_value = 1;
    w->fg.color = rgb_from_doubles(r, g, b);
    InvalidateRect(w->hwnd, NULL, TRUE);
}

static void apply_font(Widget* w) {
    if (!w) return;
    LOGFONTW lf = {0};
    HFONT base = (HFONT)SendMessageW(w->hwnd, WM_GETFONT, 0, 0);
    if (base) GetObjectW(base, sizeof(lf), &lf);
    else GetObjectW((HFONT)GetStockObject(DEFAULT_GUI_FONT), sizeof(lf), &lf);
    if (w->font_size > 0) {
        // Font size in points → logical units for current DPI
        HDC hdc = GetDC(w->hwnd);
        int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
        ReleaseDC(w->hwnd, hdc);
        lf.lfHeight = -MulDiv((int)w->font_size, dpi, 72);
    }
    lf.lfWeight = w->font_bold ? FW_BOLD : FW_NORMAL;
    if (w->custom_font) DeleteObject(w->custom_font);
    w->custom_font = CreateFontIndirectW(&lf);
    SendMessageW(w->hwnd, WM_SETFONT, (WPARAM)w->custom_font, TRUE);
}

void aether_ui_set_font_size(int handle, double size) {
    Widget* w = widget_at(handle);
    if (!w) return;
    w->font_size = size;
    apply_font(w);
}

void aether_ui_set_font_bold(int handle, int bold) {
    Widget* w = widget_at(handle);
    if (!w) return;
    w->font_bold = bold;
    apply_font(w);
}

void aether_ui_set_corner_radius(int handle, double radius) {
    Widget* w = widget_at(handle);
    if (!w) return;
    w->corner_radius = (int)radius;
    RECT r;
    GetClientRect(w->hwnd, &r);
    HRGN rgn = CreateRoundRectRgn(0, 0, r.right + 1, r.bottom + 1,
                                   (int)radius * 2, (int)radius * 2);
    SetWindowRgn(w->hwnd, rgn, TRUE);
}

void aether_ui_set_edge_insets(int handle, double top, double right,
                               double bottom, double left) {
    Widget* w = widget_at(handle);
    if (!w) return;
    w->stack.padding_top = (int)top;
    w->stack.padding_right = (int)right;
    w->stack.padding_bottom = (int)bottom;
    w->stack.padding_left = (int)left;
    if (w->kind == WK_VSTACK || w->kind == WK_HSTACK || w->kind == WK_ZSTACK) {
        stack_do_layout(w->hwnd);
    }
}

void aether_ui_set_width(int handle, int width) {
    Widget* w = widget_at(handle);
    if (w) w->pref_width = width;
}

void aether_ui_set_height(int handle, int height) {
    Widget* w = widget_at(handle);
    if (w) w->pref_height = height;
}

void aether_ui_set_opacity(int handle, double opacity) {
    Widget* w = widget_at(handle);
    if (!w) return;
    w->opacity = opacity;
    // WS_EX_LAYERED only works reliably on top-level windows; for child
    // widgets this is a no-op. Apps requesting child opacity should use
    // compositing backends (we fall back silently).
    LONG_PTR ex = GetWindowLongPtrW(w->hwnd, GWL_EXSTYLE);
    if (!(ex & WS_CHILD)) {
        SetWindowLongPtrW(w->hwnd, GWL_EXSTYLE, ex | WS_EX_LAYERED);
        double a = opacity;
        if (a < 0) a = 0;
        if (a > 1) a = 1;
        SetLayeredWindowAttributes(w->hwnd, 0, (BYTE)(a * 255), LWA_ALPHA);
    }
}

void aether_ui_set_enabled(int handle, int enabled) {
    Widget* w = widget_at(handle);
    if (w) EnableWindow(w->hwnd, enabled);
}

void aether_ui_set_tooltip(int handle, const char* text) {
    Widget* w = widget_at(handle);
    if (!w) return;
    static HWND tooltip_hwnd = NULL;
    if (!tooltip_hwnd) {
        tooltip_hwnd = CreateWindowExW(0, TOOLTIPS_CLASSW, NULL,
            WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            NULL, NULL, GetModuleHandleW(NULL), NULL);
    }
    if (w->tooltip) free(w->tooltip);
    w->tooltip = _wcsdup(utf8_to_wide(text));
    TOOLINFOW ti;
    memset(&ti, 0, sizeof(ti));
    ti.cbSize = sizeof(ti);
    ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    ti.hwnd = GetParent(w->hwnd);
    ti.uId = (UINT_PTR)w->hwnd;
    ti.lpszText = w->tooltip;
    SendMessageW(tooltip_hwnd, TTM_ADDTOOLW, 0, (LPARAM)&ti);
}

void aether_ui_set_distribution(int handle, int distribution) {
    Widget* w = widget_at(handle);
    if (w) w->stack.distribution = distribution;
}

void aether_ui_set_alignment(int handle, int alignment) {
    Widget* w = widget_at(handle);
    if (w) w->stack.alignment = alignment;
}

void aether_ui_match_parent_width(int handle) {
    Widget* w = widget_at(handle);
    if (w) w->pref_width = -1; // marker: fill parent
}

void aether_ui_match_parent_height(int handle) {
    Widget* w = widget_at(handle);
    if (w) w->pref_height = -1;
}

void aether_ui_set_margin(int handle, int top, int right, int bottom, int left) {
    Widget* w = widget_at(handle);
    if (!w) return;
    w->margin_top = top;
    w->margin_right = right;
    w->margin_bottom = bottom;
    w->margin_left = left;
}

void aether_ui_set_onclick_ctx_style_apply(void* ctx, double r, double g, double b, double a) {
    (void)ctx; (void)r; (void)g; (void)b; (void)a;
}
void aether_ui_set_bg_color_ctx(void* ctx, double r, double g, double b, double a) {
    aether_ui_set_bg_color((int)(intptr_t)ctx, r, g, b, a);
}
void aether_ui_set_text_color_ctx(void* ctx, double r, double g, double b) {
    aether_ui_set_text_color((int)(intptr_t)ctx, r, g, b);
}
void aether_ui_set_font_size_ctx(void* ctx, double size) {
    aether_ui_set_font_size((int)(intptr_t)ctx, size);
}
void aether_ui_set_font_bold_ctx(void* ctx, int bold) {
    aether_ui_set_font_bold((int)(intptr_t)ctx, bold);
}
void aether_ui_set_corner_radius_ctx(void* ctx, double radius) {
    aether_ui_set_corner_radius((int)(intptr_t)ctx, radius);
}
void aether_ui_set_opacity_ctx(void* ctx, double opacity) {
    aether_ui_set_opacity((int)(intptr_t)ctx, opacity);
}
void aether_ui_set_enabled_ctx(void* ctx, int enabled) {
    aether_ui_set_enabled((int)(intptr_t)ctx, enabled);
}
void aether_ui_set_tooltip_ctx(void* ctx, const char* text) {
    aether_ui_set_tooltip((int)(intptr_t)ctx, text);
}

// ---------------------------------------------------------------------------
// Events (hover, double-click, click-on-arbitrary-widget).
// ---------------------------------------------------------------------------
void aether_ui_on_click_impl(int handle, void* boxed_closure) {
    Widget* w = widget_at(handle);
    if (w) w->on_click = (AeClosure*)boxed_closure;
}

void aether_ui_on_hover_impl(int handle, void* boxed_closure) {
    Widget* w = widget_at(handle);
    if (w) w->on_hover = (AeClosure*)boxed_closure;
}

void aether_ui_on_double_click_impl(int handle, void* boxed_closure) {
    Widget* w = widget_at(handle);
    if (w) w->on_double_click = (AeClosure*)boxed_closure;
}

// ---------------------------------------------------------------------------
// System services: alert, file open, clipboard, timer, open URL, dark mode.
// ---------------------------------------------------------------------------
void aether_ui_alert_impl(const char* title, const char* message) {
    ensure_win_init();
    MessageBoxW(NULL, utf8_to_wide(message), utf8_to_wide(title),
                MB_OK | MB_ICONINFORMATION);
}

char* aether_ui_file_open(const char* title) {
    ensure_win_init();
    wchar_t file[1024] = L"";
    OPENFILENAMEW ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = L"All Files\0*.*\0\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = 1024;
    ofn.lpstrTitle = utf8_to_wide(title ? title : "Open File");
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) {
        return strdup(wide_to_utf8(file));
    }
    return NULL;
}

void aether_ui_clipboard_write_impl(const char* text) {
    if (!text) return;
    if (!OpenClipboard(NULL)) return;
    EmptyClipboard();
    wchar_t* wide = utf8_to_wide(text);
    size_t len = wcslen(wide) + 1;
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, len * sizeof(wchar_t));
    if (mem) {
        wchar_t* ptr = (wchar_t*)GlobalLock(mem);
        memcpy(ptr, wide, len * sizeof(wchar_t));
        GlobalUnlock(mem);
        SetClipboardData(CF_UNICODETEXT, mem);
    }
    CloseClipboard();
}

// Timer: each user timer is keyed by (hwnd, id). We use the first live app
// window as the timer host. Callbacks live in a flat array.
typedef struct {
    int id;
    AeClosure* closure;
    int alive;
} TimerEntry;
static TimerEntry* timers = NULL;
static int timer_count = 0;
static int timer_capacity = 0;
static int next_timer_id = 100;

static void CALLBACK timer_cb(HWND hwnd, UINT msg, UINT_PTR id, DWORD now) {
    (void)hwnd; (void)msg; (void)now;
    for (int i = 0; i < timer_count; i++) {
        if (timers[i].alive && (UINT_PTR)timers[i].id == id) {
            invoke_closure(timers[i].closure);
            return;
        }
    }
}

int aether_ui_timer_create_impl(int interval_ms, void* boxed_closure) {
    if (timer_count >= timer_capacity) {
        timer_capacity = timer_capacity == 0 ? 16 : timer_capacity * 2;
        timers = (TimerEntry*)realloc(timers, sizeof(TimerEntry) * timer_capacity);
    }
    int id = next_timer_id++;
    timers[timer_count].id = id;
    timers[timer_count].closure = (AeClosure*)boxed_closure;
    timers[timer_count].alive = 1;
    timer_count++;
    HWND host = (app_count > 0) ? apps[0].hwnd : NULL;
    SetTimer(host, id, interval_ms, (TIMERPROC)timer_cb);
    return id;
}

void aether_ui_timer_cancel_impl(int timer_id) {
    for (int i = 0; i < timer_count; i++) {
        if (timers[i].id == timer_id) {
            timers[i].alive = 0;
            HWND host = (app_count > 0) ? apps[0].hwnd : NULL;
            KillTimer(host, timer_id);
            return;
        }
    }
}

void aether_ui_open_url_impl(const char* url) {
    ShellExecuteW(NULL, L"open", utf8_to_wide(url), NULL, NULL, SW_SHOWNORMAL);
}

int aether_ui_dark_mode_check(void) {
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0, KEY_READ, &key) != ERROR_SUCCESS) return 0;
    DWORD val = 1, sz = sizeof(val);
    int dark = 0;
    if (RegQueryValueExW(key, L"AppsUseLightTheme", NULL, NULL,
        (LPBYTE)&val, &sz) == ERROR_SUCCESS) dark = (val == 0);
    RegCloseKey(key);
    return dark;
}

// ---------------------------------------------------------------------------
// Secondary windows + sheets.
// ---------------------------------------------------------------------------
int aether_ui_window_create_impl(const char* title, int width, int height) {
    ensure_win_init();
    UINT dpi = GetDpiForSystem();
    int w = MulDiv(width, dpi, 96);
    int h = MulDiv(height, dpi, 96);
    RECT rc = { 0, 0, w, h };
    AdjustWindowRectExForDpi(&rc, WS_OVERLAPPEDWINDOW, FALSE, 0, dpi);
    HWND hwnd = CreateWindowExW(0, APP_CLASS, utf8_to_wide(title),
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        NULL, NULL, GetModuleHandleW(NULL), NULL);
    if (!hwnd) return 0;
    apply_window_theme(hwnd);
    return register_widget_typed(hwnd, WK_WINDOW);
}

void aether_ui_window_set_body_impl(int win_handle, int root_handle) {
    Widget* win = widget_at(win_handle);
    Widget* root = widget_at(root_handle);
    if (!win || !root) return;
    SetParent(root->hwnd, win->hwnd);
    LONG_PTR st = GetWindowLongPtrW(root->hwnd, GWL_STYLE);
    SetWindowLongPtrW(root->hwnd, GWL_STYLE, st | WS_CHILD | WS_VISIBLE);
    RECT cr;
    GetClientRect(win->hwnd, &cr);
    SetWindowPos(root->hwnd, NULL, 0, 0,
                 cr.right - cr.left, cr.bottom - cr.top,
                 SWP_NOZORDER | SWP_SHOWWINDOW);
}

void aether_ui_window_show_impl(int win_handle) {
    Widget* w = widget_at(win_handle);
    if (w) { ShowWindow(w->hwnd, SW_SHOW); UpdateWindow(w->hwnd); }
}

void aether_ui_window_close_impl(int win_handle) {
    Widget* w = widget_at(win_handle);
    if (w) DestroyWindow(w->hwnd);
}

int aether_ui_sheet_create_impl(const char* title, int width, int height) {
    return aether_ui_window_create_impl(title, width, height);
}

void aether_ui_sheet_set_body_impl(int handle, int root_handle) {
    aether_ui_window_set_body_impl(handle, root_handle);
}

void aether_ui_sheet_present_impl(int handle) {
    aether_ui_window_show_impl(handle);
}

void aether_ui_sheet_dismiss_impl(int handle) {
    aether_ui_window_close_impl(handle);
}

// GDI+ flat-API image loaders — used to pick up PNG/JPEG/GIF/BMP/TIFF at
// runtime without pulling in the C++ GDI+ headers. The returned HBITMAP
// owns its pixels and can be passed straight to STM_SETIMAGE.
__declspec(dllimport) int __stdcall GdipCreateBitmapFromFile(
    const wchar_t* filename, void** bitmap);
__declspec(dllimport) int __stdcall GdipCreateHBITMAPFromBitmap(
    void* bitmap, HBITMAP* hbm, unsigned int background_argb);
__declspec(dllimport) int __stdcall GdipDisposeImage(void* image);

// Load any format GDI+ supports (PNG, JPEG, GIF, BMP, TIFF, ICO) into an
// HBITMAP suitable for SS_BITMAP. Falls back to LoadImageW for plain BMPs
// when GDI+ initialization fails or isn't available.
static HBITMAP load_image_any_format(const wchar_t* filename) {
    ensure_gdiplus();
    if (gdiplus_started) {
        void* gdi_bitmap = NULL;
        if (GdipCreateBitmapFromFile(filename, &gdi_bitmap) == 0 && gdi_bitmap) {
            HBITMAP hbm = NULL;
            // Transparent background (0x00000000 = ARGB all zero).
            GdipCreateHBITMAPFromBitmap(gdi_bitmap, &hbm, 0);
            GdipDisposeImage(gdi_bitmap);
            if (hbm) return hbm;
        }
    }
    // BMP-only fallback.
    return (HBITMAP)LoadImageW(NULL, filename, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
}

int aether_ui_image_create(const char* filepath) {
    ensure_win_init();
    HWND h = CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_BITMAP,
        0, 0, 0, 0, widget_holder, NULL, GetModuleHandleW(NULL), NULL);
    if (!h) return 0;
    HBITMAP bmp = load_image_any_format(utf8_to_wide(filepath));
    if (bmp) SendMessageW(h, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bmp);
    return register_widget_typed(h, WK_IMAGE);
}

void aether_ui_image_set_size(int handle, int width, int height) {
    Widget* w = widget_at(handle);
    if (w) { w->pref_width = width; w->pref_height = height; }
}

// ---------------------------------------------------------------------------
// Menu bar + context menus.
//
// Win32 menus (HMENU) are not HWNDs, so they live in a parallel registry.
// Each menu item is a small command ID (starting at AE_MENU_ID_BASE) that
// we dispatch from app_wnd_proc's WM_COMMAND: the ID looks up the
// registered closure and invokes it. Menu bars attach to the app window
// via SetMenu(); context menus use TrackPopupMenu().
// ---------------------------------------------------------------------------

typedef struct {
    HMENU    hmenu;
    int      is_menu_bar;   // 1 = menu bar (top-level); 0 = popup/submenu
    int      attached;      // 1 once this menu has been added to a parent bar
    wchar_t* label;         // display text for submenus (NULL for menu bars)
} MenuEntry;

static MenuEntry* menus = NULL;
static int        menu_count = 0;
static int        menu_capacity = 0;

typedef struct {
    UINT       id;
    AeClosure* closure;
} MenuCommand;

static MenuCommand* menu_commands = NULL;
static int          menu_command_count = 0;
static int          menu_command_capacity = 0;
static UINT         next_menu_id = AE_MENU_ID_BASE;

static int register_menu(HMENU hmenu, int is_bar, const wchar_t* label) {
    if (menu_count >= menu_capacity) {
        menu_capacity = menu_capacity == 0 ? 8 : menu_capacity * 2;
        menus = (MenuEntry*)realloc(menus, sizeof(MenuEntry) * menu_capacity);
    }
    menus[menu_count].hmenu = hmenu;
    menus[menu_count].is_menu_bar = is_bar;
    menus[menu_count].attached = 0;
    menus[menu_count].label = label ? _wcsdup(label) : NULL;
    menu_count++;
    return menu_count; // 1-based
}

static MenuEntry* menu_at(int handle) {
    if (handle < 1 || handle > menu_count) return NULL;
    return &menus[handle - 1];
}

// Called from the app window's WM_COMMAND when LOWORD(wParam) is a menu ID.
// Returns 1 if handled.
static int menu_dispatch_command(UINT id) {
    for (int i = 0; i < menu_command_count; i++) {
        if (menu_commands[i].id == id) {
            invoke_closure(menu_commands[i].closure);
            return 1;
        }
    }
    return 0;
}

int aether_ui_menu_bar_create(void) {
    ensure_win_init();
    HMENU hmenu = CreateMenu();
    return register_menu(hmenu, 1, NULL);
}

int aether_ui_menu_create(const char* label) {
    ensure_win_init();
    HMENU hmenu = CreatePopupMenu();
    return register_menu(hmenu, 0, utf8_to_wide(label ? label : "Menu"));
}

void aether_ui_menu_add_item(int menu_handle, const char* label,
                             void* boxed_closure) {
    MenuEntry* m = menu_at(menu_handle);
    if (!m) return;
    UINT id = next_menu_id++;
    AppendMenuW(m->hmenu, MF_STRING, id, utf8_to_wide(label));
    if (menu_command_count >= menu_command_capacity) {
        menu_command_capacity = menu_command_capacity == 0
                                ? 32 : menu_command_capacity * 2;
        menu_commands = (MenuCommand*)realloc(menu_commands,
            sizeof(MenuCommand) * menu_command_capacity);
    }
    menu_commands[menu_command_count].id = id;
    menu_commands[menu_command_count].closure = (AeClosure*)boxed_closure;
    menu_command_count++;
}

void aether_ui_menu_add_separator(int menu_handle) {
    MenuEntry* m = menu_at(menu_handle);
    if (m) AppendMenuW(m->hmenu, MF_SEPARATOR, 0, NULL);
}

void aether_ui_menu_bar_add_menu(int bar_handle, int menu_handle) {
    MenuEntry* bar = menu_at(bar_handle);
    MenuEntry* sub = menu_at(menu_handle);
    if (!bar || !sub) return;
    const wchar_t* label = sub->label ? sub->label : L"Menu";
    AppendMenuW(bar->hmenu, MF_POPUP, (UINT_PTR)sub->hmenu, label);
    sub->attached = 1;
}

void aether_ui_menu_bar_attach(int app_handle, int bar_handle) {
    if (app_handle < 1 || app_handle > app_count) return;
    MenuEntry* bar = menu_at(bar_handle);
    if (!bar) return;
    AppEntry* e = &apps[app_handle - 1];
    // aether_ui_app_run_raw hasn't created the window yet in the common flow,
    // so stash the menu bar and attach it at show-time.
    e->pending_menu = bar->hmenu;
}

void aether_ui_menu_popup(int menu_handle, int anchor_widget) {
    MenuEntry* m = menu_at(menu_handle);
    Widget* w = widget_at(anchor_widget);
    if (!m || !w) return;
    POINT pt;
    GetCursorPos(&pt);
    TrackPopupMenu(m->hmenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON,
                   pt.x, pt.y, 0, GetParent(w->hwnd), NULL);
}

// ---------------------------------------------------------------------------
// Grid layout.
//
// A 2D container where children claim (row, col) cells with optional row
// and column spans. Columns are equal-width by default; rows size to their
// tallest child. Matches GtkGrid / NSGridView semantics.
// ---------------------------------------------------------------------------

typedef struct {
    HWND hwnd;
    int  cols;
    int  row_spacing;
    int  col_spacing;
    struct {
        HWND hwnd;
        int  row, col, row_span, col_span;
    } items[64];
    int item_count;
} GridEntry;

static GridEntry** grids = NULL;
static int         grid_count = 0;
static int         grid_capacity = 0;

static GridEntry* grid_for_hwnd(HWND hwnd) {
    for (int i = 0; i < grid_count; i++) {
        if (grids[i] && grids[i]->hwnd == hwnd) return grids[i];
    }
    return NULL;
}

static void grid_do_layout(HWND hwnd) {
    GridEntry* g = grid_for_hwnd(hwnd);
    if (!g || g->item_count == 0) return;

    RECT client;
    GetClientRect(hwnd, &client);
    int total_w = client.right - client.left;
    int total_h = client.bottom - client.top;

    // Determine row count from max row index.
    int rows = 0;
    for (int i = 0; i < g->item_count; i++) {
        int r = g->items[i].row + g->items[i].row_span;
        if (r > rows) rows = r;
    }
    if (rows == 0) return;

    int col_w = (total_w - g->col_spacing * (g->cols - 1)) / g->cols;
    int row_h = (total_h - g->row_spacing * (rows - 1)) / rows;
    if (col_w < 0) col_w = 0;
    if (row_h < 0) row_h = 0;

    for (int i = 0; i < g->item_count; i++) {
        int x = g->items[i].col * (col_w + g->col_spacing);
        int y = g->items[i].row * (row_h + g->row_spacing);
        int w = col_w * g->items[i].col_span
                + g->col_spacing * (g->items[i].col_span - 1);
        int h = row_h * g->items[i].row_span
                + g->row_spacing * (g->items[i].row_span - 1);
        SetWindowPos(g->items[i].hwnd, NULL, x, y, w, h,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

static LRESULT CALLBACK grid_wnd_proc(HWND hwnd, UINT msg,
                                       WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_SIZE:
            grid_do_layout(hwnd);
            return 0;
        case WM_ERASEBKGND:
            return stack_wnd_proc(hwnd, msg, wp, lp);
        case WM_COMMAND:
        case WM_HSCROLL:
        case WM_VSCROLL:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORBTN:
            return stack_wnd_proc(hwnd, msg, wp, lp);
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int aether_ui_grid_create(int cols, int row_spacing, int col_spacing) {
    ensure_win_init();
    if (cols < 1) cols = 1;
    HWND hwnd = CreateWindowExW(0, GRID_CLASS, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 0, 0, 0, widget_holder, NULL, GetModuleHandleW(NULL), NULL);
    if (!hwnd) return 0;
    if (grid_count >= grid_capacity) {
        grid_capacity = grid_capacity == 0 ? 4 : grid_capacity * 2;
        grids = (GridEntry**)realloc(grids, sizeof(GridEntry*) * grid_capacity);
    }
    GridEntry* g = (GridEntry*)calloc(1, sizeof(GridEntry));
    g->hwnd = hwnd;
    g->cols = cols;
    g->row_spacing = row_spacing;
    g->col_spacing = col_spacing;
    grids[grid_count++] = g;
    return register_widget_typed(hwnd, WK_GRID);
}

void aether_ui_grid_place(int grid_handle, int child_handle,
                          int row, int col, int row_span, int col_span) {
    Widget* g = widget_at(grid_handle);
    Widget* c = widget_at(child_handle);
    if (!g || !c || g->kind != WK_GRID) return;
    GridEntry* ge = grid_for_hwnd(g->hwnd);
    if (!ge || ge->item_count >= 64) return;
    if (row_span < 1) row_span = 1;
    if (col_span < 1) col_span = 1;
    // Reparent the child to the grid.
    SetParent(c->hwnd, g->hwnd);
    LONG_PTR st = GetWindowLongPtrW(c->hwnd, GWL_STYLE);
    SetWindowLongPtrW(c->hwnd, GWL_STYLE, st | WS_CHILD | WS_VISIBLE);
    ge->items[ge->item_count].hwnd = c->hwnd;
    ge->items[ge->item_count].row = row;
    ge->items[ge->item_count].col = col;
    ge->items[ge->item_count].row_span = row_span;
    ge->items[ge->item_count].col_span = col_span;
    ge->item_count++;
    grid_do_layout(g->hwnd);
}

// ---------------------------------------------------------------------------
// Canvas backend — GDI+ flat-API path, minimal for first pass.
//
// Paths are recorded as a command stream; WM_PAINT replays them onto an
// offscreen bitmap then BitBlt-s to screen. Supports begin_path, move_to,
// line_to, stroke, fill_rect, clear.
// ---------------------------------------------------------------------------
typedef enum {
    CV_BEGIN, CV_MOVE, CV_LINE, CV_STROKE, CV_FILL_RECT, CV_CLEAR
} CanvasCmdKind;

typedef struct {
    CanvasCmdKind k;
    // p0..p3 carry geometry: x1,y1,x2,y2 for lines; x,y,w,h for rects;
    // line width in p0 for stroke commands.
    float p0, p1, p2, p3;
    float cr, cg, cb, calpha;
} CanvasCmd;

typedef struct {
    HWND hwnd;
    int width, height;
    CanvasCmd* cmds;
    int cmd_count;
    int cmd_cap;
    float cur_x, cur_y;
} Canvas;

static Canvas* canvases = NULL;
static int canvas_count = 0;
static int canvas_cap = 0;

static int canvas_id_for_hwnd(HWND hwnd) {
    for (int i = 0; i < canvas_count; i++) {
        if (canvases[i].hwnd == hwnd) return i + 1;
    }
    return 0;
}

static void canvas_add_cmd(int canvas_id, CanvasCmd cmd) {
    if (canvas_id < 1 || canvas_id > canvas_count) return;
    Canvas* c = &canvases[canvas_id - 1];
    if (c->cmd_count >= c->cmd_cap) {
        c->cmd_cap = c->cmd_cap == 0 ? 128 : c->cmd_cap * 2;
        c->cmds = (CanvasCmd*)realloc(c->cmds, sizeof(CanvasCmd) * c->cmd_cap);
    }
    c->cmds[c->cmd_count++] = cmd;
}

int aether_ui_canvas_create_impl(int width, int height) {
    ensure_win_init();
    HWND h = CreateWindowExW(0, CANVAS_CLASS, L"",
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, widget_holder, NULL, GetModuleHandleW(NULL), NULL);
    if (!h) return 0;
    if (canvas_count >= canvas_cap) {
        canvas_cap = canvas_cap == 0 ? 8 : canvas_cap * 2;
        canvases = (Canvas*)realloc(canvases, sizeof(Canvas) * canvas_cap);
    }
    Canvas* cv = &canvases[canvas_count++];
    cv->hwnd = h;
    cv->width = width;
    cv->height = height;
    cv->cmds = NULL;
    cv->cmd_count = 0;
    cv->cmd_cap = 0;
    cv->cur_x = cv->cur_y = 0;
    int widget_handle = register_widget_typed(h, WK_CANVAS);
    Widget* ww = widget_at(widget_handle);
    if (ww) {
        ww->pref_width = width;
        ww->pref_height = height;
        ww->u.canvas.canvas_id = canvas_count;
    }
    return canvas_count;
}

int aether_ui_canvas_get_widget(int canvas_id) {
    if (canvas_id < 1 || canvas_id > canvas_count) return 0;
    return handle_for_hwnd(canvases[canvas_id - 1].hwnd);
}

// begin_path starts a fresh command stream — drop any previously-recorded
// commands so a redraw-per-frame loop doesn't accumulate unboundedly.
// Previously this was an append-only op, which meant an animated canvas
// leaked ~16 bytes/cmd * 60Hz forever (hundreds of MB/hr on busy scenes).
void aether_ui_canvas_begin_path_impl(int canvas_id) {
    if (canvas_id >= 1 && canvas_id <= canvas_count) {
        canvases[canvas_id - 1].cmd_count = 0;
    }
    CanvasCmd c = {0}; c.k = CV_BEGIN;
    canvas_add_cmd(canvas_id, c);
}

void aether_ui_canvas_move_to_impl(int canvas_id, float x, float y) {
    CanvasCmd c = {0}; c.k = CV_MOVE; c.p0 = x; c.p1 = y;
    canvas_add_cmd(canvas_id, c);
    if (canvas_id >= 1 && canvas_id <= canvas_count) {
        canvases[canvas_id - 1].cur_x = x;
        canvases[canvas_id - 1].cur_y = y;
    }
}

void aether_ui_canvas_line_to_impl(int canvas_id, float x, float y) {
    if (canvas_id < 1 || canvas_id > canvas_count) return;
    Canvas* cv = &canvases[canvas_id - 1];
    CanvasCmd c = {0}; c.k = CV_LINE;
    c.p0 = cv->cur_x; c.p1 = cv->cur_y; c.p2 = x; c.p3 = y;
    canvas_add_cmd(canvas_id, c);
    cv->cur_x = x; cv->cur_y = y;
}

void aether_ui_canvas_stroke_impl(int canvas_id, float r, float g, float b,
                                   float a, float line_width) {
    CanvasCmd c = {0};
    c.k = CV_STROKE; c.cr = r; c.cg = g; c.cb = b; c.calpha = a; c.p0 = line_width;
    canvas_add_cmd(canvas_id, c);
}

void aether_ui_canvas_fill_rect_impl(int canvas_id, float x, float y,
                                      float w, float h,
                                      float r, float g, float b, float a) {
    CanvasCmd c = {0};
    c.k = CV_FILL_RECT; c.p0 = x; c.p1 = y; c.p2 = w; c.p3 = h;
    c.cr = r; c.cg = g; c.cb = b; c.calpha = a;
    canvas_add_cmd(canvas_id, c);
}

void aether_ui_canvas_clear_impl(int canvas_id) {
    if (canvas_id < 1 || canvas_id > canvas_count) return;
    canvases[canvas_id - 1].cmd_count = 0;
    CanvasCmd c = {0}; c.k = CV_CLEAR;
    canvas_add_cmd(canvas_id, c);
}

void aether_ui_canvas_redraw_impl(int canvas_id) {
    if (canvas_id < 1 || canvas_id > canvas_count) return;
    InvalidateRect(canvases[canvas_id - 1].hwnd, NULL, TRUE);
}

static void canvas_paint(HWND hwnd, HDC hdc, int width, int height) {
    int id = canvas_id_for_hwnd(hwnd);
    if (id == 0) return;
    Canvas* cv = &canvases[id - 1];
    // Plain GDI replay — GDI+ bindings from C are clunky; for a first
    // pass this delivers lines + filled rects with correct pixels.
    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP old_bmp = (HBITMAP)SelectObject(mem, bmp);
    // White background
    RECT full = { 0, 0, width, height };
    HBRUSH white = (HBRUSH)GetStockObject(WHITE_BRUSH);
    FillRect(mem, &full, white);

    HPEN cur_pen = NULL;
    HPEN old_pen = (HPEN)SelectObject(mem, GetStockObject(BLACK_PEN));

    for (int i = 0; i < cv->cmd_count; i++) {
        CanvasCmd* cmd = &cv->cmds[i];
        switch (cmd->k) {
            case CV_CLEAR:
                FillRect(mem, &full, white);
                break;
            case CV_LINE: {
                MoveToEx(mem, (int)cmd->p0, (int)cmd->p1, NULL);
                LineTo(mem, (int)cmd->p2, (int)cmd->p3);
                break;
            }
            case CV_STROKE: {
                if (cur_pen) DeleteObject(cur_pen);
                int ri = (int)(cmd->cr * 255), gi = (int)(cmd->cg * 255),
                    bi = (int)(cmd->cb * 255);
                cur_pen = CreatePen(PS_SOLID, (int)cmd->p0, RGB(ri, gi, bi));
                SelectObject(mem, cur_pen);
                break;
            }
            case CV_FILL_RECT: {
                int ri = (int)(cmd->cr * 255), gi = (int)(cmd->cg * 255),
                    bi = (int)(cmd->cb * 255);
                HBRUSH br = CreateSolidBrush(RGB(ri, gi, bi));
                RECT r = { (int)cmd->p0, (int)cmd->p1,
                           (int)(cmd->p0 + cmd->p2), (int)(cmd->p1 + cmd->p3) };
                FillRect(mem, &r, br);
                DeleteObject(br);
                break;
            }
            default: break;
        }
    }

    BitBlt(hdc, 0, 0, width, height, mem, 0, 0, SRCCOPY);

    SelectObject(mem, old_pen);
    if (cur_pen) DeleteObject(cur_pen);
    SelectObject(mem, old_bmp);
    DeleteObject(bmp);
    DeleteDC(mem);
}

static LRESULT CALLBACK canvas_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT r;
            GetClientRect(hwnd, &r);
            canvas_paint(hwnd, hdc, r.right - r.left, r.bottom - r.top);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------------------
// Animation: opacity interpolation via WM_TIMER.
// ---------------------------------------------------------------------------
typedef struct {
    int widget_handle;
    double start, target;
    int duration_ms;
    DWORD start_tick;
    int timer_id;
} Animation;
static Animation* anims = NULL;
static int anim_count = 0;
static int anim_cap = 0;

static void CALLBACK anim_tick(HWND hwnd, UINT msg, UINT_PTR id, DWORD now) {
    (void)hwnd; (void)msg; (void)now;
    for (int i = 0; i < anim_count; i++) {
        if ((UINT_PTR)anims[i].timer_id != id) continue;
        DWORD elapsed = GetTickCount() - anims[i].start_tick;
        double t = (double)elapsed / (double)anims[i].duration_ms;
        if (t >= 1.0) t = 1.0;
        double v = anims[i].start + (anims[i].target - anims[i].start) * t;
        aether_ui_set_opacity(anims[i].widget_handle, v);
        if (t >= 1.0) {
            KillTimer(NULL, anims[i].timer_id);
            // swap-delete
            anims[i] = anims[anim_count - 1];
            anim_count--;
        }
        return;
    }
}

void aether_ui_animate_opacity_impl(int handle, double target, int duration_ms) {
    if (anim_count >= anim_cap) {
        anim_cap = anim_cap == 0 ? 8 : anim_cap * 2;
        anims = (Animation*)realloc(anims, sizeof(Animation) * anim_cap);
    }
    Widget* w = widget_at(handle);
    double start = (w && w->opacity >= 0) ? w->opacity : 1.0;
    Animation* a = &anims[anim_count++];
    a->widget_handle = handle;
    a->start = start;
    a->target = target;
    a->duration_ms = duration_ms > 0 ? duration_ms : 200;
    a->start_tick = GetTickCount();
    a->timer_id = next_timer_id++;
    SetTimer(NULL, a->timer_id, 16, (TIMERPROC)anim_tick);
}

// ---------------------------------------------------------------------------
// Widget manipulation: remove/clear children.
// ---------------------------------------------------------------------------
void aether_ui_remove_child_impl(int parent_handle, int child_handle) {
    Widget* c = widget_at(child_handle);
    Widget* p = widget_at(parent_handle);
    if (c && c->hwnd) DestroyWindow(c->hwnd);
    if (p && (p->kind == WK_VSTACK || p->kind == WK_HSTACK || p->kind == WK_ZSTACK)) {
        stack_do_layout(p->hwnd);
    }
}

void aether_ui_clear_children_impl(int handle) {
    Widget* p = widget_at(handle);
    if (!p) return;
    HWND c = GetWindow(p->hwnd, GW_CHILD);
    while (c) {
        HWND next = GetWindow(c, GW_HWNDNEXT);
        DestroyWindow(c);
        c = next;
    }
    if (p->kind == WK_VSTACK || p->kind == WK_HSTACK || p->kind == WK_ZSTACK) {
        stack_do_layout(p->hwnd);
    }
}

// ---------------------------------------------------------------------------
// AetherUIDriver — Win32 adapter.
//
// The HTTP test server (socket accept, parsing, routing, JSON) lives in
// aether_ui_test_server.c and is shared with the GTK4 and AppKit
// backends. This section only provides the Win32-specific pieces:
//
//   * Backend hooks that answer widget introspection queries from the
//     server thread (widget_type, widget_text, toggle_active, etc.)
//   * UI-thread marshalling: HTTP requests fill an AetherDriverActionCtx
//     and hand it to dispatch_action(), which SendMessages it to a
//     hidden AE_WM_DRIVER window on the UI thread and blocks until the
//     action completes.
//   * Banner creation and sealing.
// ---------------------------------------------------------------------------

#include "aether_ui_test_server.h"

#define AE_WM_DRIVER (WM_USER + 0x42)

static HWND driver_host_hwnd = NULL;

// Widget-kind → short string (used as the "type" field in the driver JSON).
static const char* widget_kind_name(WidgetKind k) {
    switch (k) {
        case WK_TEXT: return "text";
        case WK_BUTTON: return "button";
        case WK_TEXTFIELD: return "textfield";
        case WK_SECUREFIELD: return "securefield";
        case WK_TEXTAREA: return "textarea";
        case WK_TOGGLE: return "toggle";
        case WK_SLIDER: return "slider";
        case WK_PICKER: return "picker";
        case WK_PROGRESSBAR: return "progressbar";
        case WK_IMAGE: return "image";
        case WK_VSTACK: return "vstack";
        case WK_HSTACK: return "hstack";
        case WK_ZSTACK: return "zstack";
        case WK_FORM: return "form";
        case WK_FORM_SECTION: return "form_section";
        case WK_NAVSTACK: return "navstack";
        case WK_SCROLLVIEW: return "scrollview";
        case WK_SPACER: return "spacer";
        case WK_DIVIDER: return "divider";
        case WK_CANVAS: return "canvas";
        case WK_WINDOW: return "window";
        case WK_SHEET: return "sheet";
        default: return "widget";
    }
}

// Seal APIs — delegate to the shared driver so the server sees a single
// source of truth. We also keep a per-widget `sealed` flag as a fast
// path for the WM_COMMAND handler, which fires on every button click.
void aether_ui_seal_widget_impl(int handle) {
    Widget* w = widget_at(handle);
    if (w) w->sealed = 1;
    aether_ui_test_server_seal_widget(handle);
}

void aether_ui_seal_subtree_impl(int handle) {
    Widget* w = widget_at(handle);
    if (!w) return;
    aether_ui_seal_widget_impl(handle);
    for (HWND c = GetWindow(w->hwnd, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
        int ch = handle_for_hwnd(c);
        if (ch) aether_ui_seal_subtree_impl(ch);
    }
}

// ---------------------------------------------------------------------------
// Backend hooks for the shared test server.
// ---------------------------------------------------------------------------
static int hook_widget_count(void) { return widget_count; }

static const char* hook_widget_type(int handle) {
    Widget* w = widget_at(handle);
    return w ? widget_kind_name(w->kind) : "null";
}

static void hook_widget_text_into(int handle, char* buf, int bufsize) {
    Widget* w = widget_at(handle);
    if (!w || !w->hwnd) { buf[0] = '\0'; return; }
    wchar_t wbuf[1024];
    GetWindowTextW(w->hwnd, wbuf, 1024);
    WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, buf, bufsize, NULL, NULL);
}

static int hook_widget_visible(int handle) {
    Widget* w = widget_at(handle);
    return (w && IsWindowVisible(w->hwnd)) ? 1 : 0;
}

static int hook_widget_parent(int handle) {
    Widget* w = widget_at(handle);
    if (!w) return 0;
    return handle_for_hwnd(GetParent(w->hwnd));
}

static int hook_toggle_active(int handle) {
    Widget* w = widget_at(handle);
    if (!w || w->kind != WK_TOGGLE) return 0;
    return SendMessageW(w->hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0;
}

static double hook_slider_value(int handle) {
    Widget* w = widget_at(handle);
    return (w && w->kind == WK_SLIDER) ? w->u.slider.cur_v : 0.0;
}

static double hook_progressbar_fraction(int handle) {
    Widget* w = widget_at(handle);
    return (w && w->kind == WK_PROGRESSBAR) ? w->u.progressbar.fraction : 0.0;
}

// dispatch_action: sends the ctx to the UI thread via AE_WM_DRIVER and
// blocks until the WndProc fills in ctx->result. Runs on the server
// thread — SendMessageW is synchronous so no explicit wait is needed.
static LRESULT CALLBACK driver_host_proc(HWND hwnd, UINT msg,
                                         WPARAM wp, LPARAM lp) {
    if (msg == AE_WM_DRIVER) {
        AetherDriverActionCtx* ctx = (AetherDriverActionCtx*)lp;
        if (ctx->action == AETHER_DRV_SET_STATE) {
            aether_ui_state_set(ctx->handle, ctx->dval);
            ctx->result = 0;
            ctx->done = 1;
            return 0;
        }
        Widget* w = widget_at(ctx->handle);
        if (!w) { ctx->result = 3; ctx->done = 1; return 0; }
        if (ctx->handle == aether_ui_test_server_banner_handle()) {
            ctx->result = 2; ctx->done = 1; return 0;
        }
        if (aether_ui_test_server_is_sealed(ctx->handle)) {
            ctx->result = 1; ctx->done = 1; return 0;
        }
        switch (ctx->action) {
            case AETHER_DRV_CLICK:
                if (w->kind == WK_BUTTON) invoke_closure(w->on_click);
                break;
            case AETHER_DRV_SET_TEXT:
                if (w->kind == WK_TEXT || w->kind == WK_TEXTFIELD
                    || w->kind == WK_SECUREFIELD || w->kind == WK_TEXTAREA) {
                    SetWindowTextW(w->hwnd, utf8_to_wide(ctx->sval));
                }
                break;
            case AETHER_DRV_TOGGLE:
                if (w->kind == WK_TOGGLE) {
                    int cur = aether_ui_toggle_get_active(ctx->handle);
                    aether_ui_toggle_set_active(ctx->handle, !cur);
                    invoke_closure(w->on_change);
                }
                break;
            case AETHER_DRV_SET_VALUE:
                if (w->kind == WK_SLIDER)
                    aether_ui_slider_set_value(ctx->handle, ctx->dval);
                else if (w->kind == WK_PROGRESSBAR)
                    aether_ui_progressbar_set_fraction(ctx->handle, ctx->dval);
                break;
            default: break;
        }
        ctx->result = 0;
        ctx->done = 1;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void hook_dispatch_action(AetherDriverActionCtx* ctx) {
    SendMessageW(driver_host_hwnd, AE_WM_DRIVER, 0, (LPARAM)ctx);
}

// List direct children of a widget. Returns the number written; -1 if
// the widget itself wasn't found.
static int hook_widget_children(int handle, int* out, int max) {
    Widget* w = widget_at(handle);
    if (!w) return -1;
    int n = 0;
    for (HWND c = GetWindow(w->hwnd, GW_CHILD); c && n < max;
         c = GetWindow(c, GW_HWNDNEXT)) {
        int ch = handle_for_hwnd(c);
        if (ch > 0) {
            if (out) out[n] = ch;
            n++;
        }
    }
    return n;
}

// Screenshot the app's first window to a PNG in memory.
// Uses BitBlt + GDI+ (via the same flat-API binding we use for images).
__declspec(dllimport) int __stdcall GdipCreateBitmapFromHBITMAP(
    HBITMAP hbm, HPALETTE pal, void** bitmap);
__declspec(dllimport) int __stdcall GdipSaveImageToStream(
    void* image, void* stream, const GUID* clsid, const void* params);
__declspec(dllimport) int __stdcall GdipGetImageEncodersSize(
    unsigned int* num_encoders, unsigned int* size);
__declspec(dllimport) int __stdcall GdipGetImageEncoders(
    unsigned int num_encoders, unsigned int size, void* encoders);

static int hook_screenshot_png(unsigned char** out_data, size_t* out_len) {
    if (app_count == 0) return -1;
    HWND hwnd = apps[0].hwnd;
    if (!hwnd) return -1;
    RECT r;
    if (!GetClientRect(hwnd, &r)) return -1;
    int w = r.right - r.left, h = r.bottom - r.top;
    if (w <= 0 || h <= 0) return -1;

    HDC src = GetDC(hwnd);
    HDC mem = CreateCompatibleDC(src);
    HBITMAP bmp = CreateCompatibleBitmap(src, w, h);
    HGDIOBJ old = SelectObject(mem, bmp);
    BitBlt(mem, 0, 0, w, h, src, 0, 0, SRCCOPY);
    SelectObject(mem, old);

    ensure_gdiplus();
    void* gdi_bitmap = NULL;
    int rc = -1;
    if (gdiplus_started
        && GdipCreateBitmapFromHBITMAP(bmp, NULL, &gdi_bitmap) == 0
        && gdi_bitmap) {
        // Serialize to an IStream, then copy its bytes out.
        IStream* stream = NULL;
        if (CreateStreamOnHGlobal(NULL, TRUE, &stream) == S_OK && stream) {
            // PNG codec CLSID {557CF406-1A04-11D3-9A73-0000F81EF32E}
            GUID png_clsid = {0x557cf406, 0x1a04, 0x11d3,
                              {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};
            if (GdipSaveImageToStream(gdi_bitmap, stream,
                                       &png_clsid, NULL) == 0) {
                HGLOBAL hg;
                if (GetHGlobalFromStream(stream, &hg) == S_OK) {
                    size_t sz = GlobalSize(hg);
                    void* src_ptr = GlobalLock(hg);
                    if (src_ptr && sz > 0) {
                        *out_data = (unsigned char*)malloc(sz);
                        memcpy(*out_data, src_ptr, sz);
                        *out_len = sz;
                        rc = 0;
                    }
                    GlobalUnlock(hg);
                }
            }
            stream->lpVtbl->Release(stream);
        }
        GdipDisposeImage(gdi_bitmap);
    }

    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(hwnd, src);
    return rc;
}

static const AetherDriverHooks win32_driver_hooks = {
    .widget_count         = hook_widget_count,
    .widget_type          = hook_widget_type,
    .widget_text_into     = hook_widget_text_into,
    .widget_visible       = hook_widget_visible,
    .widget_parent        = hook_widget_parent,
    .toggle_active        = hook_toggle_active,
    .slider_value         = hook_slider_value,
    .progressbar_fraction = hook_progressbar_fraction,
    .dispatch_action      = hook_dispatch_action,
    .widget_children      = hook_widget_children,
    .screenshot_png       = hook_screenshot_png,
};

void aether_ui_enable_test_server_impl(int port, int root_handle) {
    // Create a "Under Remote Control" banner, style it red+bold, seal it,
    // and hoist it to the top of the root stack.
    int banner = aether_ui_text_create("Under Remote Control");
    Widget* bw = widget_at(banner);
    if (bw) {
        HFONT base = (HFONT)SendMessageW(bw->hwnd, WM_GETFONT, 0, 0);
        LOGFONTW lf;
        GetObjectW(base, sizeof(lf), &lf);
        lf.lfWeight = FW_BOLD;
        HFONT bold = CreateFontIndirectW(&lf);
        SendMessageW(bw->hwnd, WM_SETFONT, (WPARAM)bold, TRUE);
        bw->custom_font = bold;
        bw->fg.has_value = 1;
        bw->fg.color = RGB(200, 0, 0);
    }
    Widget* root = widget_at(root_handle);
    if (root && bw && root->kind == WK_VSTACK) {
        SetParent(bw->hwnd, root->hwnd);
        LONG_PTR st = GetWindowLongPtrW(bw->hwnd, GWL_STYLE);
        SetWindowLongPtrW(bw->hwnd, GWL_STYLE, st | WS_CHILD | WS_VISIBLE);
        SetWindowPos(bw->hwnd, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        stack_do_layout(root->hwnd);
    }
    aether_ui_test_server_set_banner(banner);
    aether_ui_seal_widget_impl(banner); // banner is not automatable

    // Hidden window that receives AE_WM_DRIVER on the UI thread.
    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = driver_host_proc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"AetherUIDriverHost";
    RegisterClassExW(&wc);
    driver_host_hwnd = CreateWindowExW(0, L"AetherUIDriverHost", L"", 0,
        0, 0, 0, 0, HWND_MESSAGE, NULL, GetModuleHandleW(NULL), NULL);

    aether_ui_test_server_start(port, &win32_driver_hooks);
}
