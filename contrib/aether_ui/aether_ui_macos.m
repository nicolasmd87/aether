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
#import <QuartzCore/QuartzCore.h>
#include "aether_ui_gtk4.h"  // same API surface
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// ---------------------------------------------------------------------------
// Closure struct — must match Aether codegen's _AeClosure layout.
// ---------------------------------------------------------------------------
typedef struct {
    void (*fn)(void);
    void* env;
} AeClosure;

// ---------------------------------------------------------------------------
// Widget type tags — mirror of widget_type_name() in the GTK4 backend.
// Kept in a parallel array so the test server can report types without
// guessing via isKindOfClass:.
// ---------------------------------------------------------------------------
enum {
    AUI_UNKNOWN = 0,
    AUI_TEXT, AUI_BUTTON, AUI_TOGGLE, AUI_SLIDER, AUI_PICKER,
    AUI_TEXTFIELD, AUI_SECUREFIELD, AUI_TEXTAREA, AUI_TEXTAREA_INNER,
    AUI_PROGRESSBAR, AUI_DIVIDER, AUI_SCROLLVIEW,
    AUI_VSTACK, AUI_HSTACK, AUI_ZSTACK, AUI_SPACER,
    AUI_CANVAS, AUI_IMAGE, AUI_FORM_SECTION, AUI_FORM_SECTION_INNER,
    AUI_NAVSTACK, AUI_BANNER, AUI_WINDOW, AUI_SHEET
};

// ---------------------------------------------------------------------------
// Widget registry — flat array of NSView*, 1-based handles.
// ---------------------------------------------------------------------------
static NSView* __strong *widgets = NULL;
static int* widget_types = NULL;
static int widget_count = 0;
static int widget_capacity = 0;

static int register_widget_typed(void* widget, int type) {
    if (widget_count >= widget_capacity) {
        int new_cap = widget_capacity == 0 ? 64 : widget_capacity * 2;
        NSView* __strong *new_widgets = (__strong NSView**)calloc(new_cap, sizeof(NSView*));
        int* new_types = (int*)calloc(new_cap, sizeof(int));
        if (widgets) {
            for (int i = 0; i < widget_count; i++) {
                new_widgets[i] = widgets[i];
                new_types[i] = widget_types[i];
            }
            free(widgets);
            free(widget_types);
        }
        widgets = new_widgets;
        widget_types = new_types;
        widget_capacity = new_cap;
    }
    widgets[widget_count] = (__bridge NSView*)widget;
    widget_types[widget_count] = type;
    widget_count++;
    return widget_count;
}

int aether_ui_register_widget(void* widget) {
    return register_widget_typed(widget, AUI_UNKNOWN);
}

void* aether_ui_get_widget(int handle) {
    if (handle < 1 || handle > widget_count) return NULL;
    return (__bridge void*)widgets[handle - 1];
}

static int get_widget_type(int handle) {
    if (handle < 1 || handle > widget_count) return AUI_UNKNOWN;
    return widget_types[handle - 1];
}

static int handle_for_view(NSView* v) {
    if (!v) return 0;
    for (int i = 0; i < widget_count; i++) {
        if (widgets[i] == v) return i + 1;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Reactive state
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
    (void)notification;
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
    (void)sender;
    return YES;
}
@end

static AetherAppDelegate* app_delegate = nil;
static NSWindow* primary_window = nil;

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
    primary_window = window;
    return 1;
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
    return register_widget_typed((__bridge void*)label, AUI_TEXT);
}

void aether_ui_text_set_string(int handle, const char* text) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (v && [v isKindOfClass:[NSTextField class]]) {
        [(NSTextField*)v setStringValue:
            [NSString stringWithUTF8String:text ? text : ""]];
    }
}

// ---------------------------------------------------------------------------
// Button click target — holds an AeClosure and dispatches to it.
// ---------------------------------------------------------------------------
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
static NSMutableArray* retained_targets = nil;
static void retain_target(id obj) {
    if (!retained_targets) retained_targets = [NSMutableArray array];
    [retained_targets addObject:obj];
}

// Default low content-hugging priority so buttons fill horizontal space in
// hstacks (matching GTK4's grid-like look on single-char button rows).
static void configure_button(NSButton* btn) {
    [btn setContentHuggingPriority:200
                    forOrientation:NSLayoutConstraintOrientationHorizontal];
}

int aether_ui_button_create(const char* label, void* boxed_closure) {
    NSButton* btn = [NSButton buttonWithTitle:
        [NSString stringWithUTF8String:label ? label : ""]
                                       target:nil action:nil];
    configure_button(btn);
    if (boxed_closure) {
        AetherButtonTarget* target = [[AetherButtonTarget alloc] init];
        target.closure = (AeClosure*)boxed_closure;
        [btn setTarget:target];
        [btn setAction:@selector(buttonPressed:)];
        retain_target(target);
    }
    return register_widget_typed((__bridge void*)btn, AUI_BUTTON);
}

int aether_ui_button_create_plain(const char* label) {
    NSButton* btn = [NSButton buttonWithTitle:
        [NSString stringWithUTF8String:label ? label : ""]
                                       target:nil action:nil];
    configure_button(btn);
    return register_widget_typed((__bridge void*)btn, AUI_BUTTON);
}

void aether_ui_set_onclick_ctx(void* ctx, void* boxed_closure) {
    int handle = (int)(intptr_t)ctx;
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (!v || !boxed_closure) return;
    if ([v isKindOfClass:[NSButton class]]) {
        AetherButtonTarget* target = [[AetherButtonTarget alloc] init];
        target.closure = (AeClosure*)boxed_closure;
        [(NSButton*)v setTarget:target];
        [(NSButton*)v setAction:@selector(buttonPressed:)];
        retain_target(target);
    } else {
        // For non-button widgets, attach a click gesture recognizer
        aether_ui_on_click_impl(handle, boxed_closure);
    }
}

int aether_ui_vstack_create(int spacing) {
    NSStackView* stack = [[NSStackView alloc] init];
    [stack setOrientation:NSUserInterfaceLayoutOrientationVertical];
    [stack setSpacing:spacing];
    [stack setAlignment:NSLayoutAttributeLeading];
    [stack setTranslatesAutoresizingMaskIntoConstraints:NO];
    return register_widget_typed((__bridge void*)stack, AUI_VSTACK);
}

int aether_ui_hstack_create(int spacing) {
    NSStackView* stack = [[NSStackView alloc] init];
    [stack setOrientation:NSUserInterfaceLayoutOrientationHorizontal];
    [stack setSpacing:spacing];
    [stack setAlignment:NSLayoutAttributeCenterY];
    // Fill distribution matches GTK4's box behavior: children grow/shrink
    // according to their content-hugging priority. Buttons (set to 200 at
    // creation) absorb leftover space; spacers (priority 1) soak up the rest.
    [stack setDistribution:NSStackViewDistributionFill];
    [stack setTranslatesAutoresizingMaskIntoConstraints:NO];
    return register_widget_typed((__bridge void*)stack, AUI_HSTACK);
}

