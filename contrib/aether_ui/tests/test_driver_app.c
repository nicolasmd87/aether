// Test-driver harness: creates a window with known-id widgets and starts
// the AetherUIDriver on the port given by AETHER_UI_TEST_PORT. Used by
// test_driver.sh to verify the HTTP test server works cross-platform.

#include "../aether_ui_backend.h"
#include <windows.h>
#include <stdlib.h>

int main(void) {
    int app = aether_ui_app_create("Driver Test", 400, 260);
    int root = aether_ui_vstack_create(6);
    int heading = aether_ui_text_create("Heading");
    int btn = aether_ui_button_create_plain("Click me");
    int tf = aether_ui_textfield_create("name", (void*)0);
    int tg = aether_ui_toggle_create("toggle", (void*)0);
    int sl = aether_ui_slider_create(0.0, 100.0, 25.0, (void*)0);
    aether_ui_widget_add_child_ctx((void*)(intptr_t)root, heading);
    aether_ui_widget_add_child_ctx((void*)(intptr_t)root, btn);
    aether_ui_widget_add_child_ctx((void*)(intptr_t)root, tf);
    aether_ui_widget_add_child_ctx((void*)(intptr_t)root, tg);
    aether_ui_widget_add_child_ctx((void*)(intptr_t)root, sl);
    aether_ui_app_set_body(app, root);
    aether_ui_app_run_raw(app);
    return 0;
}
