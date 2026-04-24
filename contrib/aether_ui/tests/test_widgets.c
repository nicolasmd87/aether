// Widget-creation smoke tests: exercise every widget constructor and
// every state getter/setter, making sure they don't crash and basic
// round-tripping works.
//
// Runs headless — creates widgets but never pumps the message loop.
// Suitable for CI on every platform.
//
// Sets AETHER_UI_HEADLESS=1 at startup (see main()). Every backend's
// modal API (menu_popup, alert, file_open, sheet_present, …) must
// honor this flag and no-op, otherwise the test hangs on CI waiting
// for user input that will never come.

#include "test_framework.h"
#include "../aether_ui_backend.h"
#include <stdlib.h>  // setenv

int ae_test_pass = 0;
int ae_test_fail = 0;
const char* ae_test_current = "";

static void test_state_roundtrip(void) {
    AE_TEST(state_roundtrip);
    int s = aether_ui_state_create(1.5);
    AE_CASE(s >= 1, "state handle is positive");
    AE_CASE(aether_ui_state_get(s) == 1.5, "initial value preserved");
    aether_ui_state_set(s, 42.0);
    AE_CASE(aether_ui_state_get(s) == 42.0, "set reflects in get");
}

static void test_basic_widgets(void) {
    AE_TEST(basic_widgets);
    int t = aether_ui_text_create("Hello");
    AE_CASE(t >= 1, "text_create returns handle");
    aether_ui_text_set_string(t, "World");
    AE_CASE(1, "text_set_string does not crash");

    int b = aether_ui_button_create_plain("Click");
    AE_CASE(b >= 1, "button_create returns handle");

    int d = aether_ui_divider_create();
    AE_CASE(d >= 1, "divider_create returns handle");

    int s = aether_ui_spacer_create();
    AE_CASE(s >= 1, "spacer_create returns handle");
}

static void test_layout_containers(void) {
    AE_TEST(layout_containers);
    int v = aether_ui_vstack_create(8);
    int h = aether_ui_hstack_create(4);
    int z = aether_ui_zstack_create();
    int f = aether_ui_form_create();
    int fs = aether_ui_form_section_create("Section");
    int n = aether_ui_navstack_create();
    int sv = aether_ui_scrollview_create();
    AE_CASE(v >= 1, "vstack_create");
    AE_CASE(h >= 1, "hstack_create");
    AE_CASE(z >= 1, "zstack_create");
    AE_CASE(f >= 1, "form_create");
    AE_CASE(fs >= 1, "form_section_create");
    AE_CASE(n >= 1, "navstack_create");
    AE_CASE(sv >= 1, "scrollview_create");
}

static void test_input_widgets(void) {
    AE_TEST(input_widgets);
    int tf = aether_ui_textfield_create("placeholder", (void*)0);
    AE_CASE(tf >= 1, "textfield_create");
    aether_ui_textfield_set_text(tf, "hello");
    const char* got = aether_ui_textfield_get_text(tf);
    AE_CASE(got && strcmp(got, "hello") == 0, "textfield roundtrip");

    int sf = aether_ui_securefield_create("pass", (void*)0);
    AE_CASE(sf >= 1, "securefield_create");

    int tg = aether_ui_toggle_create("On?", (void*)0);
    AE_CASE(tg >= 1, "toggle_create");
    aether_ui_toggle_set_active(tg, 1);
    AE_CASE(aether_ui_toggle_get_active(tg) == 1, "toggle on");
    aether_ui_toggle_set_active(tg, 0);
    AE_CASE(aether_ui_toggle_get_active(tg) == 0, "toggle off");

    int sl = aether_ui_slider_create(0.0, 100.0, 50.0, (void*)0);
    AE_CASE(sl >= 1, "slider_create");
    AE_CASE(aether_ui_slider_get_value(sl) == 50.0, "slider initial");
    aether_ui_slider_set_value(sl, 75.0);
    AE_CASE(aether_ui_slider_get_value(sl) == 75.0, "slider set");

    int pk = aether_ui_picker_create((void*)0);
    aether_ui_picker_add_item(pk, "One");
    aether_ui_picker_add_item(pk, "Two");
    aether_ui_picker_set_selected(pk, 1);
    AE_CASE(aether_ui_picker_get_selected(pk) == 1, "picker selection");

    int pb = aether_ui_progressbar_create(0.25);
    AE_CASE(pb >= 1, "progressbar_create");
    aether_ui_progressbar_set_fraction(pb, 0.75);
    AE_CASE(1, "progressbar set");

    int ta = aether_ui_textarea_create("", (void*)0);
    aether_ui_textarea_set_text(ta, "multi\nline");
    char* text = aether_ui_textarea_get_text(ta);
    AE_CASE(text && (strcmp(text, "multi\r\nline") == 0
                 || strcmp(text, "multi\nline") == 0),
            "textarea roundtrip");
    free(text);
}

