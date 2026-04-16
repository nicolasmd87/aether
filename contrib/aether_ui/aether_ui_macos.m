// Aether UI — macOS AppKit backend for Aether
// Port of aether-ui-macos (Rust/objc2) to Objective-C.
//
// This file implements the same C API as aether_ui_gtk4.c using AppKit.
// The Aether module.ae is platform-agnostic — only the backend changes.
//
// Compile on macOS with:
//   clang -fobjc-arc -framework AppKit -framework Foundation \
//         aether_ui_macos.m -c -o aether_ui_macos.o

#ifdef __APPLE__

#import <Cocoa/Cocoa.h>
#include "aether_ui_gtk4.h"  // same API surface
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Closure struct — must match Aether codegen's _AeClosure layout.
// ---------------------------------------------------------------------------
typedef struct {
    void (*fn)(void);
    void* env;
} AeClosure;

// ---------------------------------------------------------------------------
// Widget registry — flat array of NSView*, 1-based handles.
// ---------------------------------------------------------------------------
static NSView* __strong *widgets = NULL;
static int widget_count = 0;
static int widget_capacity = 0;

int aether_ui_register_widget(void* widget) {
    if (widget_count >= widget_capacity) {
        widget_capacity = widget_capacity == 0 ? 64 : widget_capacity * 2;
        NSView* __strong *new_widgets = (__strong NSView**)calloc(widget_capacity, sizeof(NSView*));
        if (widgets) {
            for (int i = 0; i < widget_count; i++) new_widgets[i] = widgets[i];
            free(widgets);
        }
        widgets = new_widgets;
    }
    widgets[widget_count] = (__bridge NSView*)widget;
    widget_count++;
    return widget_count; // 1-based
}

void* aether_ui_get_widget(int handle) {
    if (handle < 1 || handle > widget_count) return NULL;
    return (__bridge void*)widgets[handle - 1];
}

// ---------------------------------------------------------------------------
// Reactive state — same implementation as GTK4 backend.
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
    return state_count;
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
    if (val == (int)val)
        snprintf(buf, sizeof(buf), "%s%d%s", b->prefix, (int)val, b->suffix);
    else
        snprintf(buf, sizeof(buf), "%s%.2f%s", b->prefix, val, b->suffix);
    aether_ui_text_set_string(text_handle, buf);
}

static void update_text_bindings(int state_handle) {
    double val = aether_ui_state_get(state_handle);
    for (int i = 0; i < text_binding_count; i++) {
        TextBinding* b = &text_bindings[i];
        if (b->state_handle != state_handle) continue;
        char buf[256];
        if (val == (int)val)
            snprintf(buf, sizeof(buf), "%s%d%s", b->prefix, (int)val, b->suffix);
        else
            snprintf(buf, sizeof(buf), "%s%.2f%s", b->prefix, val, b->suffix);
        aether_ui_text_set_string(b->text_handle, buf);
    }
}

// ---------------------------------------------------------------------------
// App lifecycle
// ---------------------------------------------------------------------------

@interface AetherAppDelegate : NSObject <NSApplicationDelegate>
@property (strong) NSWindow* window;
@property (assign) int rootHandle;
@end

@implementation AetherAppDelegate
- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    if (self.rootHandle > 0) {
        NSView* root = (__bridge NSView*)aether_ui_get_widget(self.rootHandle);
        if (root) {
            [self.window setContentView:root];
        }
    }
    [self.window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    return YES;
}
@end

static AetherAppDelegate* app_delegate = nil;

int aether_ui_app_create(const char* title, int width, int height) {
    NSRect frame = NSMakeRect(200, 200, width, height);
    NSWindowStyleMask style = NSWindowStyleMaskTitled |
                               NSWindowStyleMaskClosable |
                               NSWindowStyleMaskMiniaturizable |
                               NSWindowStyleMaskResizable;
    NSWindow* window = [[NSWindow alloc] initWithContentRect:frame
                                                   styleMask:style
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];
    [window setTitle:[NSString stringWithUTF8String:title ? title : ""]];

    app_delegate = [[AetherAppDelegate alloc] init];
    app_delegate.window = window;
    app_delegate.rootHandle = 0;
    return 1; // single app
}

void aether_ui_app_set_body(int app_handle, int root_handle) {
    (void)app_handle;
    if (app_delegate) app_delegate.rootHandle = root_handle;
}

