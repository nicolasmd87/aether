// Aether UI — GTK4 backend for Aether
// Port of aether-ui-gtk4 (Rust) to C, calling the GTK4 C API directly.
//
// This file is compiled separately and linked with Aether programs via:
//   ae build app.ae --extra contrib/aether_ui/aether_ui_gtk4.c $(pkg-config --cflags --libs gtk4)

#include "aether_ui_gtk4.h"
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// Closure struct — must match Aether codegen's _AeClosure layout.
// box_closure() returns a malloc'd copy of this struct.
// ---------------------------------------------------------------------------
typedef struct {
    void (*fn)(void);
    void* env;
} AeClosure;

// ---------------------------------------------------------------------------
// GTK4 initialization — must be called before creating any widgets.
// ---------------------------------------------------------------------------
static int gtk_initialized = 0;

static void ensure_gtk_init(void) {
    if (!gtk_initialized) {
        gtk_init();
        gtk_initialized = 1;
    }
}

// ---------------------------------------------------------------------------
// Widget registry — flat array of GtkWidget*, 1-based handles.
// ---------------------------------------------------------------------------
static GtkWidget** widgets = NULL;
static int widget_count = 0;
static int widget_capacity = 0;

int aether_ui_register_widget(void* widget) {
    if (widget_count >= widget_capacity) {
        widget_capacity = widget_capacity == 0 ? 64 : widget_capacity * 2;
        widgets = realloc(widgets, sizeof(GtkWidget*) * widget_capacity);
    }
    widgets[widget_count] = (GtkWidget*)widget;
    widget_count++;
    return widget_count; // 1-based
}

void* aether_ui_get_widget(int handle) {
    if (handle < 1 || handle > widget_count) return NULL;
    return widgets[handle - 1];
}

// ---------------------------------------------------------------------------
// Reactive state — flat array of doubles, 1-based handles.
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
        state_values = realloc(state_values, sizeof(double) * state_capacity);
    }
    state_values[state_count] = initial;
    state_count++;
    return state_count; // 1-based
}

double aether_ui_state_get(int handle) {
    if (handle < 1 || handle > state_count) return 0.0;
    return state_values[handle - 1];
}

static void update_text_bindings(int state_handle);

void aether_ui_state_set(int handle, double value) {
    if (handle < 1 || handle > state_count) return;
    state_values[handle - 1] = value;
    update_text_bindings(handle);
}

void aether_ui_state_bind_text(int state_handle, int text_handle,
                               const char* prefix, const char* suffix) {
    if (text_binding_count >= text_binding_capacity) {
        text_binding_capacity = text_binding_capacity == 0 ? 32 : text_binding_capacity * 2;
        text_bindings = realloc(text_bindings, sizeof(TextBinding) * text_binding_capacity);
    }
    TextBinding* b = &text_bindings[text_binding_count++];
    b->state_handle = state_handle;
    b->text_handle = text_handle;
    b->prefix = prefix ? strdup(prefix) : strdup("");
    b->suffix = suffix ? strdup(suffix) : strdup("");

    double val = aether_ui_state_get(state_handle);
    char buf[256];
    if (val == (int)val) {
        snprintf(buf, sizeof(buf), "%s%d%s", b->prefix, (int)val, b->suffix);
    } else {
        snprintf(buf, sizeof(buf), "%s%.2f%s", b->prefix, val, b->suffix);
    }
    aether_ui_text_set_string(text_handle, buf);
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

// ---------------------------------------------------------------------------
// App lifecycle
// ---------------------------------------------------------------------------
typedef struct {
    char* title;
    int width;
    int height;
    int root_handle;
    GtkApplication* gtk_app;
} AppEntry;

static AppEntry* apps = NULL;
static int app_count = 0;
static int app_capacity = 0;

static void on_activate(GtkApplication* gtk_app, gpointer user_data) {
    AppEntry* entry = (AppEntry*)user_data;
    GtkWidget* window = gtk_application_window_new(gtk_app);
    gtk_window_set_title(GTK_WINDOW(window), entry->title);
    gtk_window_set_default_size(GTK_WINDOW(window), entry->width, entry->height);

    if (entry->root_handle > 0) {
        GtkWidget* root = aether_ui_get_widget(entry->root_handle);
        if (root) {
            gtk_window_set_child(GTK_WINDOW(window), root);
        }
    }

    gtk_window_present(GTK_WINDOW(window));
}

int aether_ui_app_create(const char* title, int width, int height) {
    if (app_count >= app_capacity) {
        app_capacity = app_capacity == 0 ? 4 : app_capacity * 2;
        apps = realloc(apps, sizeof(AppEntry) * app_capacity);
    }
    AppEntry* e = &apps[app_count];
    e->title = strdup(title);
    e->width = width;
    e->height = height;
    e->root_handle = 0;
    e->gtk_app = NULL;
    app_count++;
    return app_count; // 1-based
}

void aether_ui_app_set_body(int app_handle, int root_handle) {
    if (app_handle < 1 || app_handle > app_count) return;
    apps[app_handle - 1].root_handle = root_handle;
}

void aether_ui_app_run_raw(int app_handle) {
    if (app_handle < 1 || app_handle > app_count) return;
    AppEntry* e = &apps[app_handle - 1];
    e->gtk_app = gtk_application_new("dev.aether.ui",
        G_APPLICATION_DEFAULT_FLAGS | G_APPLICATION_NON_UNIQUE);
    g_signal_connect(e->gtk_app, "activate", G_CALLBACK(on_activate), e);
    g_application_run(G_APPLICATION(e->gtk_app), 0, NULL);
    g_object_unref(e->gtk_app);
}

// ---------------------------------------------------------------------------
// Widget creation
// ---------------------------------------------------------------------------

int aether_ui_text_create(const char* text) {
    ensure_gtk_init();
    GtkWidget* label = gtk_label_new(text ? text : "");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    return aether_ui_register_widget(label);
}

void aether_ui_text_set_string(int handle, const char* text) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (w && GTK_IS_LABEL(w)) {
        gtk_label_set_text(GTK_LABEL(w), text ? text : "");
    }
}

static void on_button_clicked(GtkButton* btn, gpointer data) {
    AeClosure* c = (AeClosure*)data;
    if (c && c->fn) {
        ((void(*)(void*))c->fn)(c->env);
    }
}

// Plain button — no callback. Use aether_ui_set_onclick to add one later.
int aether_ui_button_create_plain(const char* label) {
    ensure_gtk_init();
    GtkWidget* btn = gtk_button_new_with_label(label ? label : "");
    return aether_ui_register_widget(btn);
}

// Set click handler on an existing button (or any widget via GestureClick).
void aether_ui_set_onclick_ctx(void* ctx, void* boxed_closure) {
    int handle = (int)(intptr_t)ctx;
    GtkWidget* w = aether_ui_get_widget(handle);
    if (!w || !boxed_closure) return;
    if (GTK_IS_BUTTON(w)) {
        g_signal_connect(w, "clicked", G_CALLBACK(on_button_clicked), boxed_closure);
    }
}

// Styling functions that accept _ctx (void*) instead of int handle.
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

int aether_ui_button_create(const char* label, void* boxed_closure) {
    ensure_gtk_init();
    GtkWidget* btn = gtk_button_new_with_label(label ? label : "");
    if (boxed_closure) {
        g_signal_connect(btn, "clicked", G_CALLBACK(on_button_clicked), boxed_closure);
    }
    return aether_ui_register_widget(btn);
}

int aether_ui_vstack_create(int spacing) {
    ensure_gtk_init();
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, spacing);
    return aether_ui_register_widget(box);
}

int aether_ui_hstack_create(int spacing) {
    ensure_gtk_init();
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, spacing);
    return aether_ui_register_widget(box);
}

int aether_ui_spacer_create(void) {
    ensure_gtk_init();
    GtkWidget* spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(spacer, TRUE);
    gtk_widget_set_hexpand(spacer, TRUE);
    return aether_ui_register_widget(spacer);
}

int aether_ui_divider_create(void) {
    ensure_gtk_init();
    GtkWidget* sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    return aether_ui_register_widget(sep);
}

// ---------------------------------------------------------------------------
// Input widgets (Group 2)
// ---------------------------------------------------------------------------

// Textfield — single-line text input with on_change callback.
// Callback receives the new text: ((void(*)(void*, const char*))fn)(env, text)
static void on_entry_changed(GtkEditable* editable, gpointer data) {
    AeClosure* c = (AeClosure*)data;
    if (c && c->fn) {
        const char* text = gtk_editable_get_text(editable);
        ((void(*)(void*, const char*))c->fn)(c->env, text ? text : "");
    }
}