int aether_ui_spacer_create(void) {
    NSView* spacer = [[NSView alloc] init];
    [spacer setTranslatesAutoresizingMaskIntoConstraints:NO];
    [spacer setContentHuggingPriority:1
                       forOrientation:NSLayoutConstraintOrientationHorizontal];
    [spacer setContentHuggingPriority:1
                       forOrientation:NSLayoutConstraintOrientationVertical];
    return register_widget_typed((__bridge void*)spacer, AUI_SPACER);
}

int aether_ui_divider_create(void) {
    NSBox* sep = [[NSBox alloc] init];
    [sep setBoxType:NSBoxSeparator];
    return register_widget_typed((__bridge void*)sep, AUI_DIVIDER);
}

// ---------------------------------------------------------------------------
// Input widgets — wire up AppKit target/action or delegates to AeClosures.
// ---------------------------------------------------------------------------

@interface AetherTextFieldDelegate : NSObject <NSTextFieldDelegate>
@property (assign) AeClosure* closure;
@end

@implementation AetherTextFieldDelegate
- (void)controlTextDidChange:(NSNotification*)n {
    NSTextField* tf = [n object];
    if (self.closure && self.closure->fn) {
        const char* cs = [[tf stringValue] UTF8String];
        ((void(*)(void*, const char*))self.closure->fn)(self.closure->env, cs ? cs : "");
    }
}
@end

int aether_ui_textfield_create(const char* placeholder, void* boxed_closure) {
    NSTextField* field = [[NSTextField alloc] init];
    [field setTranslatesAutoresizingMaskIntoConstraints:NO];
    [field setEditable:YES];
    [field setBordered:YES];
    [field setBezeled:YES];
    if (placeholder && *placeholder) {
        [field setPlaceholderString:[NSString stringWithUTF8String:placeholder]];
    }
    if (boxed_closure) {
        AetherTextFieldDelegate* d = [[AetherTextFieldDelegate alloc] init];
        d.closure = (AeClosure*)boxed_closure;
        [field setDelegate:d];
        retain_target(d);
    }
    return register_widget_typed((__bridge void*)field, AUI_TEXTFIELD);
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
        [field setPlaceholderString:[NSString stringWithUTF8String:placeholder]];
    }
    if (boxed_closure) {
        AetherTextFieldDelegate* d = [[AetherTextFieldDelegate alloc] init];
        d.closure = (AeClosure*)boxed_closure;
        [field setDelegate:d];
        retain_target(d);
    }
    return register_widget_typed((__bridge void*)field, AUI_SECUREFIELD);
}

// Toggle — NSButton with switch style, target invokes closure with 0/1.
@interface AetherToggleTarget : NSObject
@property (assign) AeClosure* closure;
- (void)toggleChanged:(id)sender;
@end

@implementation AetherToggleTarget
- (void)toggleChanged:(id)sender {
    NSButton* btn = (NSButton*)sender;
    int active = [btn state] == NSControlStateValueOn ? 1 : 0;
    if (self.closure && self.closure->fn) {
        ((void(*)(void*, intptr_t))self.closure->fn)(self.closure->env, (intptr_t)active);
    }
}
@end