void aether_ui_app_run_raw(int app_handle) {
    (void)app_handle;
    @autoreleasepool {
        NSApplication* app = [NSApplication sharedApplication];
        [app setDelegate:app_delegate];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];

        // Create a basic menu bar
        NSMenu* menubar = [[NSMenu alloc] init];
        NSMenuItem* appMenuItem = [[NSMenuItem alloc] init];
        [menubar addItem:appMenuItem];
        NSMenu* appMenu = [[NSMenu alloc] init];
        [appMenu addItemWithTitle:@"Quit" action:@selector(terminate:) keyEquivalent:@"q"];
        [appMenuItem setSubmenu:appMenu];
        [app setMainMenu:menubar];

        [app run];
    }
}

// ---------------------------------------------------------------------------
// Widget creation
// ---------------------------------------------------------------------------

int aether_ui_text_create(const char* text) {
    NSTextField* label = [NSTextField labelWithString:
        [NSString stringWithUTF8String:text ? text : ""]];
    [label setEditable:NO];
    [label setBordered:NO];
    [label setSelectable:NO];
    [label setBackgroundColor:[NSColor clearColor]];
    return aether_ui_register_widget((__bridge void*)label);
}

void aether_ui_text_set_string(int handle, const char* text) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (v && [v isKindOfClass:[NSTextField class]]) {
        [(NSTextField*)v setStringValue:
            [NSString stringWithUTF8String:text ? text : ""]];
    }
}

// Button target that invokes an AeClosure
@interface AetherButtonTarget : NSObject
@property (assign) AeClosure* closure;
- (void)buttonPressed:(id)sender;
@end

@implementation AetherButtonTarget
- (void)buttonPressed:(id)sender {
    (void)sender;
    if (self.closure && self.closure->fn) {
        ((void(*)(void*))self.closure->fn)(self.closure->env);
    }
}
@end

// Keep strong refs so ARC doesn't release them
static NSMutableArray* button_targets = nil;

int aether_ui_button_create(const char* label, void* boxed_closure) {
    NSButton* btn = [NSButton buttonWithTitle:
        [NSString stringWithUTF8String:label ? label : ""]
                                       target:nil action:nil];
    if (boxed_closure) {
        if (!button_targets) button_targets = [NSMutableArray array];
        AetherButtonTarget* target = [[AetherButtonTarget alloc] init];
        target.closure = (AeClosure*)boxed_closure;
        [btn setTarget:target];
        [btn setAction:@selector(buttonPressed:)];
        [button_targets addObject:target];
    }
    return aether_ui_register_widget((__bridge void*)btn);
}

int aether_ui_vstack_create(int spacing) {
    NSStackView* stack = [[NSStackView alloc] init];
    [stack setOrientation:NSUserInterfaceLayoutOrientationVertical];
    [stack setSpacing:spacing];
    [stack setTranslatesAutoresizingMaskIntoConstraints:NO];
    return aether_ui_register_widget((__bridge void*)stack);
}

int aether_ui_hstack_create(int spacing) {
    NSStackView* stack = [[NSStackView alloc] init];
    [stack setOrientation:NSUserInterfaceLayoutOrientationHorizontal];
    [stack setSpacing:spacing];
    [stack setTranslatesAutoresizingMaskIntoConstraints:NO];
    return aether_ui_register_widget((__bridge void*)stack);
}

int aether_ui_spacer_create(void) {
    NSView* spacer = [[NSView alloc] init];
    [spacer setTranslatesAutoresizingMaskIntoConstraints:NO];
    // Flexible in both directions
    [spacer setContentHuggingPriority:1
                       forOrientation:NSLayoutConstraintOrientationHorizontal];
    [spacer setContentHuggingPriority:1
                       forOrientation:NSLayoutConstraintOrientationVertical];
    return aether_ui_register_widget((__bridge void*)spacer);
}

int aether_ui_divider_create(void) {
    NSBox* sep = [[NSBox alloc] init];
    [sep setBoxType:NSBoxSeparator];
    return aether_ui_register_widget((__bridge void*)sep);
}

// ---------------------------------------------------------------------------
// Input widgets
// ---------------------------------------------------------------------------