int aether_ui_textfield_create(const char* placeholder, void* boxed_closure) {
    ensure_gtk_init();
    GtkWidget* entry = gtk_entry_new();
    if (placeholder && *placeholder) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(entry), placeholder);
    }
    if (boxed_closure) {
        g_signal_connect(entry, "changed", G_CALLBACK(on_entry_changed), boxed_closure);
    }
    return aether_ui_register_widget(entry);
}

void aether_ui_textfield_set_text(int handle, const char* text) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (w && GTK_IS_ENTRY(w)) {
        GtkEntryBuffer* buf = gtk_entry_get_buffer(GTK_ENTRY(w));
        gtk_entry_buffer_set_text(buf, text ? text : "", -1);
    }
}

const char* aether_ui_textfield_get_text(int handle) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (w && GTK_IS_ENTRY(w)) {
        GtkEntryBuffer* buf = gtk_entry_get_buffer(GTK_ENTRY(w));
        return gtk_entry_buffer_get_text(buf);
    }
    return "";
}

// Securefield — password entry (masked input).
int aether_ui_securefield_create(const char* placeholder, void* boxed_closure) {
    ensure_gtk_init();
    GtkWidget* entry = gtk_password_entry_new();
    gtk_password_entry_set_show_peek_icon(GTK_PASSWORD_ENTRY(entry), TRUE);
    if (placeholder && *placeholder) {
        // GtkPasswordEntry doesn't have placeholder API directly;
        // set via the internal GtkText child
        GtkWidget* text_widget = gtk_widget_get_first_child(entry);
        if (text_widget && GTK_IS_TEXT(text_widget)) {
            gtk_text_set_placeholder_text(GTK_TEXT(text_widget), placeholder);
        }
    }
    if (boxed_closure) {
        g_signal_connect(entry, "changed", G_CALLBACK(on_entry_changed), boxed_closure);
    }
    return aether_ui_register_widget(entry);
}

// Toggle — checkbox with label.
// Callback receives 1 (active) or 0 (inactive).
static void on_toggle_changed(GtkCheckButton* btn, GParamSpec* pspec, gpointer data) {
    (void)pspec;
    AeClosure* c = (AeClosure*)data;
    if (c && c->fn) {
        int active = gtk_check_button_get_active(btn) ? 1 : 0;
        ((void(*)(void*, intptr_t))c->fn)(c->env, (intptr_t)active);
    }
}

int aether_ui_toggle_create(const char* label, void* boxed_closure) {
    ensure_gtk_init();
    GtkWidget* check = gtk_check_button_new_with_label(label ? label : "");
    if (boxed_closure) {
        g_signal_connect(check, "notify::active", G_CALLBACK(on_toggle_changed), boxed_closure);
    }
    return aether_ui_register_widget(check);
}

void aether_ui_toggle_set_active(int handle, int active) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (w && GTK_IS_CHECK_BUTTON(w)) {
        gtk_check_button_set_active(GTK_CHECK_BUTTON(w), active != 0);
    }
}

int aether_ui_toggle_get_active(int handle) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (w && GTK_IS_CHECK_BUTTON(w)) {
        return gtk_check_button_get_active(GTK_CHECK_BUTTON(w)) ? 1 : 0;
    }
    return 0;
}

// Slider — horizontal scale with min/max/initial and on_change callback.
// Callback receives the new double value.
static void on_slider_changed(GtkRange* range, gpointer data) {
    AeClosure* c = (AeClosure*)data;
    if (c && c->fn) {
        double val = gtk_range_get_value(range);
        ((void(*)(void*, double))c->fn)(c->env, val);
    }
}

int aether_ui_slider_create(double min_val, double max_val,
                            double initial, void* boxed_closure) {
    ensure_gtk_init();
    GtkAdjustment* adj = gtk_adjustment_new(initial, min_val, max_val,
                                             1.0, 10.0, 0.0);
    GtkWidget* scale = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, adj);
    gtk_scale_set_draw_value(GTK_SCALE(scale), TRUE);
    gtk_widget_set_hexpand(scale, TRUE);
    if (boxed_closure) {
        g_signal_connect(scale, "value-changed", G_CALLBACK(on_slider_changed), boxed_closure);
    }
    return aether_ui_register_widget(scale);
}

void aether_ui_slider_set_value(int handle, double value) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (w && GTK_IS_SCALE(w)) {
        gtk_range_set_value(GTK_RANGE(w), value);
    }
}

double aether_ui_slider_get_value(int handle) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (w && GTK_IS_SCALE(w)) {
        return gtk_range_get_value(GTK_RANGE(w));
    }
    return 0.0;
}

// Picker — dropdown / combo box.
// Callback receives the selected index (int).
static void on_picker_changed(GtkDropDown* dropdown, GParamSpec* pspec, gpointer data) {
    (void)pspec;
    AeClosure* c = (AeClosure*)data;
    if (c && c->fn) {
        guint idx = gtk_drop_down_get_selected(dropdown);
        ((void(*)(void*, intptr_t))c->fn)(c->env, (intptr_t)idx);
    }
}

int aether_ui_picker_create(void* boxed_closure) {
    ensure_gtk_init();
    GtkStringList* model = gtk_string_list_new(NULL);
    GtkWidget* dropdown = gtk_drop_down_new(G_LIST_MODEL(model), NULL);
    if (boxed_closure) {
        g_signal_connect(dropdown, "notify::selected",
                         G_CALLBACK(on_picker_changed), boxed_closure);
    }
    return aether_ui_register_widget(dropdown);
}

void aether_ui_picker_add_item(int handle, const char* item) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (w && GTK_IS_DROP_DOWN(w)) {
        GListModel* model = gtk_drop_down_get_model(GTK_DROP_DOWN(w));
        if (GTK_IS_STRING_LIST(model)) {
            gtk_string_list_append(GTK_STRING_LIST(model), item ? item : "");
        }
    }
}

void aether_ui_picker_set_selected(int handle, int index) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (w && GTK_IS_DROP_DOWN(w)) {
        gtk_drop_down_set_selected(GTK_DROP_DOWN(w), (guint)index);
    }
}

int aether_ui_picker_get_selected(int handle) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (w && GTK_IS_DROP_DOWN(w)) {
        return (int)gtk_drop_down_get_selected(GTK_DROP_DOWN(w));
    }
    return 0;
}

// Textarea — multi-line text input in a scrolled window.
// Callback receives the full text content.
static void on_textbuffer_changed(GtkTextBuffer* buffer, gpointer data) {
    AeClosure* c = (AeClosure*)data;
    if (c && c->fn) {
        GtkTextIter start, end;
        gtk_text_buffer_get_start_iter(buffer, &start);
        gtk_text_buffer_get_end_iter(buffer, &end);
        char* text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
        ((void(*)(void*, const char*))c->fn)(c->env, text ? text : "");
        g_free(text);
    }
}

int aether_ui_textarea_create(const char* placeholder, void* boxed_closure) {
    ensure_gtk_init();
    GtkWidget* textview = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textview), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(textview), 4);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(textview), 4);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(textview), 4);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(textview), 4);

    // Wrap in a scrolled window
    GtkWidget* scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrolled), 80);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), textview);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_widget_set_hexpand(scrolled, TRUE);

    if (boxed_closure) {
        GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
        g_signal_connect(buf, "changed", G_CALLBACK(on_textbuffer_changed), boxed_closure);
    }

    // Store the textview handle separately so set/get text works on it
    // Register the scrolled window as the visible widget
    int scroll_handle = aether_ui_register_widget(scrolled);
    // Also register the textview for direct text access
    aether_ui_register_widget(textview);
    // The textview handle is scroll_handle + 1
    return scroll_handle;
}

void aether_ui_textarea_set_text(int handle, const char* text) {
    // Textarea handle points to scrolled window; textview is handle+1
    GtkWidget* w = aether_ui_get_widget(handle + 1);
    if (w && GTK_IS_TEXT_VIEW(w)) {
        GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(w));
        gtk_text_buffer_set_text(buf, text ? text : "", -1);
    }
}

char* aether_ui_textarea_get_text(int handle) {
    GtkWidget* w = aether_ui_get_widget(handle + 1);
    if (w && GTK_IS_TEXT_VIEW(w)) {
        GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(w));
        GtkTextIter start, end;
        gtk_text_buffer_get_start_iter(buf, &start);
        gtk_text_buffer_get_end_iter(buf, &end);
        return gtk_text_buffer_get_text(buf, &start, &end, FALSE);
    }
    return strdup("");
}

// Scrollview container
int aether_ui_scrollview_create(void) {
    ensure_gtk_init();
    GtkWidget* scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_widget_set_hexpand(scrolled, TRUE);
    return aether_ui_register_widget(scrolled);
}

// Progressbar
int aether_ui_progressbar_create(double fraction) {
    ensure_gtk_init();
    GtkWidget* bar = gtk_progress_bar_new();
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(bar), fraction);
    gtk_widget_set_hexpand(bar, TRUE);
    return aether_ui_register_widget(bar);
}