int aether_ui_toggle_create(const char* label, void* boxed_closure) {
    NSButton* check = [NSButton checkboxWithTitle:
        [NSString stringWithUTF8String:label ? label : ""]
                                           target:nil action:nil];
    if (boxed_closure) {
        AetherToggleTarget* target = [[AetherToggleTarget alloc] init];
        target.closure = (AeClosure*)boxed_closure;
        [check setTarget:target];
        [check setAction:@selector(toggleChanged:)];
        retain_target(target);
    }
    return register_widget_typed((__bridge void*)check, AUI_TOGGLE);
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

// Slider — continuous; target invokes closure with double value.
@interface AetherSliderTarget : NSObject
@property (assign) AeClosure* closure;
- (void)sliderChanged:(id)sender;
@end

@implementation AetherSliderTarget
- (void)sliderChanged:(id)sender {
    NSSlider* s = (NSSlider*)sender;
    double val = [s doubleValue];
    if (self.closure && self.closure->fn) {
        ((void(*)(void*, double))self.closure->fn)(self.closure->env, val);
    }
}
@end

int aether_ui_slider_create(double min_val, double max_val,
                            double initial, void* boxed_closure) {
    NSSlider* slider = [NSSlider sliderWithValue:initial
                                        minValue:min_val
                                        maxValue:max_val
                                          target:nil action:nil];
    [slider setTranslatesAutoresizingMaskIntoConstraints:NO];
    [slider setContinuous:YES];
    if (boxed_closure) {
        AetherSliderTarget* target = [[AetherSliderTarget alloc] init];
        target.closure = (AeClosure*)boxed_closure;
        [slider setTarget:target];
        [slider setAction:@selector(sliderChanged:)];
        retain_target(target);
    }
    return register_widget_typed((__bridge void*)slider, AUI_SLIDER);
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

// Picker — NSPopUpButton; target invokes closure with selected index.
@interface AetherPickerTarget : NSObject
@property (assign) AeClosure* closure;
- (void)pickerChanged:(id)sender;
@end

@implementation AetherPickerTarget
- (void)pickerChanged:(id)sender {
    NSPopUpButton* p = (NSPopUpButton*)sender;
    intptr_t idx = [p indexOfSelectedItem];
    if (self.closure && self.closure->fn) {
        ((void(*)(void*, intptr_t))self.closure->fn)(self.closure->env, idx);
    }
}
@end

int aether_ui_picker_create(void* boxed_closure) {
    NSPopUpButton* popup = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    [popup setTranslatesAutoresizingMaskIntoConstraints:NO];
    if (boxed_closure) {
        AetherPickerTarget* target = [[AetherPickerTarget alloc] init];
        target.closure = (AeClosure*)boxed_closure;
        [popup setTarget:target];
        [popup setAction:@selector(pickerChanged:)];
        retain_target(target);
    }
    return register_widget_typed((__bridge void*)popup, AUI_PICKER);
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

// Textarea — NSTextView in NSScrollView. Delegate fires closure on text change.
@interface AetherTextViewDelegate : NSObject <NSTextViewDelegate>
@property (assign) AeClosure* closure;
@end

@implementation AetherTextViewDelegate
- (void)textDidChange:(NSNotification*)n {
    NSTextView* tv = [n object];
    if (self.closure && self.closure->fn) {
        const char* cs = [[tv string] UTF8String];
        ((void(*)(void*, const char*))self.closure->fn)(self.closure->env, cs ? cs : "");
    }
}
@end

int aether_ui_textarea_create(const char* placeholder, void* boxed_closure) {
    (void)placeholder;
    NSTextView* tv = [[NSTextView alloc] init];
    [tv setRichText:NO];
    [tv setEditable:YES];
    [tv setSelectable:YES];
    [tv setAutoresizingMask:NSViewWidthSizable];

    NSScrollView* scrollView = [[NSScrollView alloc] init];
    [scrollView setDocumentView:tv];
    [scrollView setHasVerticalScroller:YES];
    [scrollView setHasHorizontalScroller:NO];
    [scrollView setBorderType:NSBezelBorder];
    [scrollView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [scrollView.heightAnchor constraintGreaterThanOrEqualToConstant:80].active = YES;

    if (boxed_closure) {
        AetherTextViewDelegate* d = [[AetherTextViewDelegate alloc] init];
        d.closure = (AeClosure*)boxed_closure;
        [tv setDelegate:d];
        retain_target(d);
    }

    int scroll_handle = register_widget_typed((__bridge void*)scrollView, AUI_TEXTAREA);
    register_widget_typed((__bridge void*)tv, AUI_TEXTAREA_INNER);
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
    return register_widget_typed((__bridge void*)sv, AUI_SCROLLVIEW);
}

int aether_ui_progressbar_create(double fraction) {
    NSProgressIndicator* bar = [[NSProgressIndicator alloc] init];
    [bar setStyle:NSProgressIndicatorStyleBar];
    [bar setIndeterminate:NO];
    [bar setMinValue:0.0];
    [bar setMaxValue:1.0];
    [bar setDoubleValue:fraction];
    [bar setTranslatesAutoresizingMaskIntoConstraints:NO];
    return register_widget_typed((__bridge void*)bar, AUI_PROGRESSBAR);
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
    return register_widget_typed((__bridge void*)container, AUI_ZSTACK);
}

int aether_ui_form_create(void) {
    NSStackView* stack = [[NSStackView alloc] init];
    [stack setOrientation:NSUserInterfaceLayoutOrientationVertical];
    [stack setSpacing:16];
    [stack setAlignment:NSLayoutAttributeLeading];
    [stack setEdgeInsets:NSEdgeInsetsMake(20, 20, 20, 20)];
    [stack setTranslatesAutoresizingMaskIntoConstraints:NO];
    return register_widget_typed((__bridge void*)stack, AUI_VSTACK);
}

int aether_ui_form_section_create(const char* title) {
    NSBox* box = [[NSBox alloc] init];
    [box setTitle:[NSString stringWithUTF8String:title ? title : ""]];
    [box setTranslatesAutoresizingMaskIntoConstraints:NO];

    NSStackView* inner = [[NSStackView alloc] init];
    [inner setOrientation:NSUserInterfaceLayoutOrientationVertical];
    [inner setSpacing:8];
    [inner setAlignment:NSLayoutAttributeLeading];
    [inner setEdgeInsets:NSEdgeInsetsMake(8, 8, 8, 8)];
    [box setContentView:inner];

    int frame_handle = register_widget_typed((__bridge void*)box, AUI_FORM_SECTION);
    register_widget_typed((__bridge void*)inner, AUI_FORM_SECTION_INNER);
    return frame_handle;
}

int aether_ui_navstack_create(void) {
    NSView* container = [[NSView alloc] init];
    [container setTranslatesAutoresizingMaskIntoConstraints:NO];
    return register_widget_typed((__bridge void*)container, AUI_NAVSTACK);
}

void aether_ui_navstack_push(int handle, const char* title, int body_handle) {
    (void)title;
    NSView* container = (__bridge NSView*)aether_ui_get_widget(handle);
    NSView* body = (__bridge NSView*)aether_ui_get_widget(body_handle);
    if (!container || !body) return;
    for (NSView* sub in [[container subviews] copy]) {
        [sub removeFromSuperview];
    }
    [body setTranslatesAutoresizingMaskIntoConstraints:NO];
    [container addSubview:body];
    [body.leadingAnchor constraintEqualToAnchor:container.leadingAnchor].active = YES;
    [body.trailingAnchor constraintEqualToAnchor:container.trailingAnchor].active = YES;
    [body.topAnchor constraintEqualToAnchor:container.topAnchor].active = YES;
    [body.bottomAnchor constraintEqualToAnchor:container.bottomAnchor].active = YES;
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
    if ([v isKindOfClass:[NSButton class]]) {
        NSButton* btn = (NSButton*)v;
        [btn setBezelStyle:NSBezelStyleRegularSquare];
        [btn setBordered:NO];
    }
}

void aether_ui_set_bg_gradient(int handle,
                               double r1, double g1, double b1,
                               double r2, double g2, double b2, int vertical) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (!v) return;
    [v setWantsLayer:YES];
    CAGradientLayer* grad = [CAGradientLayer layer];
    grad.frame = v.bounds;
    grad.colors = @[
        (id)[[NSColor colorWithRed:r1 green:g1 blue:b1 alpha:1.0] CGColor],
        (id)[[NSColor colorWithRed:r2 green:g2 blue:b2 alpha:1.0] CGColor]
    ];
    if (vertical) {
        grad.startPoint = CGPointMake(0.5, 0.0);
        grad.endPoint = CGPointMake(0.5, 1.0);
    } else {
        grad.startPoint = CGPointMake(0.0, 0.5);
        grad.endPoint = CGPointMake(1.0, 0.5);
    }
    grad.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
    [v.layer insertSublayer:grad atIndex:0];
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
    [v.widthAnchor constraintEqualToConstant:width].active = YES;
}

void aether_ui_set_height(int handle, int height) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (!v) return;
    [v setTranslatesAutoresizingMaskIntoConstraints:NO];
    [v.heightAnchor constraintEqualToConstant:height].active = YES;
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
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (!v) return;
    [v setContentHuggingPriority:1
                  forOrientation:NSLayoutConstraintOrientationHorizontal];
}

void aether_ui_match_parent_height(int handle) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (!v) return;
    [v setContentHuggingPriority:1
                  forOrientation:NSLayoutConstraintOrientationVertical];
}

void aether_ui_set_margin(int handle, int top, int right, int bottom, int left) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (v && [v isKindOfClass:[NSStackView class]]) {
        [(NSStackView*)v setEdgeInsets:NSEdgeInsetsMake(top, left, bottom, right)];
    }
}