int aether_ui_textfield_create(const char* placeholder, void* boxed_closure) {
    NSTextField* field = [[NSTextField alloc] init];
    [field setTranslatesAutoresizingMaskIntoConstraints:NO];
    if (placeholder && *placeholder) {
        [field setPlaceholderString:
            [NSString stringWithUTF8String:placeholder]];
    }
    // TODO: wire on_change callback via NSControlTextEditingDelegate
    (void)boxed_closure;
    return aether_ui_register_widget((__bridge void*)field);
}

void aether_ui_textfield_set_text(int handle, const char* text) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (v && [v isKindOfClass:[NSTextField class]]) {
        [(NSTextField*)v setStringValue:
            [NSString stringWithUTF8String:text ? text : ""]];
    }
}

const char* aether_ui_textfield_get_text(int handle) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (v && [v isKindOfClass:[NSTextField class]]) {
        return [[(NSTextField*)v stringValue] UTF8String];
    }
    return "";
}

int aether_ui_securefield_create(const char* placeholder, void* boxed_closure) {
    NSSecureTextField* field = [[NSSecureTextField alloc] init];
    [field setTranslatesAutoresizingMaskIntoConstraints:NO];
    if (placeholder && *placeholder) {
        [field setPlaceholderString:
            [NSString stringWithUTF8String:placeholder]];
    }
    (void)boxed_closure;
    return aether_ui_register_widget((__bridge void*)field);
}

int aether_ui_toggle_create(const char* label, void* boxed_closure) {
    NSButton* check = [NSButton checkboxWithTitle:
        [NSString stringWithUTF8String:label ? label : ""]
                                           target:nil action:nil];
    (void)boxed_closure;
    return aether_ui_register_widget((__bridge void*)check);
}

void aether_ui_toggle_set_active(int handle, int active) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (v && [v isKindOfClass:[NSButton class]]) {
        [(NSButton*)v setState:active ? NSControlStateValueOn : NSControlStateValueOff];
    }
}

int aether_ui_toggle_get_active(int handle) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (v && [v isKindOfClass:[NSButton class]]) {
        return [(NSButton*)v state] == NSControlStateValueOn ? 1 : 0;
    }
    return 0;
}

int aether_ui_slider_create(double min_val, double max_val,
                            double initial, void* boxed_closure) {
    NSSlider* slider = [NSSlider sliderWithValue:initial
                                        minValue:min_val
                                        maxValue:max_val
                                          target:nil action:nil];
    [slider setTranslatesAutoresizingMaskIntoConstraints:NO];
    (void)boxed_closure;
    return aether_ui_register_widget((__bridge void*)slider);
}

void aether_ui_slider_set_value(int handle, double value) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (v && [v isKindOfClass:[NSSlider class]]) {
        [(NSSlider*)v setDoubleValue:value];
    }
}

double aether_ui_slider_get_value(int handle) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (v && [v isKindOfClass:[NSSlider class]]) {
        return [(NSSlider*)v doubleValue];
    }
    return 0.0;
}

int aether_ui_picker_create(void* boxed_closure) {
    NSPopUpButton* popup = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    [popup setTranslatesAutoresizingMaskIntoConstraints:NO];
    (void)boxed_closure;
    return aether_ui_register_widget((__bridge void*)popup);
}

void aether_ui_picker_add_item(int handle, const char* item) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (v && [v isKindOfClass:[NSPopUpButton class]]) {
        [(NSPopUpButton*)v addItemWithTitle:
            [NSString stringWithUTF8String:item ? item : ""]];
    }
}

void aether_ui_picker_set_selected(int handle, int index) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (v && [v isKindOfClass:[NSPopUpButton class]]) {
        [(NSPopUpButton*)v selectItemAtIndex:index];
    }
}

int aether_ui_picker_get_selected(int handle) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (v && [v isKindOfClass:[NSPopUpButton class]]) {
        return (int)[(NSPopUpButton*)v indexOfSelectedItem];
    }
    return 0;
}

int aether_ui_textarea_create(const char* placeholder, void* boxed_closure) {
    NSTextView* tv = [[NSTextView alloc] init];
    [tv setRichText:NO];
    [tv setEditable:YES];
    [tv setSelectable:YES];

    NSScrollView* scrollView = [[NSScrollView alloc] init];
    [scrollView setDocumentView:tv];
    [scrollView setHasVerticalScroller:YES];
    [scrollView setTranslatesAutoresizingMaskIntoConstraints:NO];

    (void)placeholder;
    (void)boxed_closure;
    int scroll_handle = aether_ui_register_widget((__bridge void*)scrollView);
    aether_ui_register_widget((__bridge void*)tv);
    return scroll_handle;
}