void aether_ui_progressbar_set_fraction(int handle, double fraction) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (w && GTK_IS_PROGRESS_BAR(w)) {
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(w), fraction);
    }
}

// ---------------------------------------------------------------------------
// Layout containers (Group 3)
// ---------------------------------------------------------------------------

// ZStack — overlay container where children stack on top of each other.
int aether_ui_zstack_create(void) {
    ensure_gtk_init();
    GtkWidget* overlay = gtk_overlay_new();
    gtk_widget_set_vexpand(overlay, TRUE);
    gtk_widget_set_hexpand(overlay, TRUE);
    return aether_ui_register_widget(overlay);
}

// Form — vertical box with padding, suitable for form layouts.
int aether_ui_form_create(void) {
    ensure_gtk_init();
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_top(box, 20);
    gtk_widget_set_margin_bottom(box, 20);
    gtk_widget_set_margin_start(box, 20);
    gtk_widget_set_margin_end(box, 20);
    return aether_ui_register_widget(box);
}

// Form section — frame with title and inner content box.
int aether_ui_form_section_create(const char* title) {
    ensure_gtk_init();
    GtkWidget* frame = gtk_frame_new(title);
    GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_top(inner, 8);
    gtk_widget_set_margin_bottom(inner, 8);
    gtk_widget_set_margin_start(inner, 8);
    gtk_widget_set_margin_end(inner, 8);
    gtk_frame_set_child(GTK_FRAME(frame), inner);
    // Register both: frame for tree, inner for child appending
    int frame_handle = aether_ui_register_widget(frame);
    aether_ui_register_widget(inner);
    return frame_handle;
}

// NavStack — page stack with slide transitions.
static int navstack_page_counts[64] = {0};

int aether_ui_navstack_create(void) {
    ensure_gtk_init();
    GtkWidget* stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_set_transition_duration(GTK_STACK(stack), 250);
    gtk_widget_set_vexpand(stack, TRUE);
    gtk_widget_set_hexpand(stack, TRUE);
    int handle = aether_ui_register_widget(stack);
    if (handle <= 64) navstack_page_counts[handle - 1] = 0;
    return handle;
}

void aether_ui_navstack_push(int handle, const char* title, int body_handle) {
    GtkWidget* stack = aether_ui_get_widget(handle);
    GtkWidget* body = aether_ui_get_widget(body_handle);
    if (!stack || !body || !GTK_IS_STACK(stack)) return;
    int idx = (handle <= 64) ? navstack_page_counts[handle - 1]++ : 0;
    char name[32];
    snprintf(name, sizeof(name), "page_%d", idx);
    gtk_stack_add_named(GTK_STACK(stack), body, name);
    gtk_stack_set_visible_child(GTK_STACK(stack), body);
    (void)title;
}

void aether_ui_navstack_pop(int handle) {
    GtkWidget* stack = aether_ui_get_widget(handle);
    if (!stack || !GTK_IS_STACK(stack)) return;
    if (handle > 64) return;
    int count = navstack_page_counts[handle - 1];
    if (count <= 1) return;
    char name[32];
    snprintf(name, sizeof(name), "page_%d", count - 1);
    GtkWidget* page = gtk_stack_get_child_by_name(GTK_STACK(stack), name);
    if (page) {
        // Show previous page
        char prev_name[32];
        snprintf(prev_name, sizeof(prev_name), "page_%d", count - 2);
        GtkWidget* prev = gtk_stack_get_child_by_name(GTK_STACK(stack), prev_name);
        if (prev) gtk_stack_set_visible_child(GTK_STACK(stack), prev);
        gtk_stack_remove(GTK_STACK(stack), page);
        navstack_page_counts[handle - 1]--;
    }
}

// ---------------------------------------------------------------------------
// Styling (Group 4)
// ---------------------------------------------------------------------------

// Apply CSS scoped to a single widget via a unique class name.
// Each widget gets "aui-{handle}" as a CSS class, and the CSS rule
// targets that class specifically — no global side effects.
// Single global CSS provider — accumulates all styling rules.
static char* global_css_buf = NULL;
static int global_css_len = 0;
static int global_css_cap = 0;
static GtkCssProvider* global_css_provider = NULL;

static void global_css_append(const char* rule) {
    int rlen = (int)strlen(rule);
    if (global_css_len + rlen + 2 > global_css_cap) {
        global_css_cap = (global_css_cap == 0) ? 4096 : global_css_cap * 2;
        global_css_buf = realloc(global_css_buf, global_css_cap);
    }
    memcpy(global_css_buf + global_css_len, rule, rlen);
    global_css_len += rlen;
    global_css_buf[global_css_len++] = '\n';
    global_css_buf[global_css_len] = '\0';
}

static void global_css_reload(void) {
    if (!global_css_buf) return;
    ensure_gtk_init();
    GdkDisplay* display = gdk_display_get_default();
    if (!display) return;
    if (!global_css_provider) {
        global_css_provider = gtk_css_provider_new();
        gtk_style_context_add_provider_for_display(
            display, GTK_STYLE_PROVIDER(global_css_provider),
            GTK_STYLE_PROVIDER_PRIORITY_USER);
    }
    gtk_css_provider_load_from_string(global_css_provider, global_css_buf);
}

static void aether_ui_apply_css(int handle, GtkWidget* widget, const char* property_css) {
    char class_name[32];
    snprintf(class_name, sizeof(class_name), "aui-%d", handle);
    gtk_widget_add_css_class(widget, class_name);

    char css[512];
    snprintf(css, sizeof(css), ".%s { %s }", class_name, property_css);
    global_css_append(css);
    global_css_reload();
}

void aether_ui_set_bg_color(int handle, double r, double g, double b, double a) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (!w) return;

    char cls[32];
    snprintf(cls, sizeof(cls), "aui-bg-%d", handle);
    gtk_widget_add_css_class(w, cls);
    if (GTK_IS_BUTTON(w)) gtk_widget_add_css_class(w, "flat");

    char css[512];
    int ri = (int)(r * 255), gi = (int)(g * 255), bi = (int)(b * 255);
    if (GTK_IS_BUTTON(w)) {
        snprintf(css, sizeof(css),
            "button.flat.%s { background-color: rgb(%d,%d,%d); background-image: none; }\n"
            "button.flat.%s:hover { background-color: rgb(%d,%d,%d); background-image: none; }",
            cls, ri, gi, bi, cls, ri, gi, bi);
    } else {
        snprintf(css, sizeof(css),
            ".%s { background: rgba(%d,%d,%d,%.2f); }",
            cls, ri, gi, bi, a);
    }
    global_css_append(css);
    global_css_reload();
}

void aether_ui_set_bg_gradient(int handle,
                               double r1, double g1, double b1,
                               double r2, double g2, double b2,
                               int vertical) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (!w) return;
    const char* dir = vertical ? "to bottom" : "to right";
    char prop[256];
    snprintf(prop, sizeof(prop),
        "background: linear-gradient(%s, rgb(%d,%d,%d), rgb(%d,%d,%d));",
        dir,
        (int)(r1 * 255), (int)(g1 * 255), (int)(b1 * 255),
        (int)(r2 * 255), (int)(g2 * 255), (int)(b2 * 255));
    aether_ui_apply_css(handle, w, prop);
}

void aether_ui_set_text_color(int handle, double r, double g, double b) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (!w) return;
    if (GTK_IS_LABEL(w)) {
        PangoAttrList* attrs = pango_attr_list_new();
        PangoAttribute* attr = pango_attr_foreground_new(
            (guint16)(r * 65535), (guint16)(g * 65535), (guint16)(b * 65535));
        pango_attr_list_insert(attrs, attr);
        gtk_label_set_attributes(GTK_LABEL(w), attrs);
        pango_attr_list_unref(attrs);
    } else {
        char prop[128];
        snprintf(prop, sizeof(prop), "color: rgb(%d,%d,%d);",
            (int)(r * 255), (int)(g * 255), (int)(b * 255));
        aether_ui_apply_css(handle, w, prop);
    }
}

void aether_ui_set_font_size(int handle, double size) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (!w || !GTK_IS_LABEL(w)) return;
    PangoAttrList* attrs = gtk_label_get_attributes(GTK_LABEL(w));
    if (!attrs) attrs = pango_attr_list_new();
    else attrs = pango_attr_list_copy(attrs);
    PangoAttribute* attr = pango_attr_size_new((int)(size * PANGO_SCALE));
    pango_attr_list_insert(attrs, attr);
    gtk_label_set_attributes(GTK_LABEL(w), attrs);
    pango_attr_list_unref(attrs);
}