// ---------------------------------------------------------------------------
// Context-aware styling — cast _ctx (void*) to int handle and delegate.
// ---------------------------------------------------------------------------
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
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    if (title) [panel setTitle:[NSString stringWithUTF8String:title]];
    [panel setCanChooseFiles:YES];
    [panel setCanChooseDirectories:NO];
    [panel setAllowsMultipleSelection:NO];
    if ([panel runModal] == NSModalResponseOK) {
        NSURL* url = [[panel URLs] firstObject];
        if (url) return strdup([[url path] UTF8String]);
    }
    return strdup("");
}

void aether_ui_clipboard_write_impl(const char* text) {
    NSPasteboard* pb = [NSPasteboard generalPasteboard];
    [pb clearContents];
    [pb setString:[NSString stringWithUTF8String:text ? text : ""]
          forType:NSPasteboardTypeString];
}

// Timer — NSTimer scheduled on the main runloop. Fires closure on every tick.
@interface AetherTimerTarget : NSObject
@property (assign) AeClosure* closure;
@property (strong) NSTimer* timer;
- (void)tick:(NSTimer*)t;
@end

@implementation AetherTimerTarget
- (void)tick:(NSTimer*)t {
    (void)t;
    if (self.closure && self.closure->fn) {
        ((void(*)(void*))self.closure->fn)(self.closure->env);
    }
}
@end

static NSMutableArray<AetherTimerTarget*>* active_timers = nil;

int aether_ui_timer_create_impl(int interval_ms, void* boxed_closure) {
    if (!boxed_closure || interval_ms <= 0) return 0;
    if (!active_timers) active_timers = [NSMutableArray array];
    AetherTimerTarget* t = [[AetherTimerTarget alloc] init];
    t.closure = (AeClosure*)boxed_closure;
    NSTimeInterval interval = (NSTimeInterval)interval_ms / 1000.0;
    t.timer = [NSTimer scheduledTimerWithTimeInterval:interval
                                               target:t
                                             selector:@selector(tick:)
                                             userInfo:nil
                                              repeats:YES];
    [active_timers addObject:t];
    return (int)[active_timers count];  // 1-based id
}

void aether_ui_timer_cancel_impl(int timer_id) {
    if (!active_timers) return;
    if (timer_id < 1 || timer_id > (int)[active_timers count]) return;
    AetherTimerTarget* t = active_timers[timer_id - 1];
    [t.timer invalidate];
    t.timer = nil;
}

void aether_ui_open_url_impl(const char* url) {
    if (!url) return;
    [[NSWorkspace sharedWorkspace] openURL:
        [NSURL URLWithString:[NSString stringWithUTF8String:url]]];
}

int aether_ui_dark_mode_check(void) {
    if (@available(macOS 10.14, *)) {
        NSAppearance* a = [NSApp effectiveAppearance];
        NSAppearanceName name = [a bestMatchFromAppearancesWithNames:@[
            NSAppearanceNameAqua, NSAppearanceNameDarkAqua]];
        return [name isEqualToString:NSAppearanceNameDarkAqua] ? 1 : 0;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Multi-window support — NSWindow per handle.
// ---------------------------------------------------------------------------
static NSMutableArray<NSWindow*>* extra_windows = nil;

int aether_ui_window_create_impl(const char* title, int width, int height) {
    if (!extra_windows) extra_windows = [NSMutableArray array];
    NSRect frame = NSMakeRect(250, 250, width, height);
    NSWindowStyleMask style = NSWindowStyleMaskTitled |
                               NSWindowStyleMaskClosable |
                               NSWindowStyleMaskResizable;
    NSWindow* win = [[NSWindow alloc] initWithContentRect:frame
                                                styleMask:style
                                                  backing:NSBackingStoreBuffered
                                                    defer:NO];
    [win setTitle:[NSString stringWithUTF8String:title ? title : ""]];
    [extra_windows addObject:win];
    return (int)[extra_windows count];
}

void aether_ui_window_set_body_impl(int win_handle, int root_handle) {
    if (!extra_windows || win_handle < 1 || win_handle > (int)[extra_windows count]) return;
    NSWindow* win = extra_windows[win_handle - 1];
    NSView* root = (__bridge NSView*)aether_ui_get_widget(root_handle);
    if (root) [win setContentView:root];
}

void aether_ui_window_show_impl(int win_handle) {
    if (!extra_windows || win_handle < 1 || win_handle > (int)[extra_windows count]) return;
    [extra_windows[win_handle - 1] makeKeyAndOrderFront:nil];
}

void aether_ui_window_close_impl(int win_handle) {
    if (!extra_windows || win_handle < 1 || win_handle > (int)[extra_windows count]) return;
    [extra_windows[win_handle - 1] close];
}

// Sheet — modal NSWindow attached to primary window via beginSheet:.
static NSMutableArray<NSWindow*>* sheet_windows = nil;

int aether_ui_sheet_create_impl(const char* title, int width, int height) {
    if (!sheet_windows) sheet_windows = [NSMutableArray array];
    NSRect frame = NSMakeRect(0, 0, width, height);
    NSWindow* sheet = [[NSWindow alloc] initWithContentRect:frame
                                                  styleMask:NSWindowStyleMaskTitled |
                                                            NSWindowStyleMaskClosable
                                                    backing:NSBackingStoreBuffered
                                                      defer:NO];
    [sheet setTitle:[NSString stringWithUTF8String:title ? title : ""]];
    [sheet_windows addObject:sheet];
    int idx = (int)[sheet_windows count];
    // Register under the widget registry too, so callers can reference by handle.
    return register_widget_typed((__bridge void*)[sheet contentView], AUI_SHEET) * 0 + idx;
}

void aether_ui_sheet_set_body_impl(int handle, int root_handle) {
    if (!sheet_windows || handle < 1 || handle > (int)[sheet_windows count]) return;
    NSWindow* sheet = sheet_windows[handle - 1];
    NSView* root = (__bridge NSView*)aether_ui_get_widget(root_handle);
    if (root) [sheet setContentView:root];
}

void aether_ui_sheet_present_impl(int handle) {
    if (!sheet_windows || handle < 1 || handle > (int)[sheet_windows count]) return;
    NSWindow* sheet = sheet_windows[handle - 1];
    if (primary_window) {
        [primary_window beginSheet:sheet completionHandler:^(NSModalResponse r) { (void)r; }];
    } else {
        [sheet makeKeyAndOrderFront:nil];
    }
}

void aether_ui_sheet_dismiss_impl(int handle) {
    if (!sheet_windows || handle < 1 || handle > (int)[sheet_windows count]) return;
    NSWindow* sheet = sheet_windows[handle - 1];
    if (primary_window && [sheet sheetParent]) {
        [primary_window endSheet:sheet];
    } else {
        [sheet close];
    }
}

int aether_ui_image_create(const char* filepath) {
    NSImageView* iv = [[NSImageView alloc] init];
    if (filepath && *filepath) {
        NSImage* img = [[NSImage alloc] initWithContentsOfFile:
            [NSString stringWithUTF8String:filepath]];
        [iv setImage:img];
    }
    return register_widget_typed((__bridge void*)iv, AUI_IMAGE);
}

void aether_ui_image_set_size(int handle, int width, int height) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (v) {
        [v setTranslatesAutoresizingMaskIntoConstraints:NO];
        [v.widthAnchor constraintEqualToConstant:width].active = YES;
        [v.heightAnchor constraintEqualToConstant:height].active = YES;
    }
}

// ---------------------------------------------------------------------------
// Canvas — NSView subclass replays command buffer via Core Graphics.
// ---------------------------------------------------------------------------

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
    double x, y;
    double r, g, b, a;
    double w, h;
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

@interface AetherCanvasView : NSView
@property (assign) int canvasId;
@end

@implementation AetherCanvasView
- (BOOL)isFlipped { return YES; }

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    CanvasState* cs = get_canvas_state(self.canvasId);
    if (!cs) return;
    CGContextRef cg = [[NSGraphicsContext currentContext] CGContext];
    if (!cg) return;

    for (int i = 0; i < cs->count; i++) {
        CanvasCmd* c = &cs->cmds[i];
        switch (c->type) {
            case CANVAS_BEGIN_PATH:
                CGContextBeginPath(cg);
                break;
            case CANVAS_MOVE_TO:
                CGContextMoveToPoint(cg, c->x, c->y);
                break;
            case CANVAS_LINE_TO:
                CGContextAddLineToPoint(cg, c->x, c->y);
                break;
            case CANVAS_STROKE:
                CGContextSetRGBStrokeColor(cg, c->r, c->g, c->b, c->a);
                CGContextSetLineWidth(cg, c->x);  // line_width stored in x
                CGContextSetLineCap(cg, kCGLineCapRound);
                CGContextSetLineJoin(cg, kCGLineJoinRound);
                CGContextStrokePath(cg);
                break;
            case CANVAS_FILL_RECT:
                CGContextSetRGBFillColor(cg, c->r, c->g, c->b, c->a);
                CGContextFillRect(cg, CGRectMake(c->x, c->y, c->w, c->h));
                break;
            case CANVAS_CLEAR:
                break;
        }
    }
}
@end