void aether_ui_textarea_set_text(int handle, const char* text) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle + 1);
    if (v && [v isKindOfClass:[NSTextView class]]) {
        [(NSTextView*)v setString:
            [NSString stringWithUTF8String:text ? text : ""]];
    }
}

char* aether_ui_textarea_get_text(int handle) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle + 1);
    if (v && [v isKindOfClass:[NSTextView class]]) {
        return strdup([[(NSTextView*)v string] UTF8String]);
    }
    return strdup("");
}

int aether_ui_scrollview_create(void) {
    NSScrollView* sv = [[NSScrollView alloc] init];
    [sv setHasVerticalScroller:YES];
    [sv setTranslatesAutoresizingMaskIntoConstraints:NO];
    return aether_ui_register_widget((__bridge void*)sv);
}

int aether_ui_progressbar_create(double fraction) {
    NSProgressIndicator* bar = [[NSProgressIndicator alloc] init];
    [bar setStyle:NSProgressIndicatorStyleBar];
    [bar setIndeterminate:NO];
    [bar setMinValue:0.0];
    [bar setMaxValue:1.0];
    [bar setDoubleValue:fraction];
    [bar setTranslatesAutoresizingMaskIntoConstraints:NO];
    return aether_ui_register_widget((__bridge void*)bar);
}

void aether_ui_progressbar_set_fraction(int handle, double fraction) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (v && [v isKindOfClass:[NSProgressIndicator class]]) {
        [(NSProgressIndicator*)v setDoubleValue:fraction];
    }
}

// ---------------------------------------------------------------------------
// Layout containers
// ---------------------------------------------------------------------------

int aether_ui_zstack_create(void) {
    NSView* container = [[NSView alloc] init];
    [container setTranslatesAutoresizingMaskIntoConstraints:NO];
    return aether_ui_register_widget((__bridge void*)container);
}

int aether_ui_form_create(void) {
    return aether_ui_vstack_create(16);
}

int aether_ui_form_section_create(const char* title) {
    NSBox* box = [[NSBox alloc] init];
    [box setTitle:[NSString stringWithUTF8String:title ? title : ""]];
    [box setTranslatesAutoresizingMaskIntoConstraints:NO];

    NSStackView* inner = [[NSStackView alloc] init];
    [inner setOrientation:NSUserInterfaceLayoutOrientationVertical];
    [inner setSpacing:8];
    [box setContentView:inner];

    int frame_handle = aether_ui_register_widget((__bridge void*)box);
    aether_ui_register_widget((__bridge void*)inner);
    return frame_handle;
}

int aether_ui_navstack_create(void) {
    // Use a simple NSView as container; push/pop swap contentView
    NSView* container = [[NSView alloc] init];
    [container setTranslatesAutoresizingMaskIntoConstraints:NO];
    return aether_ui_register_widget((__bridge void*)container);
}

void aether_ui_navstack_push(int handle, const char* title, int body_handle) {
    (void)title;
    NSView* container = (__bridge NSView*)aether_ui_get_widget(handle);
    NSView* body = (__bridge NSView*)aether_ui_get_widget(body_handle);
    if (!container || !body) return;
    // Remove old children, add new
    for (NSView* sub in [container subviews]) {
        [sub removeFromSuperview];
    }
    [container addSubview:body];
}

void aether_ui_navstack_pop(int handle) {
    (void)handle;
}

// ---------------------------------------------------------------------------
// Styling
// ---------------------------------------------------------------------------

void aether_ui_set_bg_color(int handle, double r, double g, double b, double a) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (!v) return;
    [v setWantsLayer:YES];
    v.layer.backgroundColor = [[NSColor colorWithRed:r green:g blue:b alpha:a] CGColor];
}

void aether_ui_set_bg_gradient(int handle,
                               double r1, double g1, double b1,
                               double r2, double g2, double b2, int vertical) {
    (void)handle; (void)r1; (void)g1; (void)b1;
    (void)r2; (void)g2; (void)b2; (void)vertical;
    // TODO: CAGradientLayer
}

void aether_ui_set_text_color(int handle, double r, double g, double b) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (v && [v isKindOfClass:[NSTextField class]]) {
        [(NSTextField*)v setTextColor:[NSColor colorWithRed:r green:g blue:b alpha:1.0]];
    }
}