void aether_ui_set_font_bold(int handle, int bold) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (!w || !GTK_IS_LABEL(w)) return;
    PangoAttrList* attrs = gtk_label_get_attributes(GTK_LABEL(w));
    if (!attrs) attrs = pango_attr_list_new();
    else attrs = pango_attr_list_copy(attrs);
    PangoAttribute* attr = pango_attr_weight_new(
        bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL);
    pango_attr_list_insert(attrs, attr);
    gtk_label_set_attributes(GTK_LABEL(w), attrs);
    pango_attr_list_unref(attrs);
}

void aether_ui_set_corner_radius(int handle, double radius) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (!w) return;
    char prop[64];
    snprintf(prop, sizeof(prop), "border-radius: %dpx;", (int)radius);
    aether_ui_apply_css(handle, w, prop);
}

void aether_ui_set_edge_insets(int handle, double top, double right,
                               double bottom, double left) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (!w) return;
    char prop[128];
    snprintf(prop, sizeof(prop), "padding: %dpx %dpx %dpx %dpx;",
        (int)top, (int)right, (int)bottom, (int)left);
    aether_ui_apply_css(handle, w, prop);
}

void aether_ui_set_width(int handle, int width) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (w) gtk_widget_set_size_request(w, width, -1);
}

void aether_ui_set_height(int handle, int height) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (!w) return;
    char prop[128];
    snprintf(prop, sizeof(prop), "min-height: %dpx; max-height: %dpx;", height, height);
    aether_ui_apply_css(handle, w, prop);
    gtk_widget_set_vexpand(w, FALSE);
    gtk_widget_set_valign(w, GTK_ALIGN_CENTER);
}

void aether_ui_set_opacity(int handle, double opacity) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (w) gtk_widget_set_opacity(w, opacity);
}

void aether_ui_set_enabled(int handle, int enabled) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (w) gtk_widget_set_sensitive(w, enabled != 0);
}

void aether_ui_set_tooltip(int handle, const char* text) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (w) gtk_widget_set_tooltip_text(w, text);
}

void aether_ui_set_distribution(int handle, int distribution) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (w && GTK_IS_BOX(w)) {
        gtk_box_set_homogeneous(GTK_BOX(w), distribution == 1);
    }
}

void aether_ui_set_alignment(int handle, int alignment) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (!w || !GTK_IS_BOX(w)) return;
    int is_vertical = (gtk_orientable_get_orientation(GTK_ORIENTABLE(w))
                       == GTK_ORIENTATION_VERTICAL);
    GtkAlign align;
    if (is_vertical) {
        switch (alignment) {
            case 5: align = GTK_ALIGN_START; break;   // Leading
            case 9: align = GTK_ALIGN_CENTER; break;  // CenterX
            case 7: align = GTK_ALIGN_FILL; break;    // Fill
            default: align = GTK_ALIGN_FILL; break;
        }
        for (GtkWidget* c = gtk_widget_get_first_child(w); c;
             c = gtk_widget_get_next_sibling(c)) {
            gtk_widget_set_halign(c, align);
        }
    } else {
        switch (alignment) {
            case 3: align = GTK_ALIGN_START; break;   // Top
            case 12: align = GTK_ALIGN_CENTER; break; // CenterY
            case 4: align = GTK_ALIGN_END; break;     // Bottom
            default: align = GTK_ALIGN_FILL; break;
        }
        for (GtkWidget* c = gtk_widget_get_first_child(w); c;
             c = gtk_widget_get_next_sibling(c)) {
            gtk_widget_set_valign(c, align);
        }
    }
}

void aether_ui_match_parent_width(int handle) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (w) {
        gtk_widget_set_hexpand(w, TRUE);
        gtk_widget_set_halign(w, GTK_ALIGN_FILL);
    }
}

void aether_ui_match_parent_height(int handle) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (w) {
        gtk_widget_set_vexpand(w, TRUE);
        gtk_widget_set_valign(w, GTK_ALIGN_FILL);
    }
}

void aether_ui_set_margin(int handle, int top, int right, int bottom, int left) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (!w) return;
    gtk_widget_set_margin_top(w, top);
    gtk_widget_set_margin_end(w, right);
    gtk_widget_set_margin_bottom(w, bottom);
    gtk_widget_set_margin_start(w, left);
}

// ---------------------------------------------------------------------------
// System integration (Group 5)
// ---------------------------------------------------------------------------

// Alert dialog — modal message box with OK button.
void aether_ui_alert_impl(const char* title, const char* message) {
    ensure_gtk_init();
    GtkAlertDialog* dialog = gtk_alert_dialog_new("%s", message ? message : "");
    if (title) gtk_alert_dialog_set_detail(dialog, title);
    // Show on the active window if we have one, otherwise standalone
    gtk_alert_dialog_show(dialog, NULL);
    g_object_unref(dialog);
}

// File open dialog — blocks and returns the selected path, or "" if cancelled.
char* aether_ui_file_open(const char* title) {
    ensure_gtk_init();
    GtkFileDialog* dialog = gtk_file_dialog_new();
    if (title) gtk_file_dialog_set_title(dialog, title);

    // Use a synchronous approach via main loop iteration
    // For simplicity, use the legacy FileChooserDialog
    g_object_unref(dialog);

    // Fallback to GtkFileChooserNative for sync behavior
    // GTK4 file dialogs are async-only; return empty for now
    // TODO: implement async callback pattern
    return strdup("");
}

// Clipboard read/write
void aether_ui_clipboard_write_impl(const char* text) {
    ensure_gtk_init();
    GdkDisplay* display = gdk_display_get_default();
    if (!display) return;
    GdkClipboard* clipboard = gdk_display_get_clipboard(display);
    gdk_clipboard_set_text(clipboard, text ? text : "");
}

// Timer — schedule a repeating callback at interval_ms.
static gboolean on_timer_tick(gpointer data) {
    AeClosure* c = (AeClosure*)data;
    if (c && c->fn) {
        ((void(*)(void*))c->fn)(c->env);
    }
    return G_SOURCE_CONTINUE;
}

int aether_ui_timer_create_impl(int interval_ms, void* boxed_closure) {
    if (!boxed_closure || interval_ms <= 0) return 0;
    guint id = g_timeout_add((guint)interval_ms, on_timer_tick, boxed_closure);
    return (int)id;
}

void aether_ui_timer_cancel_impl(int timer_id) {
    if (timer_id > 0) g_source_remove((guint)timer_id);
}

// Open URL in default browser
void aether_ui_open_url_impl(const char* url) {
    if (!url) return;
    ensure_gtk_init();
    GtkUriLauncher* launcher = gtk_uri_launcher_new(url);
    gtk_uri_launcher_launch(launcher, NULL, NULL, NULL, NULL);
    g_object_unref(launcher);
}

// Dark mode detection
int aether_ui_dark_mode_check(void) {
    ensure_gtk_init();
    GtkSettings* settings = gtk_settings_get_default();
    if (!settings) return 0;
    gboolean dark = FALSE;
    g_object_get(settings, "gtk-application-prefer-dark-theme", &dark, NULL);
    if (dark) return 1;
    // Also check theme name
    char* theme = NULL;
    g_object_get(settings, "gtk-theme-name", &theme, NULL);
    if (theme) {
        int is_dark = (strstr(theme, "dark") != NULL || strstr(theme, "Dark") != NULL);
        g_free(theme);
        if (is_dark) return 1;
    }
    return 0;
}

// Multi-window support
typedef struct {
    GtkWindow* window;
    int root_handle;
} WindowEntry;

static WindowEntry* extra_windows = NULL;
static int extra_window_count = 0;
static int extra_window_capacity = 0;

int aether_ui_window_create_impl(const char* title, int width, int height) {
    ensure_gtk_init();
    if (extra_window_count >= extra_window_capacity) {
        extra_window_capacity = extra_window_capacity == 0 ? 8 : extra_window_capacity * 2;
        extra_windows = realloc(extra_windows, sizeof(WindowEntry) * extra_window_capacity);
    }
    GtkWidget* win = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(win), title ? title : "");
    gtk_window_set_default_size(GTK_WINDOW(win), width, height);
    WindowEntry* e = &extra_windows[extra_window_count++];
    e->window = GTK_WINDOW(win);
    e->root_handle = 0;
    return extra_window_count; // 1-based
}

void aether_ui_window_set_body_impl(int win_handle, int root_handle) {
    if (win_handle < 1 || win_handle > extra_window_count) return;
    WindowEntry* e = &extra_windows[win_handle - 1];
    e->root_handle = root_handle;
    GtkWidget* root = aether_ui_get_widget(root_handle);
    if (root) gtk_window_set_child(e->window, root);
}

void aether_ui_window_show_impl(int win_handle) {
    if (win_handle < 1 || win_handle > extra_window_count) return;
    gtk_window_present(extra_windows[win_handle - 1].window);
}

