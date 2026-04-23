# Aether UI on Windows

The Windows backend for `contrib/aether_ui` is a native Win32
implementation in [`contrib/aether_ui/aether_ui_win32.c`](../contrib/aether_ui/aether_ui_win32.c).
It implements the same ABI as the GTK4 (Linux) and AppKit (macOS) backends
— declared in [`aether_ui_backend.h`](../contrib/aether_ui/aether_ui_backend.h) —
but is driven entirely by USER32, GDI+, Common Controls, Windows Shell,
DWM, and Winsock2. No third-party runtime is shipped.

## Why native Win32 (not MSYS2-GTK, WebView2, or XAML)

- **Zero runtime dependencies.** USER32, GDI+, COMCTL32, COMDLG32, DWMAPI
  are present on every Windows 10 installation. Binaries are standalone
  `.exe` files — no MSYS2-GTK's 200 MB runtime, no Edge WebView2
  runtime, no MSIX packaging.
- **House style.** Aether's networking (`std/net/*`) already uses
  `winsock2`, and `runtime/utils/aether_thread.h` exposes Win32
  thread/mutex/condvar shims under `#ifdef _WIN32`. A Win32 UI backend
  matches this convention.
- **Retained-mode widget mapping.** Win32's `HWND` tree maps 1:1 to the
  handle-based widget registry already used by the GTK4 and AppKit
  backends. XAML/WinUI would require a C++/WinRT COM surface; WebView2
  would force an HTML retrofit onto a retained-mode DSL.
- **MinGW-friendly.** The backend compiles cleanly under MinGW-w64 GCC —
  the same toolchain the rest of Aether uses on Windows. No MSVC
  dependency.

## Prerequisites