void aether_ui_set_font_size(int handle, double size) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (v && [v isKindOfClass:[NSTextField class]]) {
        [(NSTextField*)v setFont:[NSFont systemFontOfSize:size]];
    }
}

void aether_ui_set_font_bold(int handle, int bold) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (v && [v isKindOfClass:[NSTextField class]]) {
        NSFont* font = [(NSTextField*)v font];
        CGFloat size = font ? [font pointSize] : 13.0;
        if (bold)
            [(NSTextField*)v setFont:[NSFont boldSystemFontOfSize:size]];
        else
            [(NSTextField*)v setFont:[NSFont systemFontOfSize:size]];
    }
}

void aether_ui_set_corner_radius(int handle, double radius) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (!v) return;
    [v setWantsLayer:YES];
    v.layer.cornerRadius = radius;
    v.layer.masksToBounds = YES;
}

void aether_ui_set_edge_insets(int handle, double top, double right,
                               double bottom, double left) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (v && [v isKindOfClass:[NSStackView class]]) {
        [(NSStackView*)v setEdgeInsets:NSEdgeInsetsMake(top, left, bottom, right)];
    }
}

void aether_ui_set_width(int handle, int width) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (!v) return;
    [v setTranslatesAutoresizingMaskIntoConstraints:NO];
    [NSLayoutConstraint constraintWithItem:v attribute:NSLayoutAttributeWidth
        relatedBy:NSLayoutRelationEqual toItem:nil attribute:NSLayoutAttributeNotAnAttribute
        multiplier:1.0 constant:width].active = YES;
}

void aether_ui_set_height(int handle, int height) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (!v) return;
    [v setTranslatesAutoresizingMaskIntoConstraints:NO];
    [NSLayoutConstraint constraintWithItem:v attribute:NSLayoutAttributeHeight
        relatedBy:NSLayoutRelationEqual toItem:nil attribute:NSLayoutAttributeNotAnAttribute
        multiplier:1.0 constant:height].active = YES;
}

void aether_ui_set_opacity(int handle, double opacity) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (v) [v setAlphaValue:opacity];
}

void aether_ui_set_enabled(int handle, int enabled) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (v && [v isKindOfClass:[NSControl class]]) {
        [(NSControl*)v setEnabled:enabled != 0];
    }
}

void aether_ui_set_tooltip(int handle, const char* text) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (v) [v setToolTip:[NSString stringWithUTF8String:text ? text : ""]];
}

void aether_ui_set_distribution(int handle, int distribution) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (v && [v isKindOfClass:[NSStackView class]]) {
        [(NSStackView*)v setDistribution:distribution];
    }
}

void aether_ui_set_alignment(int handle, int alignment) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (v && [v isKindOfClass:[NSStackView class]]) {
        [(NSStackView*)v setAlignment:alignment];
    }
}

void aether_ui_match_parent_width(int handle) {
    (void)handle; // Auto-layout handles this
}

void aether_ui_match_parent_height(int handle) {
    (void)handle;
}

void aether_ui_set_margin(int handle, int top, int right, int bottom, int left) {
    (void)handle; (void)top; (void)right; (void)bottom; (void)left;
    // NSView doesn't have margin; use layout constraints or edge insets
}

// ---------------------------------------------------------------------------
// System integration
// ---------------------------------------------------------------------------

void aether_ui_alert_impl(const char* title, const char* message) {
    NSAlert* alert = [[NSAlert alloc] init];
    [alert setMessageText:[NSString stringWithUTF8String:title ? title : ""]];
    [alert setInformativeText:[NSString stringWithUTF8String:message ? message : ""]];
    [alert addButtonWithTitle:@"OK"];
    [alert runModal];
}

char* aether_ui_file_open(const char* title) {
    (void)title;
    return strdup("");
}

void aether_ui_clipboard_write_impl(const char* text) {
    NSPasteboard* pb = [NSPasteboard generalPasteboard];
    [pb clearContents];
    [pb setString:[NSString stringWithUTF8String:text ? text : ""]
          forType:NSPasteboardTypeString];
}

int aether_ui_timer_create_impl(int interval_ms, void* boxed_closure) {
    (void)interval_ms; (void)boxed_closure;
    return 0; // TODO: NSTimer
}

void aether_ui_timer_cancel_impl(int timer_id) {
    (void)timer_id;
}