void aether_ui_window_close_impl(int win_handle) {
    if (win_handle < 1 || win_handle > extra_window_count) return;
    gtk_window_close(extra_windows[win_handle - 1].window);
}

// Sheet (modal dialog window)
int aether_ui_sheet_create_impl(const char* title, int width, int height) {
    ensure_gtk_init();
    GtkWidget* win = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(win), title ? title : "");
    gtk_window_set_default_size(GTK_WINDOW(win), width, height);
    gtk_window_set_modal(GTK_WINDOW(win), TRUE);
    gtk_window_set_resizable(GTK_WINDOW(win), TRUE);
    return aether_ui_register_widget(win);
}

void aether_ui_sheet_set_body_impl(int handle, int root_handle) {
    GtkWidget* win = aether_ui_get_widget(handle);
    GtkWidget* root = aether_ui_get_widget(root_handle);
    if (win && root && GTK_IS_WINDOW(win)) {
        gtk_window_set_child(GTK_WINDOW(win), root);
    }
}

void aether_ui_sheet_present_impl(int handle) {
    GtkWidget* win = aether_ui_get_widget(handle);
    if (win && GTK_IS_WINDOW(win)) {
        gtk_window_present(GTK_WINDOW(win));
    }
}

void aether_ui_sheet_dismiss_impl(int handle) {
    GtkWidget* win = aether_ui_get_widget(handle);
    if (win && GTK_IS_WINDOW(win)) {
        gtk_window_close(GTK_WINDOW(win));
    }
}

// Image widget
int aether_ui_image_create(const char* filepath) {
    ensure_gtk_init();
    GtkWidget* img;
    if (filepath && *filepath) {
        img = gtk_image_new_from_file(filepath);
    } else {
        img = gtk_image_new();
    }
    return aether_ui_register_widget(img);
}

void aether_ui_image_set_size(int handle, int width, int height) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (w && GTK_IS_IMAGE(w)) {
        gtk_image_set_pixel_size(GTK_IMAGE(w), width > height ? width : height);
        gtk_widget_set_size_request(w, width, height);
    }
}

// ---------------------------------------------------------------------------
// Canvas drawing + Events + Animation (Group 6)
// ---------------------------------------------------------------------------

// Canvas — command-buffer drawing via Cairo on a GtkDrawingArea.

typedef enum {
    CANVAS_BEGIN_PATH,
    CANVAS_MOVE_TO,
    CANVAS_LINE_TO,
    CANVAS_STROKE,
    CANVAS_FILL_RECT,
    CANVAS_CLEAR
} CanvasCmdType;

typedef struct {
    CanvasCmdType type;
    double x, y;              // MOVE_TO, LINE_TO coords
    double r, g, b, a;        // STROKE / FILL color
    double w, h;              // FILL_RECT width/height or STROKE line_width (in x)
} CanvasCmd;

typedef struct {
    CanvasCmd* cmds;
    int count;
    int capacity;
    int widget_handle;
} CanvasState;

static CanvasState* canvas_states = NULL;
static int canvas_state_count = 0;
static int canvas_state_capacity = 0;

static CanvasState* get_canvas_state(int canvas_id) {
    if (canvas_id < 1 || canvas_id > canvas_state_count) return NULL;
    return &canvas_states[canvas_id - 1];
}

static void canvas_add_cmd(int canvas_id, CanvasCmd cmd) {
    CanvasState* cs = get_canvas_state(canvas_id);
    if (!cs) return;
    if (cs->count >= cs->capacity) {
        cs->capacity = cs->capacity == 0 ? 64 : cs->capacity * 2;
        cs->cmds = realloc(cs->cmds, sizeof(CanvasCmd) * cs->capacity);
    }
    cs->cmds[cs->count++] = cmd;
}

static void canvas_draw_func(GtkDrawingArea* area, cairo_t* cr,
                              int width, int height, gpointer data) {
    (void)area; (void)width; (void)height;
    int canvas_id = (int)(intptr_t)data;
    CanvasState* cs = get_canvas_state(canvas_id);
    if (!cs) return;

    for (int i = 0; i < cs->count; i++) {
        CanvasCmd* c = &cs->cmds[i];
        switch (c->type) {
            case CANVAS_BEGIN_PATH:
                cairo_new_path(cr);
                break;
            case CANVAS_MOVE_TO:
                cairo_move_to(cr, c->x, c->y);
                break;
            case CANVAS_LINE_TO:
                cairo_line_to(cr, c->x, c->y);
                break;
            case CANVAS_STROKE:
                cairo_set_source_rgba(cr, c->r, c->g, c->b, c->a);
                cairo_set_line_width(cr, c->x); // line_width stored in x
                cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
                cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
                cairo_stroke(cr);
                break;
            case CANVAS_FILL_RECT:
                cairo_set_source_rgba(cr, c->r, c->g, c->b, c->a);
                cairo_rectangle(cr, c->x, c->y, c->w, c->h);
                cairo_fill(cr);
                break;
            case CANVAS_CLEAR:
                break;
        }
    }
}

int aether_ui_canvas_create_impl(int width, int height) {
    ensure_gtk_init();
    GtkWidget* da = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(da), width);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(da), height);

    if (canvas_state_count >= canvas_state_capacity) {
        canvas_state_capacity = canvas_state_capacity == 0 ? 16 : canvas_state_capacity * 2;
        canvas_states = realloc(canvas_states, sizeof(CanvasState) * canvas_state_capacity);
    }
    CanvasState* cs = &canvas_states[canvas_state_count];
    cs->cmds = NULL;
    cs->count = 0;
    cs->capacity = 0;
    canvas_state_count++;
    int canvas_id = canvas_state_count; // 1-based

    cs->widget_handle = aether_ui_register_widget(da);

    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(da), canvas_draw_func,
                                    (gpointer)(intptr_t)canvas_id, NULL);
    return canvas_id;
}

int aether_ui_canvas_get_widget(int canvas_id) {
    CanvasState* cs = get_canvas_state(canvas_id);
    return cs ? cs->widget_handle : 0;
}

void aether_ui_canvas_begin_path_impl(int canvas_id) {
    canvas_add_cmd(canvas_id, (CanvasCmd){ .type = CANVAS_BEGIN_PATH });
}

void aether_ui_canvas_move_to_impl(int canvas_id, float x, float y) {
    canvas_add_cmd(canvas_id, (CanvasCmd){ .type = CANVAS_MOVE_TO, .x = x, .y = y });
}

void aether_ui_canvas_line_to_impl(int canvas_id, float x, float y) {
    canvas_add_cmd(canvas_id, (CanvasCmd){ .type = CANVAS_LINE_TO, .x = x, .y = y });
}

void aether_ui_canvas_stroke_impl(int canvas_id, float r, float g, float b,
                             float a, float line_width) {
    canvas_add_cmd(canvas_id, (CanvasCmd){
        .type = CANVAS_STROKE, .r = r, .g = g, .b = b, .a = a, .x = line_width
    });
}

void aether_ui_canvas_fill_rect_impl(int canvas_id, float x, float y,
                                float w, float h,
                                float r, float g, float b, float a) {
    canvas_add_cmd(canvas_id, (CanvasCmd){
        .type = CANVAS_FILL_RECT, .x = x, .y = y, .w = w, .h = h,
        .r = r, .g = g, .b = b, .a = a
    });
}

void aether_ui_canvas_clear_impl(int canvas_id) {
    CanvasState* cs = get_canvas_state(canvas_id);
    if (cs) {
        cs->count = 0;
        GtkWidget* w = aether_ui_get_widget(cs->widget_handle);
        if (w) gtk_widget_queue_draw(w);
    }
}

void aether_ui_canvas_redraw_impl(int canvas_id) {
    CanvasState* cs = get_canvas_state(canvas_id);
    if (cs) {
        GtkWidget* w = aether_ui_get_widget(cs->widget_handle);
        if (w) gtk_widget_queue_draw(w);
    }
}

// Event handlers — hover and double-click

static void on_hover_enter(GtkEventControllerMotion* ctrl, double x, double y,
                            gpointer data) {
    (void)ctrl; (void)x; (void)y;
    AeClosure* c = (AeClosure*)data;
    if (c && c->fn) {
        ((void(*)(void*, intptr_t))c->fn)(c->env, 1);
    }
}

static void on_hover_leave(GtkEventControllerMotion* ctrl, gpointer data) {
    (void)ctrl;
    AeClosure* c = (AeClosure*)data;
    if (c && c->fn) {
        ((void(*)(void*, intptr_t))c->fn)(c->env, 0);
    }
}

void aether_ui_on_hover_impl(int handle, void* boxed_closure) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (!w || !boxed_closure) return;
    GtkEventController* motion = gtk_event_controller_motion_new();
    g_signal_connect(motion, "enter", G_CALLBACK(on_hover_enter), boxed_closure);
    g_signal_connect(motion, "leave", G_CALLBACK(on_hover_leave), boxed_closure);
    gtk_widget_add_controller(w, motion);
}

