# Aether UI for Aether

Port of the [Perry](https://github.com/PerryTS/perry) UI framework to Aether.
Declarative widget DSL backed by GTK4 (Linux) and AppKit (macOS), using
Aether's trailing-block builder pattern.

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

```bash
# Prerequisites: GTK4 dev libraries
sudo apt install libgtk-4-dev   # Debian/Ubuntu

# Build and run examples
./contrib/aether_ui/build.sh contrib/aether_ui/example_counter.ae build/perry_counter
./build/perry_counter

./contrib/aether_ui/build.sh contrib/aether_ui/example_form.ae build/perry_form
./build/perry_form
```

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

| Widget | Aether function | GTK4 mapping |
|--------|----------------|--------------|
| Text | `aether_ui.text("label")` | GtkLabel |
| Button | `aether_ui.button("label") callback { }` | GtkButton |
| VStack | `aether_ui.vstack(spacing) { children }` | GtkBox vertical |
| HStack | `aether_ui.hstack(spacing) { children }` | GtkBox horizontal |
| Spacer | `aether_ui.spacer()` | Expanding GtkBox |
| Divider | `aether_ui.divider()` | GtkSeparator |
| TextField | `aether_ui.textfield("hint") callback \|val\| { }` | GtkEntry |
| SecureField | `aether_ui.securefield("hint") callback \|val\| { }` | GtkPasswordEntry |
| Toggle | `aether_ui.toggle("label") callback \|active\| { }` | GtkCheckButton |
| Slider | `aether_ui.slider(min, max, init) callback \|val\| { }` | GtkScale |
| Picker | `aether_ui.picker() callback \|idx\| { }` | GtkDropDown |
| TextArea | `aether_ui.textarea("hint") callback \|val\| { }` | GtkTextView in ScrolledWindow |
| ProgressBar | `aether_ui.progressbar(0.75)` | GtkProgressBar |
| ScrollView | `aether_ui.scrollview() { children }` | GtkScrolledWindow |

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

## AetherUIDriver — AetherUIDriver

Aether UI includes a Selenium-like AetherUIDriver test server. Enable it in your app:

```aether
aether_ui.enable_test_server(9222, root)
```

This injects an irremovable "Under Remote Control" banner and starts an HTTP
server. Drive the app with curl, any HTTP client, or the included test script:

```bash
./build/perry_testable &
./contrib/aether_ui/test_automation.sh
kill %1
```

### API endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/widgets` | List all widgets as JSON |
| GET | `/widget/{id}` | Widget state (type, text, value, visible, sealed) |
| POST | `/widget/{id}/click` | Simulate button click |
| POST | `/widget/{id}/set_text?v=X` | Set text/textfield value |
| POST | `/widget/{id}/toggle` | Toggle a checkbox |
| POST | `/widget/{id}/set_value?v=X` | Set slider/progress value |
| GET | `/state/{id}` | Get reactive state value |
| POST | `/state/{id}/set?v=X` | Set reactive state value |

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
| C header | `aether_ui_gtk4.h` | Shared API surface for both backends |
| Build script | `build.sh` | Auto-detects platform (Darwin/Linux) |
| Test script | `test_automation.sh` | Example curl-based test suite (17 assertions) |

## Platform support

| Platform | Backend | Status |
|----------|---------|--------|
| Linux | GTK4 (`aether_ui_gtk4.c`) | Full — all widgets, canvas, events, styling, test server |
| macOS | AppKit (`aether_ui_macos.m`) | Full — all widgets, canvas, events, styling, test server |

## Status

All groups (1-7) plus AetherUIDriver are implemented.