int aether_ui_canvas_create_impl(int width, int height) {
    AetherCanvasView* v = [[AetherCanvasView alloc] initWithFrame:NSMakeRect(0, 0, width, height)];
    [v setTranslatesAutoresizingMaskIntoConstraints:NO];
    [v.widthAnchor constraintEqualToConstant:width].active = YES;
    [v.heightAnchor constraintEqualToConstant:height].active = YES;

    if (canvas_state_count >= canvas_state_capacity) {
        canvas_state_capacity = canvas_state_capacity == 0 ? 16 : canvas_state_capacity * 2;
        canvas_states = realloc(canvas_states, sizeof(CanvasState) * canvas_state_capacity);
    }
    CanvasState* cs = &canvas_states[canvas_state_count];
    cs->cmds = NULL;
    cs->count = 0;
    cs->capacity = 0;
    canvas_state_count++;
    int canvas_id = canvas_state_count;

    v.canvasId = canvas_id;
    cs->widget_handle = register_widget_typed((__bridge void*)v, AUI_CANVAS);
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
    if (!cs) return;
    cs->count = 0;
    NSView* v = (__bridge NSView*)aether_ui_get_widget(cs->widget_handle);
    if (v) [v setNeedsDisplay:YES];
}

void aether_ui_canvas_redraw_impl(int canvas_id) {
    CanvasState* cs = get_canvas_state(canvas_id);
    if (!cs) return;
    NSView* v = (__bridge NSView*)aether_ui_get_widget(cs->widget_handle);
    if (v) [v setNeedsDisplay:YES];
}

// ---------------------------------------------------------------------------
// Events — hover (NSTrackingArea), click + double-click (gesture recognizers)
// ---------------------------------------------------------------------------

@interface AetherHoverView : NSView
@property (assign) AeClosure* closure;
@property (strong) NSTrackingArea* trackingArea;
@end

@implementation AetherHoverView
- (void)updateTrackingAreas {
    if (self.trackingArea) {
        [self removeTrackingArea:self.trackingArea];
    }
    self.trackingArea = [[NSTrackingArea alloc]
        initWithRect:[self bounds]
             options:NSTrackingMouseEnteredAndExited | NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect
               owner:self
            userInfo:nil];
    [self addTrackingArea:self.trackingArea];
    [super updateTrackingAreas];
}

- (void)mouseEntered:(NSEvent*)event {
    (void)event;
    if (self.closure && self.closure->fn) {
        ((void(*)(void*, intptr_t))self.closure->fn)(self.closure->env, (intptr_t)1);
    }
}

- (void)mouseExited:(NSEvent*)event {
    (void)event;
    if (self.closure && self.closure->fn) {
        ((void(*)(void*, intptr_t))self.closure->fn)(self.closure->env, (intptr_t)0);
    }
}
@end

// NSTrackingArea attached to an existing view via a helper object.
@interface AetherHoverMonitor : NSObject
@property (assign) AeClosure* closure;
@property (weak) NSView* view;
@property (strong) NSTrackingArea* trackingArea;
- (void)attach;
@end

@implementation AetherHoverMonitor
- (void)attach {
    self.trackingArea = [[NSTrackingArea alloc]
        initWithRect:[self.view bounds]
             options:NSTrackingMouseEnteredAndExited | NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect
               owner:self
            userInfo:nil];
    [self.view addTrackingArea:self.trackingArea];
}

- (void)mouseEntered:(NSEvent*)event {
    (void)event;
    if (self.closure && self.closure->fn) {
        ((void(*)(void*, intptr_t))self.closure->fn)(self.closure->env, (intptr_t)1);
    }
}

- (void)mouseExited:(NSEvent*)event {
    (void)event;
    if (self.closure && self.closure->fn) {
        ((void(*)(void*, intptr_t))self.closure->fn)(self.closure->env, (intptr_t)0);
    }
}
@end

void aether_ui_on_hover_impl(int handle, void* boxed_closure) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (!v || !boxed_closure) return;
    AetherHoverMonitor* m = [[AetherHoverMonitor alloc] init];
    m.closure = (AeClosure*)boxed_closure;
    m.view = v;
    [m attach];
    retain_target(m);
}

@interface AetherClickRecognizer : NSClickGestureRecognizer
@property (assign) AeClosure* closure;
- (void)clicked:(NSClickGestureRecognizer*)r;
@end