static void on_double_click(GtkGestureClick* gesture, int n_press,
                             double x, double y, gpointer data) {
    (void)gesture; (void)x; (void)y;
    if (n_press != 2) return;
    AeClosure* c = (AeClosure*)data;
    if (c && c->fn) {
        ((void(*)(void*))c->fn)(c->env);
    }
}

void aether_ui_on_double_click_impl(int handle, void* boxed_closure) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (!w || !boxed_closure) return;
    GtkGesture* gesture = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(gesture), 1);
    g_signal_connect(gesture, "pressed", G_CALLBACK(on_double_click), boxed_closure);
    gtk_widget_add_controller(w, GTK_EVENT_CONTROLLER(gesture));
}

// Click handler (single click on any widget, not just buttons)
void aether_ui_on_click_impl(int handle, void* boxed_closure) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (!w || !boxed_closure) return;
    GtkGesture* gesture = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(gesture), 1);
    g_signal_connect(gesture, "released",
        G_CALLBACK(on_button_clicked), boxed_closure);
    gtk_widget_add_controller(w, GTK_EVENT_CONTROLLER(gesture));
}

// Animation — opacity and position

void aether_ui_animate_opacity_impl(int handle, double target, int duration_ms) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (!w) return;
    // For simplicity, set immediately (proper animation needs GLib timer)
    gtk_widget_set_opacity(w, target);
}

// Widget removal
void aether_ui_remove_child_impl(int parent_handle, int child_handle) {
    GtkWidget* parent = aether_ui_get_widget(parent_handle);
    GtkWidget* child = aether_ui_get_widget(child_handle);
    if (!parent || !child) return;
    if (GTK_IS_BOX(parent)) {
        gtk_box_remove(GTK_BOX(parent), child);
    }
}

void aether_ui_clear_children_impl(int handle) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (!w) return;
    if (GTK_IS_BOX(w)) {
        GtkWidget* child;
        while ((child = gtk_widget_get_first_child(w)) != NULL) {
            gtk_box_remove(GTK_BOX(w), child);
        }
    }
}

// ---------------------------------------------------------------------------
// AetherUIDriver — Selenium-like AetherUIDriver
//
// An embedded HTTP server for Selenium-like remote control. When enabled:
//   1. A "Under Remote Control" banner is injected into the root — it cannot
//      be dismissed or hidden via the test API.
//   2. Widgets marked with seal_widget() are non-automatable — the test server
//      returns 403 Forbidden for any action on sealed widgets.
//   3. A simple HTTP API exposes widget state and actions.
//
// Endpoints:
//   GET  /widgets                    — list all widget handles + types (JSON)
//   GET  /widget/{id}               — widget state (type, text, value, visible)
//   POST /widget/{id}/click         — simulate button click
//   POST /widget/{id}/set_text?v=X  — set text/textfield value
//   POST /widget/{id}/toggle        — toggle a checkbox
//   POST /widget/{id}/set_value?v=X — set slider value
//   GET  /state/{id}                — get reactive state value
//   POST /state/{id}/set?v=X        — set reactive state value
// ---------------------------------------------------------------------------

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

// Sealed widgets — test server refuses to interact with these.
static int* sealed_widgets = NULL;
static int sealed_count = 0;
static int sealed_capacity = 0;

void aether_ui_seal_widget_impl(int handle) {
    if (sealed_count >= sealed_capacity) {
        sealed_capacity = sealed_capacity == 0 ? 32 : sealed_capacity * 2;
        sealed_widgets = realloc(sealed_widgets, sizeof(int) * sealed_capacity);
    }
    sealed_widgets[sealed_count++] = handle;
}

// Walk GTK widget tree and seal every descendant.
static void seal_subtree_recursive(GtkWidget* w) {
    if (!w) return;
    // Find this widget's handle in the registry
    for (int i = 0; i < widget_count; i++) {
        if (widgets[i] == w) {
            aether_ui_seal_widget_impl(i + 1);
            break;
        }
    }
    // Recurse into children
    for (GtkWidget* child = gtk_widget_get_first_child(w);
         child; child = gtk_widget_get_next_sibling(child)) {
        seal_subtree_recursive(child);
    }
}

void aether_ui_seal_subtree_impl(int handle) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (w) seal_subtree_recursive(w);
}

static int is_widget_sealed(int handle) {
    for (int i = 0; i < sealed_count; i++) {
        if (sealed_widgets[i] == handle) return 1;
    }
    return 0;
}

// Find the registry handle for a GtkWidget, or 0 if not registered.
static int handle_for_widget(GtkWidget* w) {
    if (!w) return 0;
    for (int i = 0; i < widget_count; i++) {
        if (widgets[i] == w) return i + 1;
    }
    return 0;
}

// Find parent handle for a widget by walking the GTK widget tree.
static int parent_handle_for(int handle) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (!w) return 0;
    GtkWidget* parent = gtk_widget_get_parent(w);
    return handle_for_widget(parent);
}

// Banner widget handle — protected from the test API.
static int banner_handle = 0;

static const char* widget_type_name(GtkWidget* w) {
    if (!w) return "null";
    if (GTK_IS_LABEL(w)) return "text";
    if (GTK_IS_BUTTON(w)) return "button";
    if (GTK_IS_ENTRY(w)) return "textfield";
    if (GTK_IS_PASSWORD_ENTRY(w)) return "securefield";
    if (GTK_IS_CHECK_BUTTON(w)) return "toggle";
    if (GTK_IS_SCALE(w)) return "slider";
    if (GTK_IS_DROP_DOWN(w)) return "picker";
    if (GTK_IS_TEXT_VIEW(w)) return "textarea";
    if (GTK_IS_PROGRESS_BAR(w)) return "progressbar";
    if (GTK_IS_SEPARATOR(w)) return "divider";
    if (GTK_IS_SCROLLED_WINDOW(w)) return "scrollview";
    if (GTK_IS_OVERLAY(w)) return "zstack";
    if (GTK_IS_DRAWING_AREA(w)) return "canvas";
    if (GTK_IS_IMAGE(w)) return "image";
    if (GTK_IS_BOX(w)) {
        GtkOrientation o = gtk_orientable_get_orientation(GTK_ORIENTABLE(w));
        return o == GTK_ORIENTATION_VERTICAL ? "vstack" : "hstack";
    }
    return "widget";
}

// Get widget text content (for text labels and text fields).
static const char* widget_text_content(GtkWidget* w) {
    if (!w) return "";
    if (GTK_IS_LABEL(w)) return gtk_label_get_text(GTK_LABEL(w));
    if (GTK_IS_ENTRY(w)) {
        GtkEntryBuffer* buf = gtk_entry_get_buffer(GTK_ENTRY(w));
        return gtk_entry_buffer_get_text(buf);
    }
    if (GTK_IS_BUTTON(w)) return gtk_button_get_label(GTK_BUTTON(w));
    return "";
}

// Build JSON response for a single widget.
static int widget_to_json(int handle, char* buf, int bufsize) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (!w) {
        return snprintf(buf, bufsize, "{\"id\":%d,\"type\":\"null\"}", handle);
    }
    const char* type = widget_type_name(w);
    const char* text = widget_text_content(w);
    int visible = gtk_widget_get_visible(w) ? 1 : 0;
    int sealed = is_widget_sealed(handle) ? 1 : 0;
    int is_banner = (handle == banner_handle) ? 1 : 0;

    int n = snprintf(buf, bufsize,
        "{\"id\":%d,\"type\":\"%s\",\"text\":\"%s\",\"visible\":%s,\"sealed\":%s,\"banner\":%s",
        handle, type, text ? text : "",
        visible ? "true" : "false",
        sealed ? "true" : "false",
        is_banner ? "true" : "false");

    // Parent handle
    int parent_id = parent_handle_for(handle);
    n += snprintf(buf + n, bufsize - n, ",\"parent\":%d", parent_id);

    // Add type-specific values
    if (GTK_IS_CHECK_BUTTON(w)) {
        int active = gtk_check_button_get_active(GTK_CHECK_BUTTON(w)) ? 1 : 0;
        n += snprintf(buf + n, bufsize - n, ",\"active\":%s", active ? "true" : "false");
    } else if (GTK_IS_SCALE(w)) {
        double val = gtk_range_get_value(GTK_RANGE(w));
        n += snprintf(buf + n, bufsize - n, ",\"value\":%.2f", val);
    } else if (GTK_IS_PROGRESS_BAR(w)) {
        double val = gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(w));
        n += snprintf(buf + n, bufsize - n, ",\"value\":%.2f", val);
    }

    n += snprintf(buf + n, bufsize - n, "}");
    return n;
}