void aether_ui_open_url_impl(const char* url) {
    if (!url) return;
    [[NSWorkspace sharedWorkspace] openURL:
        [NSURL URLWithString:[NSString stringWithUTF8String:url]]];
}

int aether_ui_dark_mode_check(void) {
    NSString* appearance = [[NSUserDefaults standardUserDefaults]
        stringForKey:@"AppleInterfaceStyle"];
    return [appearance isEqualToString:@"Dark"] ? 1 : 0;
}

int aether_ui_window_create_impl(const char* title, int width, int height) {
    (void)title; (void)width; (void)height;
    return 0; // TODO
}

void aether_ui_window_set_body_impl(int h, int r) { (void)h; (void)r; }
void aether_ui_window_show_impl(int h) { (void)h; }
void aether_ui_window_close_impl(int h) { (void)h; }

int aether_ui_sheet_create_impl(const char* t, int w, int h) {
    (void)t; (void)w; (void)h; return 0;
}
void aether_ui_sheet_set_body_impl(int h, int r) { (void)h; (void)r; }
void aether_ui_sheet_present_impl(int h) { (void)h; }
void aether_ui_sheet_dismiss_impl(int h) { (void)h; }

int aether_ui_image_create(const char* filepath) {
    NSImageView* iv = [[NSImageView alloc] init];
    if (filepath && *filepath) {
        NSImage* img = [[NSImage alloc] initWithContentsOfFile:
            [NSString stringWithUTF8String:filepath]];
        [iv setImage:img];
    }
    return aether_ui_register_widget((__bridge void*)iv);
}

void aether_ui_image_set_size(int handle, int width, int height) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (v) [v setFrameSize:NSMakeSize(width, height)];
}

// ---------------------------------------------------------------------------
// Canvas (stub — Core Graphics / Quartz 2D)
// ---------------------------------------------------------------------------
int aether_ui_canvas_create_impl(int w, int h) { (void)w; (void)h; return 0; }
int aether_ui_canvas_get_widget(int c) { (void)c; return 0; }
void aether_ui_canvas_begin_path_impl(int c) { (void)c; }
void aether_ui_canvas_move_to_impl(int c, float x, float y) { (void)c; (void)x; (void)y; }
void aether_ui_canvas_line_to_impl(int c, float x, float y) { (void)c; (void)x; (void)y; }
void aether_ui_canvas_stroke_impl(int c, float r, float g, float b, float a, float lw) {
    (void)c; (void)r; (void)g; (void)b; (void)a; (void)lw;
}
void aether_ui_canvas_fill_rect_impl(int c, float x, float y, float w, float h,
                                     float r, float g, float b, float a) {
    (void)c; (void)x; (void)y; (void)w; (void)h; (void)r; (void)g; (void)b; (void)a;
}
void aether_ui_canvas_clear_impl(int c) { (void)c; }
void aether_ui_canvas_redraw_impl(int c) { (void)c; }

// Events (stubs)
void aether_ui_on_hover_impl(int h, void* c) { (void)h; (void)c; }
void aether_ui_on_double_click_impl(int h, void* c) { (void)h; (void)c; }
void aether_ui_on_click_impl(int h, void* c) { (void)h; (void)c; }
void aether_ui_animate_opacity_impl(int h, double t, int d) { (void)h; (void)t; (void)d; }
void aether_ui_remove_child_impl(int p, int c) { (void)p; (void)c; }
void aether_ui_clear_children_impl(int h) { (void)h; }

// ---------------------------------------------------------------------------
// Widget tree
// ---------------------------------------------------------------------------

void aether_ui_widget_add_child_ctx(void* parent_ctx, int child_handle) {
    int parent_handle = (int)(intptr_t)parent_ctx;
    NSView* parent = (__bridge NSView*)aether_ui_get_widget(parent_handle);
    NSView* child = (__bridge NSView*)aether_ui_get_widget(child_handle);
    if (!parent || !child) return;

    if ([parent isKindOfClass:[NSStackView class]]) {
        [(NSStackView*)parent addArrangedSubview:child];
    } else if ([parent isKindOfClass:[NSScrollView class]]) {
        [(NSScrollView*)parent setDocumentView:child];
    } else {
        [parent addSubview:child];
    }
}

void aether_ui_widget_set_hidden(int handle, int hidden) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (v) [v setHidden:hidden != 0];
}

#endif // __APPLE__