@implementation AetherClickRecognizer
- (void)clicked:(NSClickGestureRecognizer*)r {
    (void)r;
    if (self.closure && self.closure->fn) {
        ((void(*)(void*))self.closure->fn)(self.closure->env);
    }
}
@end

void aether_ui_on_click_impl(int handle, void* boxed_closure) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (!v || !boxed_closure) return;
    AetherClickRecognizer* rec = [[AetherClickRecognizer alloc] init];
    rec.closure = (AeClosure*)boxed_closure;
    [rec setTarget:rec];
    [rec setAction:@selector(clicked:)];
    rec.numberOfClicksRequired = 1;
    [v addGestureRecognizer:rec];
    retain_target(rec);
}

void aether_ui_on_double_click_impl(int handle, void* boxed_closure) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (!v || !boxed_closure) return;
    AetherClickRecognizer* rec = [[AetherClickRecognizer alloc] init];
    rec.closure = (AeClosure*)boxed_closure;
    [rec setTarget:rec];
    [rec setAction:@selector(clicked:)];
    rec.numberOfClicksRequired = 2;
    [v addGestureRecognizer:rec];
    retain_target(rec);
}

void aether_ui_animate_opacity_impl(int handle, double target, int duration_ms) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (!v) return;
    [NSAnimationContext runAnimationGroup:^(NSAnimationContext* ctx) {
        ctx.duration = (double)duration_ms / 1000.0;
        [[v animator] setAlphaValue:target];
    } completionHandler:nil];
}

// ---------------------------------------------------------------------------
// Widget manipulation — remove / clear children.
// ---------------------------------------------------------------------------
void aether_ui_remove_child_impl(int parent_handle, int child_handle) {
    NSView* parent = (__bridge NSView*)aether_ui_get_widget(parent_handle);
    NSView* child = (__bridge NSView*)aether_ui_get_widget(child_handle);
    if (!parent || !child) return;
    if ([parent isKindOfClass:[NSStackView class]]) {
        [(NSStackView*)parent removeArrangedSubview:child];
    }
    [child removeFromSuperview];
}

void aether_ui_clear_children_impl(int handle) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (!v) return;
    if ([v isKindOfClass:[NSStackView class]]) {
        NSStackView* s = (NSStackView*)v;
        for (NSView* sub in [[s arrangedSubviews] copy]) {
            [s removeArrangedSubview:sub];
            [sub removeFromSuperview];
        }
    } else {
        for (NSView* sub in [[v subviews] copy]) {
            [sub removeFromSuperview];
        }
    }
}

// ---------------------------------------------------------------------------
// Widget tree
// ---------------------------------------------------------------------------

void aether_ui_widget_add_child_ctx(void* parent_ctx, int child_handle) {
    int parent_handle = (int)(intptr_t)parent_ctx;
    NSView* parent = (__bridge NSView*)aether_ui_get_widget(parent_handle);
    NSView* child = (__bridge NSView*)aether_ui_get_widget(child_handle);
    if (!parent || !child) return;

    if ([parent isKindOfClass:[NSStackView class]]) {
        NSStackView* sv = (NSStackView*)parent;
        [sv addArrangedSubview:child];

        if ([sv orientation] == NSUserInterfaceLayoutOrientationVertical) {
            // In a vertical stack, arranged subviews only take their intrinsic
            // width by default. Pin container-like children to the parent's
            // leading/trailing anchors so nested hstacks (e.g. calculator rows)
            // fill the full width — matches GTK4 box behaviour.
            int ct = get_widget_type(child_handle);
            if (ct == AUI_HSTACK || ct == AUI_VSTACK || ct == AUI_ZSTACK ||
                ct == AUI_SCROLLVIEW || ct == AUI_FORM_SECTION ||
                ct == AUI_DIVIDER || ct == AUI_PROGRESSBAR ||
                ct == AUI_TEXTAREA || ct == AUI_NAVSTACK) {
                [child.leadingAnchor constraintEqualToAnchor:sv.leadingAnchor].active = YES;
                [child.trailingAnchor constraintEqualToAnchor:sv.trailingAnchor].active = YES;
            }
        } else {
            // Horizontal stack: constrain all button children to equal width.
            // NSStackViewDistributionFill with multiple low-hugging siblings
            // lets autolayout give the slack to one child; an explicit
            // width-equality chain forces grid-like button rows (calculator)
            // without affecting label+textfield or button+spacer rows.
            if ([child isKindOfClass:[NSButton class]]) {
                for (NSView* sib in [sv arrangedSubviews]) {
                    if (sib == child) break;
                    if ([sib isKindOfClass:[NSButton class]]) {
                        [child.widthAnchor constraintEqualToAnchor:sib.widthAnchor].active = YES;
                        break;
                    }
                }
            }
        }
    } else if ([parent isKindOfClass:[NSScrollView class]]) {
        [(NSScrollView*)parent setDocumentView:child];
    } else if ([parent isKindOfClass:[NSBox class]]) {
        // shouldn't happen — section exposes its inner stack via handle+1
        [(NSBox*)parent setContentView:child];
    } else {
        [parent addSubview:child];
        // For zstack-style containers: pin child to parent bounds
        [child setTranslatesAutoresizingMaskIntoConstraints:NO];
        int t = get_widget_type(parent_handle);
        if (t == AUI_ZSTACK || t == AUI_NAVSTACK) {
            [child.leadingAnchor constraintEqualToAnchor:parent.leadingAnchor].active = YES;
            [child.trailingAnchor constraintEqualToAnchor:parent.trailingAnchor].active = YES;
            [child.topAnchor constraintEqualToAnchor:parent.topAnchor].active = YES;
            [child.bottomAnchor constraintEqualToAnchor:parent.bottomAnchor].active = YES;
        }
    }
}

void aether_ui_widget_set_hidden(int handle, int hidden) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (v) [v setHidden:hidden != 0];
}

// ---------------------------------------------------------------------------
// AetherUIDriver — HTTP test server
//
// Runs an HTTP server on a background pthread. Widget actions are marshalled
// back to the main thread via dispatch_sync(dispatch_get_main_queue(), ...).
// Endpoint surface matches aether_ui_gtk4.c so test_automation.sh is portable.
// ---------------------------------------------------------------------------

static int* sealed_widgets = NULL;
static int sealed_count = 0;
static int sealed_capacity = 0;
static int banner_handle = 0;
static int ts_port = 0;

void aether_ui_seal_widget_impl(int handle) {
    if (sealed_count >= sealed_capacity) {
        sealed_capacity = sealed_capacity == 0 ? 32 : sealed_capacity * 2;
        sealed_widgets = realloc(sealed_widgets, sizeof(int) * sealed_capacity);
    }
    sealed_widgets[sealed_count++] = handle;
}