// Parse a simple HTTP request: "GET /path HTTP/1.1\r\n..."
// Returns the method (0=GET, 1=POST) and path.
static int parse_http_request(const char* req, char* path, int pathsize) {
    int method = 0;
    if (strncmp(req, "POST", 4) == 0) method = 1;
    const char* p = strchr(req, ' ');
    if (!p) return -1;
    p++;
    const char* end = strchr(p, ' ');
    if (!end) end = strchr(p, '\r');
    if (!end) end = p + strlen(p);
    int len = (int)(end - p);
    if (len >= pathsize) len = pathsize - 1;
    memcpy(path, p, len);
    path[len] = '\0';
    return method;
}

// Extract integer from path like "/widget/5/click" → 5
static int extract_id_from_path(const char* path, const char* prefix) {
    size_t plen = strlen(prefix);
    if (strncmp(path, prefix, plen) != 0) return -1;
    return atoi(path + plen);
}

// Extract query parameter: "/widget/5/set_text?v=hello" → "hello"
static const char* extract_query_param(const char* path, const char* key) {
    char needle[64];
    snprintf(needle, sizeof(needle), "%s=", key);
    const char* p = strstr(path, needle);
    if (!p) return NULL;
    return p + strlen(needle);
}

// Data passed to the GTK idle callback for thread-safe widget interaction.
typedef struct {
    int action;  // 0=click, 1=set_text, 2=toggle, 3=set_value, 4=set_state
    int handle;
    double dval;
    char sval[512];
    int done;
    int result;  // 0=ok, 1=sealed, 2=banner, 3=not_found
} TestAction;

static gboolean test_action_idle(gpointer data) {
    TestAction* ta = (TestAction*)data;
    GtkWidget* w = aether_ui_get_widget(ta->handle);

    if (ta->action == 4) {
        // State set — not a widget action
        aether_ui_state_set(ta->handle, ta->dval);
        ta->result = 0;
        ta->done = 1;
        return G_SOURCE_REMOVE;
    }

    if (!w) { ta->result = 3; ta->done = 1; return G_SOURCE_REMOVE; }
    if (ta->handle == banner_handle) { ta->result = 2; ta->done = 1; return G_SOURCE_REMOVE; }
    if (is_widget_sealed(ta->handle)) { ta->result = 1; ta->done = 1; return G_SOURCE_REMOVE; }

    switch (ta->action) {
        case 0: // click
            if (GTK_IS_BUTTON(w)) {
                g_signal_emit_by_name(w, "clicked");
            }
            break;
        case 1: // set_text
            if (GTK_IS_LABEL(w)) {
                gtk_label_set_text(GTK_LABEL(w), ta->sval);
            } else if (GTK_IS_ENTRY(w)) {
                GtkEntryBuffer* buf = gtk_entry_get_buffer(GTK_ENTRY(w));
                gtk_entry_buffer_set_text(buf, ta->sval, -1);
            }
            break;
        case 2: // toggle
            if (GTK_IS_CHECK_BUTTON(w)) {
                gboolean cur = gtk_check_button_get_active(GTK_CHECK_BUTTON(w));
                gtk_check_button_set_active(GTK_CHECK_BUTTON(w), !cur);
            }
            break;
        case 3: // set_value
            if (GTK_IS_SCALE(w)) {
                gtk_range_set_value(GTK_RANGE(w), ta->dval);
            } else if (GTK_IS_PROGRESS_BAR(w)) {
                gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(w), ta->dval);
            }
            break;
    }
    ta->result = 0;
    ta->done = 1;
    return G_SOURCE_REMOVE;
}

static void send_response(int fd, int status, const char* status_text,
                           const char* content_type, const char* body) {
    char header[512];
    int bodylen = body ? (int)strlen(body) : 0;
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n"
        "\r\n",
        status, status_text, content_type, bodylen);
    write(fd, header, hlen);
    if (body && bodylen > 0) write(fd, body, bodylen);
}

// Screenshot result — shared between server thread and GTK idle callback.
static unsigned char* ss_result_data = NULL;
static gsize ss_result_len = 0;
static volatile int ss_result_done = 0;

static gboolean screenshot_idle_cb(gpointer data) {
    (void)data;
    ss_result_data = NULL;
    ss_result_len = 0;

    GtkWidget* root = aether_ui_get_widget(1);
    if (!root) { ss_result_done = 1; return G_SOURCE_REMOVE; }

    GtkWidget* toplevel = root;
    while (gtk_widget_get_parent(toplevel))
        toplevel = gtk_widget_get_parent(toplevel);
    if (!GTK_IS_WINDOW(toplevel)) { ss_result_done = 1; return G_SOURCE_REMOVE; }

    GdkPaintable* paintable = gtk_widget_paintable_new(toplevel);
    int w = gdk_paintable_get_intrinsic_width(paintable);
    int h = gdk_paintable_get_intrinsic_height(paintable);
    if (w <= 0 || h <= 0) { w = 400; h = 300; }

    GtkSnapshot* snapshot = gtk_snapshot_new();
    gdk_paintable_snapshot(paintable, snapshot, (double)w, (double)h);
    GskRenderNode* node = gtk_snapshot_free_to_node(snapshot);
    if (!node) {
        g_object_unref(paintable);
        ss_result_done = 1;
        return G_SOURCE_REMOVE;
    }

    GdkSurface* surface = gtk_native_get_surface(GTK_NATIVE(toplevel));
    GskRenderer* renderer = gsk_renderer_new_for_surface(surface);
    GdkTexture* texture = gsk_renderer_render_texture(renderer, node,
        &GRAPHENE_RECT_INIT(0, 0, w, h));
    GBytes* bytes = gdk_texture_save_to_png_bytes(texture);

    gsize len = 0;
    const unsigned char* raw = g_bytes_get_data(bytes, &len);
    ss_result_data = malloc(len);
    memcpy(ss_result_data, raw, len);
    ss_result_len = len;

    g_bytes_unref(bytes);
    g_object_unref(texture);
    gsk_render_node_unref(node);
    gsk_renderer_unrealize(renderer);
    g_object_unref(renderer);
    g_object_unref(paintable);
    ss_result_done = 1;
    return G_SOURCE_REMOVE;
}

