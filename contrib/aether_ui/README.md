# Aether UI for Aether

Port of the [Perry](https://github.com/PerryTS/perry) UI framework to Aether.
Declarative widget DSL backed by GTK4 (Linux), AppKit (macOS), and a native
Win32 backend (Windows) — all three implementations share the same ABI
declared in `aether_ui_backend.h`. Uses Aether's trailing-block builder
pattern.

## Credits

This module is a from-scratch Aether + C rewrite of the aether-ui Rust crates
from the [Perry project](https://github.com/PerryTS/perry) by the Perry
contributors. The Rust implementations (`aether-ui-gtk4`, `aether-ui-macos`, and
the core `aether-ui` crate) were used as reference for architecture, widget
API design, reactive state bindings, and platform-specific GTK4/AppKit
patterns. Based on commit
[`7f1e3f9`](https://github.com/PerryTS/perry/commit/7f1e3f979832c33d2da79970ea62bc1b74c2e31a)
of the `main` branch.

Portions Copyright (c) 2026 Perry Contributors, and portions Copyright (c) 2026 Aether Contributors. MIT License.

## Quick start

### Linux (GTK4)

```bash
sudo apt install libgtk-4-dev   # Debian/Ubuntu
./contrib/aether_ui/build.sh contrib/aether_ui/example_counter.ae build/counter
./build/counter
```

### macOS (AppKit)

```bash
./contrib/aether_ui/build.sh contrib/aether_ui/example_counter.ae build/counter
./build/counter
```

### Windows (native Win32)

Build from an MSYS2 MinGW64 shell (no extra dev libraries — USER32, GDI+
and Common Controls ship with Windows itself):

```bash
./contrib/aether_ui/build.sh contrib/aether_ui/example_counter.ae build/counter
./build/counter.exe
```

Backend-level smoke tests (headless, no display needed) for any platform:

```bash
make contrib-aether-ui-check    # widget + driver tests for current backend
make benchmark-aether-ui        # microbenchmarks, CSV to stdout
```

See [docs/aether-ui-windows.md](../../docs/aether-ui-windows.md) for
Windows-specific details (DPI model, dark mode, widget mappings, known
limitations).

## How it works

Aether UI maps to Aether's builder DSL (same pattern as TinyWeb):

```aether
import contrib.aether_ui

main() {
    counter = aether_ui.ui_state(0)

    root = aether_ui.root_vstack(10) {
        aether_ui.text("Hello World")
        aether_ui.text_bound(counter, "Count: ", "")
        aether_ui.hstack(5) {
            aether_ui.button("+1") callback {
                aether_ui.ui_set(counter, aether_ui.ui_get(counter) + 1)
            }
            aether_ui.button("-1") callback {
                aether_ui.ui_set(counter, aether_ui.ui_get(counter) - 1)
            }
        }
    }

    aether_ui.app_run("My App", 400, 200, root)
}
```

## Widgets available

| Widget      | Aether function                                       | GTK4               | AppKit                  | Win32                      |
|-------------|-------------------------------------------------------|--------------------|-------------------------|----------------------------|
| Text        | `aether_ui.text("label")`                             | GtkLabel           | NSTextField (label)     | STATIC                     |
| Button      | `aether_ui.button("label") callback { }`              | GtkButton          | NSButton                | BUTTON (BS_PUSHBUTTON)     |
| VStack      | `aether_ui.vstack(spacing) { children }`              | GtkBox vertical    | NSStackView vertical    | AetherUIStack (custom)     |
| HStack      | `aether_ui.hstack(spacing) { children }`              | GtkBox horizontal  | NSStackView horizontal  | AetherUIStack (custom)     |
| Spacer      | `aether_ui.spacer()`                                  | Expanding GtkBox   | NSView flex filler      | flex placeholder           |
| Divider     | `aether_ui.divider()`                                 | GtkSeparator       | NSBox separator         | GDI line (custom class)    |
| TextField   | `aether_ui.textfield("hint") callback \|val\| { }`    | GtkEntry           | NSTextField             | EDIT                       |
| SecureField | `aether_ui.securefield("hint") callback \|val\| { }`  | GtkPasswordEntry   | NSSecureTextField       | EDIT (ES_PASSWORD)         |
| Toggle      | `aether_ui.toggle("label") callback \|active\| { }`   | GtkCheckButton     | NSButton (switch)       | BUTTON (BS_AUTOCHECKBOX)   |
| Slider      | `aether_ui.slider(min, max, init) callback \|val\|`   | GtkScale           | NSSlider                | TRACKBAR (comctl32)        |
| Picker      | `aether_ui.picker() callback \|idx\| { }`             | GtkDropDown        | NSPopUpButton           | COMBOBOX (CBS_DROPDOWNLIST)|
| TextArea    | `aether_ui.textarea("hint") callback \|val\| { }`     | GtkTextView        | NSTextView              | EDIT (ES_MULTILINE)        |
| ProgressBar | `aether_ui.progressbar(0.75)`                         | GtkProgressBar     | NSProgressIndicator     | PROGRESS (comctl32)        |
| ScrollView  | `aether_ui.scrollview() { children }`                 | GtkScrolledWindow  | NSScrollView            | AetherUIStack + WS_VSCROLL |

## Reactive state

```aether
counter = aether_ui.ui_state(0)              // create state cell
aether_ui.text_bound(counter, "Val: ", "")   // auto-updating text
aether_ui.ui_set(counter, 42)                // triggers re-render
val = aether_ui.ui_get(counter)              // read current value
```

## Widget accessors

```aether
aether_ui.set_text(handle, "new text")       // set textfield value
text = aether_ui.get_text(handle)            // get textfield value
aether_ui.set_toggle(handle, 1)              // set toggle on/off
aether_ui.set_slider(handle, 75.0)           // set slider position
aether_ui.set_progress(handle, 0.5)          // set progress bar
```

## Examples

| Example | Widgets demonstrated |
|---------|---------------------|
| `example_counter.ae` | text, button, hstack, vstack, spacer, divider, reactive state |
| `example_form.ae` | textfield, securefield, toggle, slider, textarea, progressbar |
| `example_picker.ae` | picker (dropdown), picker_add |
| `example_styled.ae` | form, section, zstack, bg_color, bg_gradient, font_size, corner_radius |
| `example_system.ae` | alert, clipboard, dark mode detection, sheet |
| `example_canvas.ae` | canvas drawing, fill_rect, stroke, on_hover, on_double_click |
| `example_testable.ae` | AetherUIDriver test server, sealed widgets, remote control banner |

## AetherUIDriver — automated UI testing, baked in

Aether UI ships with a built-in HTTP test server that lets any language
with an HTTP client drive the app:

```aether
aether_ui.enable_test_server(9222, root)
```

Or set `AETHER_UI_TEST_PORT=9222` in the environment before launching —
no code changes needed. A red "Under Remote Control" banner is injected
so a user can't mistake a test-driven session for a real one.

The HTTP API exposes `/widgets` (list + filter), `/widget/{id}` (state),
`/widget/{id}/click | set_text | toggle | set_value` (mutations), and
`/state/{id}` + `/state/{id}/set` (reactive-state cells). See the full
reference and end-to-end examples in
**[docs/aether-ui-testing.md](../../docs/aether-ui-testing.md)** —
including Bash, Python, and JavaScript test-suite skeletons plus the
`AETHER_UI_HEADLESS=1` flag for unattended CI.

For most native UI frameworks you have to bolt on Selenium/Appium. With
aether_ui it's part of the framework and works identically on macOS,
Linux, and Windows via the shared
[`aether_ui_test_server.c`](aether_ui_test_server.c).

### Widget sealing

Mark widgets as non-automatable — the test server returns 403 for sealed widgets:

```aether
danger = aether_ui.button("Delete Everything") callback { ... }
aether_ui.seal_widget(danger)
```

This maps to Aether's `hide`/`seal` philosophy: the app author declares which
capabilities the test harness is denied, not the other way around.

## Architecture

| Layer | File | Role |
|-------|------|------|
| Aether DSL | `module.ae` | Builder-pattern wrappers with `_ctx` auto-injection |
| GTK4 backend | `aether_ui_gtk4.c` | Linux: GTK4 C API calls, Cairo canvas, test server |
| macOS backend | `aether_ui_macos.m` | macOS: AppKit Objective-C |
| Win32 backend | `aether_ui_win32.c` | Windows: USER32 + GDI+ + Common Controls |
| C header | `aether_ui_backend.h` | Shared backend ABI — implemented by all three backends |
| Build script | `build.sh` | Auto-detects platform (Darwin/Linux/MinGW) |
| Test script | `test_automation.sh` | Example curl-based test suite (17 assertions) |
| Widget tests | `tests/test_widgets.c` | Cross-platform C-level smoke suite (40 assertions) |
| Driver tests | `tests/test_driver.sh` | HTTP integration against the embedded test server |
| Benchmarks | `benchmarks/bench_widgets.c` | CSV microbenchmarks — widget create, layout, state, canvas |

## Platform support

| Platform | Backend                         | Status                                                                             |
|----------|---------------------------------|------------------------------------------------------------------------------------|
| Linux    | GTK4  (`aether_ui_gtk4.c`)      | Full — all widgets, canvas, events, styling, AetherUIDriver test server            |
| macOS    | AppKit (`aether_ui_macos.m`)    | Full — all widgets, canvas, events, styling, AetherUIDriver test server            |
| Windows  | Native Win32 (`aether_ui_win32.c`) | Full — USER32 + GDI+ + Common Controls; per-monitor DPI v2; immersive dark mode; AetherUIDriver via winsock2 |

## Status

All groups (1–7) plus AetherUIDriver are implemented on every backend.
`make contrib-aether-ui-check` runs the cross-platform smoke suite and, on
Windows, the HTTP driver integration. `make benchmark-aether-ui` prints a
CSV of per-operation latencies.