Build from an MSYS2 **MINGW64** shell (not MSYS or UCRT) with:

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-make make bc
```

No other dev libraries are needed — everything resolves against system
DLLs at link time via `-lws2_32 -luser32 -lgdi32 -lgdiplus -lcomctl32
-lcomdlg32 -lshell32 -lole32 -luuid -ldwmapi -luxtheme`.

Minimum Windows version: **10 1903** (per-monitor DPI v2 + immersive
dark mode).

## Build and run

```bash
./contrib/aether_ui/build.sh contrib/aether_ui/example_counter.ae build/counter
./build/counter.exe
```

`build.sh` detects `uname -s` → `MINGW*|MSYS*|CYGWIN*` and calls GCC with
the Win32 link set.

## DPI awareness

- At startup, the backend calls
  `SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)`
  (or falls back to `SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE)`
  on pre-1703 systems).
- `aether_ui_app_create(title, w, h)` treats `w`/`h` as logical
  points; on creation they are scaled by `GetDpiForSystem() / 96.0`,
  then `AdjustWindowRectExForDpi` sizes the non-client area so the
  client area matches the requested logical size.
- Dragging the window between monitors with different DPI triggers
  `WM_DPICHANGED`; the backend accepts the suggested RECT and relays out
  children via the stack custom window class.
- Font sizes set via `aether_ui.set_font_size(pt)` are converted from
  points to logical units using the device's `LOGPIXELSY`.

## Dark mode

The backend reads
`HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize\AppsUseLightTheme`
at window creation. When the user is in dark mode the backend calls
`DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, TRUE)`
which gives the title bar the Windows 11 dark theme.

`aether_ui.dark_mode_check()` returns `1` when the system is in dark
mode and `0` otherwise — use it to pick darker widget backgrounds at
runtime.

## Widget internals

### Layout containers

VStack / HStack / ZStack are implemented by a **custom window class**
(`AetherUIStack`) whose `WM_SIZE` handler walks direct children, measures
each one, and calls `SetWindowPos` to lay them out with the configured
spacing, padding, and alignment. Spacers are zero-size placeholders that
consume leftover flex space along the primary axis. Nested stacks recurse
via the normal Win32 child-window propagation.

### Input widgets

`EDIT`, `BUTTON`, `COMBOBOX`, `TRACKBAR_CLASSW`, `PROGRESS_CLASSW` —
Windows system controls. Change notifications are dispatched through the
parent stack's `WM_COMMAND` / `WM_HSCROLL` / `WM_VSCROLL` handlers,
which look up the originating child's handle and invoke its registered
Aether closure.

### Canvas

The canvas widget records draw calls (`move_to`, `line_to`, `stroke`,
`fill_rect`, `clear`) into a command buffer. On `WM_PAINT` the buffer is
replayed onto an offscreen `HBITMAP` via GDI (pens + solid brushes)
then `BitBlt`ed to the screen. This keeps every paint flicker-free.

### Reactive state

Ported verbatim from the GTK4/AppKit backends — `state_create`,
`state_get`, `state_set`, `state_bind_text` are pure bookkeeping + a
`SetWindowTextW` on each bound text widget. No platform specifics.

### AetherUIDriver (test server)

The Win32 backend uses the **shared** test server in
[`aether_ui_test_server.c`](../contrib/aether_ui/aether_ui_test_server.c)
(same binary as the Linux/macOS drivers, single source of truth for HTTP
parsing / URL routing / JSON emission). The backend provides a small
`AetherDriverHooks` struct that answers widget queries on the server
thread and marshals mutations onto the UI thread via `SendMessageW` to a
hidden `AE_WM_DRIVER` window. Winsock2 (`WSAStartup` / `closesocket`) is
used instead of POSIX sockets; the rest is identical cross-platform.

See [`docs/aether-ui-testing.md`](aether-ui-testing.md) for the HTTP API
reference and test-suite examples.

### Keyboard navigation

The message loop wraps dispatch with `IsDialogMessageW`, which routes
Tab / Shift+Tab between WS_TABSTOP controls, activates the default
button on Enter, and cancels on Esc. No additional wiring needed — set
WS_TABSTOP at widget creation (already done for buttons, textfields,
toggles, sliders, pickers, textareas) and focus traversal works like a
native dialog.

### Image loading

`aether_ui_image_create(path)` supports PNG / JPEG / GIF / BMP / TIFF /
ICO via GDI+ `GdipCreateBitmapFromFile`. Transparent backgrounds are
preserved. Falls back to `LoadImageW` for plain BMPs on systems where
GDI+ initialization failed.

## UTF-8 ↔ UTF-16

Aether strings are UTF-8; Win32 APIs are UTF-16 (`wchar_t`). The backend
uses a pair of helpers — `utf8_to_wide` / `wide_to_utf8` — with static
rotating buffers so callers don't need to free results. The buffers hold
up to 4 KB per slot across 8 slots, which comfortably covers every label,
tooltip, and textfield the widget API can produce.

CJK, RTL, emoji, and combining-mark text round-trip cleanly through the
`textfield_get_text` / `textarea_get_text` APIs.
Tests in `contrib/aether_ui/tests/test_widgets.c` exercise Japanese, emoji,
French accents, and Cyrillic paths.

## Clipboard

`aether_ui.clipboard_write(text)` opens the clipboard, empties it, and
writes the wide string as `CF_UNICODETEXT` — never `CF_TEXT`. Reading
back from other apps (including Notepad, browsers, Office) preserves
Unicode exactly.

## Animation

`aether_ui.animate_opacity(widget, target, duration_ms)` uses
`WS_EX_LAYERED` on top-level windows with `SetLayeredWindowAttributes`
stepped from a `SetTimer` (NULL HWND) firing every 16 ms. For child
windows (which cannot be layered), the call is a no-op — composite
backends are planned but not yet implemented; see "Known limitations"
below.

## System services

| Aether call | Win32 implementation |
|-------------|----------------------|
| `alert(title, msg)` | `MessageBoxW(hwnd, msg, title, MB_OK)` |
| `file_open()` | `GetOpenFileNameW` with OFN_EXPLORER |
| `open_url(url)` | `ShellExecuteW(NULL, L"open", url, …)` |
| `clipboard_write(text)` | `OpenClipboard` + `CF_UNICODETEXT` |
| `timer_create(ms, cb)` | `SetTimer(host, id, ms, timer_cb)` |
| `dark_mode_check()` | HKCU `AppsUseLightTheme` query |

## Running the test suite

```bash
make contrib-aether-ui-check      # 40 widget tests + 14 HTTP driver tests
make benchmark-aether-ui          # CSV microbenchmarks
```

The `make ci` target runs `contrib-aether-ui-check` as step [10/10] on
every platform, so Windows CI (`windows.yml`) will exercise the Win32
backend on every PR.

## Headless mode for CI

Set `AETHER_UI_HEADLESS=1` before launching an app to suppress window
visibility. The window is still created and the message loop still pumps
(so the AetherUIDriver HTTP server keeps responding), but `SW_HIDE` is
used instead of `SW_SHOW` — nothing ever renders to the desktop.

Used automatically by `tests/test_driver.sh` so `make contrib-aether-ui-check`
works safely on `windows-latest` GitHub Actions runners and any other
unattended Windows environment.

## Known limitations

- **Child-window opacity.** `WS_EX_LAYERED` reliably applies only to
  top-level windows. `animate_opacity` on an internal widget is a no-op
  until a compositing paint path lands.
- **Canvas uses GDI, not GDI+.** Paths and gradients work through GDI
  pens/brushes; the richer GDI+ path API is wired but only for solid
  colors so far. Anti-aliased curves and gradient fills are planned.
- **High-contrast mode.** Detected via
  `SystemParametersInfoW(SPI_GETHIGHCONTRAST)` but custom theming is
  not yet suppressed.
- **IME composition.** WM_IME_COMPOSITION goes through the default EDIT
  handler, which works for simple CJK input but doesn't expose
  composition state to Aether closures.

## File layout

- [`contrib/aether_ui/aether_ui_backend.h`](../contrib/aether_ui/aether_ui_backend.h) — shared ABI
- [`contrib/aether_ui/aether_ui_win32.c`](../contrib/aether_ui/aether_ui_win32.c) — Windows backend
- [`contrib/aether_ui/build.sh`](../contrib/aether_ui/build.sh) — detects `MINGW*|MSYS*|CYGWIN*` and links the Win32 lib set
- [`contrib/aether_ui/tests/test_widgets.c`](../contrib/aether_ui/tests/test_widgets.c) — cross-platform C-level smoke suite
- [`contrib/aether_ui/tests/test_driver.sh`](../contrib/aether_ui/tests/test_driver.sh) — HTTP integration test
- [`contrib/aether_ui/benchmarks/bench_widgets.c`](../contrib/aether_ui/benchmarks/bench_widgets.c) — microbenchmarks