static void test_canvas(void) {
    AE_TEST(canvas);
    int c = aether_ui_canvas_create_impl(200, 100);
    AE_CASE(c >= 1, "canvas_create");
    int w = aether_ui_canvas_get_widget(c);
    AE_CASE(w >= 1, "canvas widget handle");
    aether_ui_canvas_begin_path_impl(c);
    aether_ui_canvas_move_to_impl(c, 10, 10);
    aether_ui_canvas_line_to_impl(c, 50, 50);
    aether_ui_canvas_stroke_impl(c, 0.0f, 0.0f, 1.0f, 1.0f, 2.0f);
    aether_ui_canvas_fill_rect_impl(c, 10, 10, 80, 20,
                                     1.0f, 0.5f, 0.0f, 1.0f);
    aether_ui_canvas_clear_impl(c);
    AE_CASE(1, "canvas drawing ops did not crash");
}

static void test_styling(void) {
    AE_TEST(styling);
    int v = aether_ui_vstack_create(0);
    aether_ui_set_bg_color(v, 0.1, 0.2, 0.3, 1.0);
    aether_ui_set_text_color(v, 0.9, 0.9, 0.9);
    aether_ui_set_font_size(v, 16.0);
    aether_ui_set_font_bold(v, 1);
    aether_ui_set_corner_radius(v, 8.0);
    aether_ui_set_edge_insets(v, 4, 8, 4, 8);
    aether_ui_set_width(v, 300);
    aether_ui_set_height(v, 200);
    aether_ui_set_opacity(v, 0.8);
    aether_ui_set_enabled(v, 1);
    aether_ui_set_tooltip(v, "Hello");
    aether_ui_set_margin(v, 1, 2, 3, 4);
    aether_ui_set_alignment(v, 1);
    aether_ui_set_distribution(v, 0);
    AE_CASE(1, "styling ops did not crash");
}

static void test_events(void) {
    AE_TEST(events);
    int b = aether_ui_button_create_plain("B");
    aether_ui_on_click_impl(b, (void*)0);
    aether_ui_on_hover_impl(b, (void*)0);
    aether_ui_on_double_click_impl(b, (void*)0);
    AE_CASE(1, "event binding ops did not crash");
}

static void test_system_services(void) {
    AE_TEST(system_services);
    // Dark mode check is the only one safe to invoke headless.
    int dm = aether_ui_dark_mode_check();
    AE_CASE(dm == 0 || dm == 1, "dark_mode_check returns 0 or 1");
    // Clipboard write is safe headless — requires no UI thread.
    aether_ui_clipboard_write_impl("aether-ui-test");
    AE_CASE(1, "clipboard write did not crash");
}

static void test_many_widgets(void) {
    AE_TEST(many_widgets_stress);
    int root = aether_ui_vstack_create(2);
    for (int i = 0; i < 500; i++) {
        int t = aether_ui_text_create("item");
        aether_ui_widget_add_child_ctx((void*)(intptr_t)root, t);
    }
    AE_CASE(1, "500-widget tree built without crashing");
}

static void test_unicode_widgets(void) {
    AE_TEST(unicode_widgets);
    int t1 = aether_ui_text_create("日本語");
    int t2 = aether_ui_text_create("emoji 🚀 rocket");
    int t3 = aether_ui_text_create("mix: café naïve");
    AE_CASE(t1 >= 1 && t2 >= 1 && t3 >= 1, "non-ASCII text created");
    aether_ui_text_set_string(t1, "中文");
    int tf = aether_ui_textfield_create("", (void*)0);
    aether_ui_textfield_set_text(tf, "Привет");
    const char* got = aether_ui_textfield_get_text(tf);
    AE_CASE(got && strcmp(got, "Привет") == 0,
            "Cyrillic roundtrips through textfield");
}