static void seal_subtree_recursive(NSView* v) {
    if (!v) return;
    int h = handle_for_view(v);
    if (h > 0) aether_ui_seal_widget_impl(h);
    for (NSView* child in [v subviews]) {
        seal_subtree_recursive(child);
    }
}

void aether_ui_seal_subtree_impl(int handle) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (v) seal_subtree_recursive(v);
}

static int is_widget_sealed(int handle) {
    for (int i = 0; i < sealed_count; i++) {
        if (sealed_widgets[i] == handle) return 1;
    }
    return 0;
}

static const char* widget_type_name_for(int handle) {
    int t = get_widget_type(handle);
    switch (t) {
        case AUI_TEXT: return "text";
        case AUI_BUTTON: return "button";
        case AUI_TOGGLE: return "toggle";
        case AUI_SLIDER: return "slider";
        case AUI_PICKER: return "picker";
        case AUI_TEXTFIELD: return "textfield";
        case AUI_SECUREFIELD: return "securefield";
        case AUI_TEXTAREA: return "textarea";
        case AUI_TEXTAREA_INNER: return "textarea_inner";
        case AUI_PROGRESSBAR: return "progressbar";
        case AUI_DIVIDER: return "divider";
        case AUI_SCROLLVIEW: return "scrollview";
        case AUI_VSTACK: return "vstack";
        case AUI_HSTACK: return "hstack";
        case AUI_ZSTACK: return "zstack";
        case AUI_SPACER: return "spacer";
        case AUI_CANVAS: return "canvas";
        case AUI_IMAGE: return "image";
        case AUI_FORM_SECTION: return "form_section";
        case AUI_FORM_SECTION_INNER: return "form_section_inner";
        case AUI_NAVSTACK: return "navstack";
        case AUI_BANNER: return "banner";
        default: return "widget";
    }
}

static const char* widget_text_for(int handle) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (!v) return "";
    if ([v isKindOfClass:[NSButton class]]) {
        return [[(NSButton*)v title] UTF8String];
    }
    if ([v isKindOfClass:[NSTextField class]]) {
        return [[(NSTextField*)v stringValue] UTF8String];
    }
    if ([v isKindOfClass:[NSTextView class]]) {
        return [[(NSTextView*)v string] UTF8String];
    }
    return "";
}

// Escape a string for embedding in JSON.
static void json_escape(const char* in, char* out, int outsize) {
    int o = 0;
    for (int i = 0; in && in[i] && o < outsize - 2; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c == '"' || c == '\\') {
            if (o < outsize - 3) { out[o++] = '\\'; out[o++] = c; }
        } else if (c < 0x20) {
            if (o < outsize - 7) o += snprintf(out + o, outsize - o, "\\u%04x", c);
        } else {
            out[o++] = c;
        }
    }
    out[o] = '\0';
}

static int widget_to_json(int handle, char* buf, int bufsize) {
    NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
    if (!v) {
        return snprintf(buf, bufsize, "{\"id\":%d,\"type\":\"null\"}", handle);
    }
    const char* type = widget_type_name_for(handle);
    const char* text = widget_text_for(handle);
    char escaped[512];
    json_escape(text, escaped, sizeof(escaped));
    int visible = ![v isHidden];
    int sealed = is_widget_sealed(handle) ? 1 : 0;
    int is_banner = (handle == banner_handle) ? 1 : 0;
    int parent_id = handle_for_view([v superview]);

    int n = snprintf(buf, bufsize,
        "{\"id\":%d,\"type\":\"%s\",\"text\":\"%s\",\"visible\":%s,\"sealed\":%s,\"banner\":%s,\"parent\":%d",
        handle, type, escaped,
        visible ? "true" : "false",
        sealed ? "true" : "false",
        is_banner ? "true" : "false",
        parent_id);

    if (get_widget_type(handle) == AUI_TOGGLE && [v isKindOfClass:[NSButton class]]) {
        int active = [(NSButton*)v state] == NSControlStateValueOn ? 1 : 0;
        n += snprintf(buf + n, bufsize - n, ",\"active\":%s", active ? "true" : "false");
    } else if ([v isKindOfClass:[NSSlider class]]) {
        n += snprintf(buf + n, bufsize - n, ",\"value\":%.2f", [(NSSlider*)v doubleValue]);
    } else if ([v isKindOfClass:[NSProgressIndicator class]]) {
        n += snprintf(buf + n, bufsize - n, ",\"value\":%.2f", [(NSProgressIndicator*)v doubleValue]);
    }
    n += snprintf(buf + n, bufsize - n, "}");
    return n;
}

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