static void handle_test_request(int client_fd) {
    char req[4096];
    int n = (int)read(client_fd, req, sizeof(req) - 1);
    if (n <= 0) { close(client_fd); return; }
    req[n] = '\0';

    char path[1024];
    int method = parse_http_request(req, path, sizeof(path));
    if (method < 0) {
        send_response(client_fd, 400, "Bad Request", "text/plain", "Bad request");
        close(client_fd);
        return;
    }

    // GET /widgets — list all widgets, with optional query filters
    //   /widgets           — all widgets
    //   /widgets?type=button  — only buttons
    //   /widgets?text=Submit  — only widgets whose text matches
    //   /widgets?type=button&text=+1  — combined filter
    if (method == 0 && strncmp(path, "/widgets", 8) == 0 &&
        (path[8] == '\0' || path[8] == '?')) {
        const char* filter_type = extract_query_param(path, "type");
        const char* filter_text = extract_query_param(path, "text");
        // Truncate filter values at '&' (multi-param)
        char ft_buf[128] = "", fx_buf[128] = "";
        if (filter_type) {
            strncpy(ft_buf, filter_type, sizeof(ft_buf) - 1);
            char* amp = strchr(ft_buf, '&'); if (amp) *amp = '\0';
        }
        if (filter_text) {
            strncpy(fx_buf, filter_text, sizeof(fx_buf) - 1);
            char* amp = strchr(fx_buf, '&'); if (amp) *amp = '\0';
        }

        char* body = malloc(widget_count * 512 + 64);
        int pos = 0;
        int first = 1;
        pos += sprintf(body + pos, "[");
        for (int i = 1; i <= widget_count; i++) {
            GtkWidget* w = aether_ui_get_widget(i);
            if (!w) continue;
            if (ft_buf[0] && strcmp(widget_type_name(w), ft_buf) != 0) continue;
            if (fx_buf[0]) {
                const char* t = widget_text_content(w);
                if (!t || strcmp(t, fx_buf) != 0) continue;
            }
            if (!first) pos += sprintf(body + pos, ",");
            first = 0;
            pos += widget_to_json(i, body + pos, 512);
        }
        pos += sprintf(body + pos, "]");
        send_response(client_fd, 200, "OK", "application/json", body);
        free(body);
        close(client_fd);
        return;
    }

    // GET /widget/{id}/children — list child widget handles
    if (method == 0 && strncmp(path, "/widget/", 8) == 0) {
        char* suffix = strchr(path + 8, '/');
        if (suffix && strcmp(suffix, "/children") == 0) {
            int id = atoi(path + 8);
            GtkWidget* w = aether_ui_get_widget(id);
            if (!w) {
                send_response(client_fd, 404, "Not Found", "application/json",
                              "{\"error\":\"widget not found\"}");
                close(client_fd);
                return;
            }
            char* body = malloc(widget_count * 64 + 64);
            int pos = 0;
            int first = 1;
            pos += sprintf(body + pos, "[");
            for (GtkWidget* child = gtk_widget_get_first_child(w);
                 child; child = gtk_widget_get_next_sibling(child)) {
                int ch = handle_for_widget(child);
                if (ch > 0) {
                    if (!first) pos += sprintf(body + pos, ",");
                    first = 0;
                    pos += widget_to_json(ch, body + pos, 512);
                }
            }
            pos += sprintf(body + pos, "]");
            send_response(client_fd, 200, "OK", "application/json", body);
            free(body);
            close(client_fd);
            return;
        }
    }

    // GET /screenshot — capture the root widget as PNG
    if (method == 0 && strcmp(path, "/screenshot") == 0) {
        ss_result_done = 0;
        g_idle_add(screenshot_idle_cb, NULL);
        while (!ss_result_done) usleep(1000);

        if (ss_result_data && ss_result_len > 0) {
            char header[256];
            int hlen = snprintf(header, sizeof(header),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: image/png\r\n"
                "Content-Length: %zu\r\n"
                "Connection: close\r\n\r\n", ss_result_len);
            write(client_fd, header, hlen);
            write(client_fd, ss_result_data, ss_result_len);
            free(ss_result_data);
            ss_result_data = NULL;
        } else {
            send_response(client_fd, 500, "Error", "application/json",
                          "{\"error\":\"screenshot failed\"}");
        }
        close(client_fd);
        return;
    }

    // GET /widget/{id} — single widget info (no trailing slash/action)
    if (method == 0 && strncmp(path, "/widget/", 8) == 0 && strchr(path + 8, '/') == NULL) {
        int id = atoi(path + 8);
        if (id < 1 || id > widget_count) {
            send_response(client_fd, 404, "Not Found", "application/json",
                          "{\"error\":\"widget not found\"}");
        } else {
            char buf[512];
            widget_to_json(id, buf, sizeof(buf));
            send_response(client_fd, 200, "OK", "application/json", buf);
        }
        close(client_fd);
        return;
    }

    // GET /state/{id}
    if (method == 0 && strncmp(path, "/state/", 7) == 0 && strchr(path + 7, '/') == NULL) {
        int id = atoi(path + 7);
        double val = aether_ui_state_get(id);
        char buf[128];
        snprintf(buf, sizeof(buf), "{\"id\":%d,\"value\":%.6f}", id, val);
        send_response(client_fd, 200, "OK", "application/json", buf);
        close(client_fd);
        return;
    }

    // POST actions — must run on GTK main thread via idle callback
    TestAction ta = {0};
    ta.done = 0;
    ta.result = 3;

    // POST /widget/{id}/click
    if (method == 1 && strncmp(path, "/widget/", 8) == 0) {
        char* action_part = strchr(path + 8, '/');
        if (action_part) {
            ta.handle = atoi(path + 8);
            if (strncmp(action_part, "/click", 6) == 0) {
                ta.action = 0;
            } else if (strncmp(action_part, "/set_text", 9) == 0) {
                ta.action = 1;
                const char* v = extract_query_param(path, "v");
                if (v) strncpy(ta.sval, v, sizeof(ta.sval) - 1);
            } else if (strncmp(action_part, "/toggle", 7) == 0) {
                ta.action = 2;
            } else if (strncmp(action_part, "/set_value", 10) == 0) {
                ta.action = 3;
                const char* v = extract_query_param(path, "v");
                if (v) ta.dval = atof(v);
            } else {
                send_response(client_fd, 400, "Bad Request", "application/json",
                              "{\"error\":\"unknown action\"}");
                close(client_fd);
                return;
            }

            g_idle_add(test_action_idle, &ta);
            // Busy-wait for GTK thread to complete the action
            while (!ta.done) usleep(1000);

            if (ta.result == 1) {
                send_response(client_fd, 403, "Forbidden", "application/json",
                              "{\"error\":\"widget is sealed\"}");
            } else if (ta.result == 2) {
                send_response(client_fd, 403, "Forbidden", "application/json",
                              "{\"error\":\"banner cannot be manipulated\"}");
            } else if (ta.result == 3) {
                send_response(client_fd, 404, "Not Found", "application/json",
                              "{\"error\":\"widget not found\"}");
            } else {
                send_response(client_fd, 200, "OK", "application/json", "{\"ok\":true}");
            }
            close(client_fd);
            return;
        }
    }

    // POST /state/{id}/set?v=X
    if (method == 1 && strncmp(path, "/state/", 7) == 0) {
        char* action_part = strchr(path + 7, '/');
        if (action_part && strncmp(action_part, "/set", 4) == 0) {
            ta.handle = atoi(path + 7);
            ta.action = 4;
            const char* v = extract_query_param(path, "v");
            if (v) ta.dval = atof(v);
            g_idle_add(test_action_idle, &ta);
            while (!ta.done) usleep(1000);
            send_response(client_fd, 200, "OK", "application/json", "{\"ok\":true}");
            close(client_fd);
            return;
        }
    }

    send_response(client_fd, 404, "Not Found", "text/plain", "Not found");
    close(client_fd);
}

static void* test_server_thread(void* arg) {
    int port = (int)(intptr_t)arg;
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) return NULL;

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)port);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "AetherUIDriver: failed to bind to port %d: %s\n",
                port, strerror(errno));
        close(server_fd);
        return NULL;
    }
    listen(server_fd, 8);
    fprintf(stderr, "AetherUIDriver: listening on http://127.0.0.1:%d\n", port);

    for (;;) {
        int client = accept(server_fd, NULL, NULL);
        if (client < 0) continue;
        handle_test_request(client);
    }
    return NULL;
}

// Inject the "Under Remote Control" banner into the app root.
// Called from the GTK activate callback when test mode is on.
static void inject_remote_control_banner(int root_handle) {
    GtkWidget* root = aether_ui_get_widget(root_handle);
    if (!root || !GTK_IS_BOX(root)) return;

    GtkWidget* banner = gtk_label_new("Under Remote Control");
    gtk_widget_add_css_class(banner, "aether-rc-banner");

    GtkCssProvider* prov = gtk_css_provider_new();
    gtk_css_provider_load_from_string(prov,
        ".aether-rc-banner {"
        "  background-color: #cc3333;"
        "  color: white;"
        "  font-weight: bold;"
        "  padding: 4px 12px;"
        "  font-size: 12px;"
        "}");
    GdkDisplay* display = gdk_display_get_default();
    if (display) {
        gtk_style_context_add_provider_for_display(
            display, GTK_STYLE_PROVIDER(prov),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 10);
    }
    g_object_unref(prov);

    banner_handle = aether_ui_register_widget(banner);
    gtk_box_prepend(GTK_BOX(root), banner);
}

static int test_server_port = 0;

void aether_ui_enable_test_server_impl(int port, int root_handle) {
    test_server_port = port;

    inject_remote_control_banner(root_handle);

    pthread_t tid;
    pthread_create(&tid, NULL, test_server_thread, (void*)(intptr_t)port);
    pthread_detach(tid);
}

// ---------------------------------------------------------------------------
// Widget tree
// ---------------------------------------------------------------------------

// Called from Aether DSL wrappers where parent comes through _ctx (void*).
void aether_ui_widget_add_child_ctx(void* parent_ctx, int child_handle) {
    int parent_handle = (int)(intptr_t)parent_ctx;
    GtkWidget* parent = aether_ui_get_widget(parent_handle);
    GtkWidget* child = aether_ui_get_widget(child_handle);
    if (!parent || !child) return;

    if (GTK_IS_BOX(parent)) {
        if (gtk_orientable_get_orientation(GTK_ORIENTABLE(parent)) == GTK_ORIENTATION_HORIZONTAL) {
            if (gtk_widget_get_hexpand(child) && gtk_widget_get_vexpand(child)) {
                gtk_widget_set_vexpand(child, FALSE);
            }
        }
        gtk_box_append(GTK_BOX(parent), child);
    } else if (GTK_IS_SCROLLED_WINDOW(parent)) {
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(parent), child);
    } else if (GTK_IS_OVERLAY(parent)) {
        if (gtk_overlay_get_child(GTK_OVERLAY(parent)) == NULL) {
            gtk_overlay_set_child(GTK_OVERLAY(parent), child);
        } else {
            gtk_overlay_add_overlay(GTK_OVERLAY(parent), child);
        }
    }
}

void aether_ui_widget_set_hidden(int handle, int hidden) {
    GtkWidget* w = aether_ui_get_widget(handle);
    if (w) gtk_widget_set_visible(w, !hidden);
}