static void test_deep_nesting(void) {
    AE_TEST(deep_nesting);
    int prev = aether_ui_vstack_create(0);
    int root = prev;
    for (int i = 0; i < 30; i++) {
        int next = aether_ui_vstack_create(0);
        aether_ui_widget_add_child_ctx((void*)(intptr_t)prev, next);
        prev = next;
    }
    AE_CASE(root >= 1, "30-level deep stack built");
}

static void test_seal_subtree(void) {
    AE_TEST(seal_subtree);
    int root = aether_ui_vstack_create(0);
    int child = aether_ui_text_create("private");
    aether_ui_widget_add_child_ctx((void*)(intptr_t)root, child);
    aether_ui_seal_subtree_impl(root);
    AE_CASE(1, "seal subtree did not crash");
}

static void test_menu(void) {
    AE_TEST(menu);
    int bar = aether_ui_menu_bar_create();
    AE_CASE(bar >= 1, "menu_bar_create");
    int file_menu = aether_ui_menu_create("File");
    AE_CASE(file_menu >= 1, "menu_create File");
    aether_ui_menu_add_item(file_menu, "Open...", (void*)0);
    aether_ui_menu_add_item(file_menu, "Save",    (void*)0);
    aether_ui_menu_add_separator(file_menu);
    aether_ui_menu_add_item(file_menu, "Quit",    (void*)0);
    aether_ui_menu_bar_add_menu(bar, file_menu);
    AE_CASE(1, "menu populated + attached to bar");

    // Context menu via popup (can't actually trigger interactively;
    // just verify it doesn't crash when given a real anchor widget).
    int ctx_menu = aether_ui_menu_create("Ctx");
    aether_ui_menu_add_item(ctx_menu, "Copy", (void*)0);
    int anchor = aether_ui_button_create_plain("anchor");
    aether_ui_menu_popup(ctx_menu, anchor);
    AE_CASE(1, "menu_popup did not crash");
}

static void test_grid(void) {
    AE_TEST(grid);
    int g = aether_ui_grid_create(2, 4, 4);
    AE_CASE(g >= 1, "grid_create returns handle");

    // Standard login-form pattern: label / field on each row.
    int l1 = aether_ui_text_create("Username:");
    int f1 = aether_ui_textfield_create("", (void*)0);
    int l2 = aether_ui_text_create("Password:");
    int f2 = aether_ui_securefield_create("", (void*)0);
    aether_ui_grid_place(g, l1, 0, 0, 1, 1);
    aether_ui_grid_place(g, f1, 0, 1, 1, 1);
    aether_ui_grid_place(g, l2, 1, 0, 1, 1);
    aether_ui_grid_place(g, f2, 1, 1, 1, 1);
    AE_CASE(1, "grid 2x2 placement does not crash");

    // Spanning cell
    int btn = aether_ui_button_create_plain("Sign In");
    aether_ui_grid_place(g, btn, 2, 0, 1, 2); // row 2, both cols
    AE_CASE(1, "grid col_span=2 placement does not crash");
}

int main(void) {
    // Announce that we're running headless so every backend skips
    // modal UI (menu popups, file dialogs, alerts, sheets). Without
    // this, TrackPopupMenu on Win32 / popUpMenuPositioningItem on
    // macOS / modal GtkAlertDialog on GTK can spin their own message
    // loop and block indefinitely — the tests have no user to click
    // anything and no outer runloop to dismiss.
    // The "0" overwrite flag preserves any external setting.
    setenv("AETHER_UI_HEADLESS", "1", 0);
    // Prevent stdout block-buffering when redirected to a CI log file
    // so progress is observable on hangs.
    setvbuf(stdout, NULL, _IOLBF, 0);

    printf("Running Aether UI backend tests...\n\n");
    test_state_roundtrip();
    test_basic_widgets();
    test_layout_containers();
    test_input_widgets();
    test_canvas();
    test_styling();
    test_events();
    test_system_services();
    test_many_widgets();
    test_unicode_widgets();
    test_deep_nesting();
    test_seal_subtree();
    test_menu();
    test_grid();
    printf("\n%d passed, %d failed\n", ae_test_pass, ae_test_fail);
    return ae_test_fail > 0 ? 1 : 0;
}