static const char* extract_query_param(const char* path, const char* key) {
    char needle[64];
    snprintf(needle, sizeof(needle), "%s=", key);
    const char* p = strstr(path, needle);
    if (!p) return NULL;
    return p + strlen(needle);
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

// Perform a widget action on the main thread and return a result code.
// action: 0=click, 1=set_text, 2=toggle, 3=set_value, 4=set_state
// returns: 0=ok, 1=sealed, 2=banner, 3=not_found
static int perform_action(int action, int handle, double dval, const char* sval) {
    __block int result = 3;
    dispatch_sync(dispatch_get_main_queue(), ^{
        if (action == 4) {
            aether_ui_state_set(handle, dval);
            result = 0;
            return;
        }
        NSView* v = (__bridge NSView*)aether_ui_get_widget(handle);
        if (!v) { result = 3; return; }
        if (handle == banner_handle) { result = 2; return; }
        if (is_widget_sealed(handle)) { result = 1; return; }

        switch (action) {
            case 0: // click
                if ([v isKindOfClass:[NSButton class]]) {
                    [(NSButton*)v performClick:nil];
                }
                break;
            case 1: // set_text
                if ([v isKindOfClass:[NSTextField class]]) {
                    [(NSTextField*)v setStringValue:[NSString stringWithUTF8String:sval ? sval : ""]];
                } else if ([v isKindOfClass:[NSTextView class]]) {
                    [(NSTextView*)v setString:[NSString stringWithUTF8String:sval ? sval : ""]];
                }
                break;
            case 2: // toggle
                if ([v isKindOfClass:[NSButton class]]) {
                    NSButton* b = (NSButton*)v;
                    NSControlStateValue cur = [b state];
                    [b setState:cur == NSControlStateValueOn ? NSControlStateValueOff : NSControlStateValueOn];
                    [b sendAction:[b action] to:[b target]];
                }
                break;
            case 3: // set_value
                if ([v isKindOfClass:[NSSlider class]]) {
                    [(NSSlider*)v setDoubleValue:dval];
                    [(NSSlider*)v sendAction:[(NSSlider*)v action] to:[(NSSlider*)v target]];
                } else if ([v isKindOfClass:[NSProgressIndicator class]]) {
                    [(NSProgressIndicator*)v setDoubleValue:dval];
                }
                break;
        }
        result = 0;
    });
    return result;
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

    // GET /widgets with optional ?type=&text= filters
    if (method == 0 && strncmp(path, "/widgets", 8) == 0 &&
        (path[8] == '\0' || path[8] == '?')) {
        const char* filter_type = extract_query_param(path, "type");
        const char* filter_text = extract_query_param(path, "text");
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
            const char* type = widget_type_name_for(i);
            if (ft_buf[0] && strcmp(type, ft_buf) != 0) continue;
            if (fx_buf[0]) {
                const char* t = widget_text_for(i);
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

    // GET /widget/{id}/children
    if (method == 0 && strncmp(path, "/widget/", 8) == 0) {
        char* suffix = strchr(path + 8, '/');
        if (suffix && strcmp(suffix, "/children") == 0) {
            int id = atoi(path + 8);
            NSView* v = (__bridge NSView*)aether_ui_get_widget(id);
            if (!v) {
                send_response(client_fd, 404, "Not Found", "application/json",
                              "{\"error\":\"widget not found\"}");
                close(client_fd);
                return;
            }
            char* body = malloc(widget_count * 512 + 64);
            int pos = 0;
            int first = 1;
            pos += sprintf(body + pos, "[");
            NSArray* subs = nil;
            if ([v isKindOfClass:[NSStackView class]]) {
                subs = [(NSStackView*)v arrangedSubviews];
            } else {
                subs = [v subviews];
            }
            for (NSView* c in subs) {
                int ch = handle_for_view(c);
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

    // GET /screenshot — capture the primary window as PNG
    if (method == 0 && strcmp(path, "/screenshot") == 0) {
        __block NSData* png = nil;
        dispatch_sync(dispatch_get_main_queue(), ^{
            NSWindow* win = primary_window;
            if (!win) return;
            NSView* v = [win contentView];
            if (!v) return;
            NSRect bounds = [v bounds];
            NSBitmapImageRep* rep = [v bitmapImageRepForCachingDisplayInRect:bounds];
            if (!rep) return;
            [v cacheDisplayInRect:bounds toBitmapImageRep:rep];
            png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
        });

        if (png && [png length] > 0) {
            char header[256];
            int hlen = snprintf(header, sizeof(header),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: image/png\r\n"
                "Content-Length: %lu\r\n"
                "Connection: close\r\n\r\n",
                (unsigned long)[png length]);
            write(client_fd, header, hlen);
            write(client_fd, [png bytes], [png length]);
        } else {
            send_response(client_fd, 500, "Error", "application/json",
                          "{\"error\":\"screenshot failed\"}");
        }
        close(client_fd);
        return;
    }

    // GET /widget/{id}
    if (method == 0 && strncmp(path, "/widget/", 8) == 0 && strchr(path + 8, '/') == NULL) {
        int id = atoi(path + 8);
        if (id < 1 || id > widget_count) {
            send_response(client_fd, 404, "Not Found", "application/json",
                          "{\"error\":\"widget not found\"}");
        } else {
            char buf[1024];
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

    // POST /widget/{id}/<action>
    if (method == 1 && strncmp(path, "/widget/", 8) == 0) {
        char* action_part = strchr(path + 8, '/');
        if (action_part) {
            int handle = atoi(path + 8);
            int action = -1;
            double dval = 0.0;
            char sval[512] = "";

            if (strncmp(action_part, "/click", 6) == 0) {
                action = 0;
            } else if (strncmp(action_part, "/set_text", 9) == 0) {
                action = 1;
                const char* v = extract_query_param(path, "v");
                if (v) {
                    strncpy(sval, v, sizeof(sval) - 1);
                    char* amp = strchr(sval, '&'); if (amp) *amp = '\0';
                }
            } else if (strncmp(action_part, "/toggle", 7) == 0) {
                action = 2;
            } else if (strncmp(action_part, "/set_value", 10) == 0) {
                action = 3;
                const char* v = extract_query_param(path, "v");
                if (v) dval = atof(v);
            } else {
                send_response(client_fd, 400, "Bad Request", "application/json",
                              "{\"error\":\"unknown action\"}");
                close(client_fd);
                return;
            }

            int result = perform_action(action, handle, dval, sval);
            if (result == 1) {
                send_response(client_fd, 403, "Forbidden", "application/json",
                              "{\"error\":\"widget is sealed\"}");
            } else if (result == 2) {
                send_response(client_fd, 403, "Forbidden", "application/json",
                              "{\"error\":\"banner cannot be manipulated\"}");
            } else if (result == 3) {
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
            int handle = atoi(path + 7);
            double dval = 0.0;
            const char* v = extract_query_param(path, "v");
            if (v) dval = atof(v);
            perform_action(4, handle, dval, NULL);
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

// Inject "Under Remote Control" banner as the first arranged subview.
static void inject_remote_control_banner(int root_handle) {
    NSView* root = (__bridge NSView*)aether_ui_get_widget(root_handle);
    if (!root || ![root isKindOfClass:[NSStackView class]]) return;

    NSTextField* banner = [NSTextField labelWithString:@"Under Remote Control"];
    [banner setEditable:NO];
    [banner setBordered:NO];
    [banner setSelectable:NO];
    [banner setTextColor:[NSColor whiteColor]];
    [banner setFont:[NSFont boldSystemFontOfSize:12]];
    [banner setAlignment:NSTextAlignmentCenter];
    [banner setWantsLayer:YES];
    banner.layer.backgroundColor = [[NSColor colorWithRed:0.8 green:0.2 blue:0.2 alpha:1.0] CGColor];
    [banner setTranslatesAutoresizingMaskIntoConstraints:NO];
    [banner.heightAnchor constraintEqualToConstant:24].active = YES;

    banner_handle = register_widget_typed((__bridge void*)banner, AUI_BANNER);
    [(NSStackView*)root insertArrangedSubview:banner atIndex:0];
    [banner.leadingAnchor constraintEqualToAnchor:root.leadingAnchor].active = YES;
    [banner.trailingAnchor constraintEqualToAnchor:root.trailingAnchor].active = YES;
}

void aether_ui_enable_test_server_impl(int port, int root_handle) {
    ts_port = port;

    // The app delegate hasn't been run yet — the root may not be in a window.
    // We can inject the banner now since it's inside the stack view.
    inject_remote_control_banner(root_handle);

    pthread_t tid;
    pthread_create(&tid, NULL, test_server_thread, (void*)(intptr_t)port);
    pthread_detach(tid);
}

#endif // __APPLE__
