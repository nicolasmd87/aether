# Testing Aether UI apps with AetherUIDriver

Aether UI includes a built-in HTTP test server — **AetherUIDriver** — that
lets you write automated end-to-end tests for your GUI application from
any language that can speak HTTP. You don't need Selenium, Appium, or a
separate test harness. Start your app with one env var, poke it with
`curl`, and assert on the JSON. The same test suite runs on macOS, Linux,
and Windows.

This is aether_ui's strongest differentiator. Most native UI frameworks
force you to bolt on a third-party automation layer. AetherUIDriver is
part of the framework.

## Quick start

Build and launch any aether_ui app with `AETHER_UI_TEST_PORT=<port>`:

```bash
./contrib/aether_ui/build.sh contrib/aether_ui/example_calculator.ae build/calc
AETHER_UI_TEST_PORT=9222 ./build/calc &

# Wait for the server to come up (a few hundred ms), then drive it:
curl http://127.0.0.1:9222/widgets
```

Response (abridged):

```json
[
  {"id":1,"type":"vstack","text":"","visible":true,"sealed":false,"banner":false,"parent":0},
  {"id":2,"type":"text","text":"Under Remote Control","visible":true,"sealed":true,"banner":true,"parent":1},
  {"id":3,"type":"text","text":"0","visible":true,"sealed":false,"banner":false,"parent":1},
  {"id":4,"type":"button","text":"7","visible":true,"sealed":false,"banner":false,"parent":5},
  ...
]
```

A red bold **"Under Remote Control"** banner is automatically injected at
the top of the window to make sure a user can't mistake a test-driven
session for a real one.

## HTTP API

| Method | Path                               | Body (query string) | Returns                       |
|--------|------------------------------------|---------------------|-------------------------------|
| GET    | `/widgets`                         |                     | JSON array of every widget    |
| GET    | `/widgets?type=<type>`             |                     | filter by widget type         |
| GET    | `/widgets?text=<text>`             |                     | filter by text (URL-encoded)  |
| GET    | `/widgets?type=<t>&text=<x>`       |                     | both filters AND-ed           |
| GET    | `/widget/{id}`                     |                     | state of a single widget      |
| GET    | `/widget/{id}/children`            |                     | direct children (array)       |
| GET    | `/screenshot`                      |                     | PNG bytes of the root window  |
| POST   | `/widget/{id}/click`               |                     | simulate a button click       |
| POST   | `/widget/{id}/set_text?v=<text>`   |                     | set text / textfield value    |
| POST   | `/widget/{id}/toggle`              |                     | flip a checkbox / toggle      |
| POST   | `/widget/{id}/set_value?v=<float>` |                     | set a slider or progress-bar  |
| GET    | `/state/{id}`                      |                     | read a reactive state cell    |
| POST   | `/state/{id}/set?v=<float>`        |                     | write a reactive state cell   |

Responses are plain `200 OK` with a short body on success, `403` when
the widget is sealed or is the banner, `404` when the handle doesn't
exist, `400` for a malformed request.

## Widget types

The `type` field identifies the widget kind, backend-independent:

`text`, `button`, `textfield`, `securefield`, `textarea`, `toggle`,
`slider`, `picker`, `progressbar`, `image`, `vstack`, `hstack`, `zstack`,
`form`, `form_section`, `navstack`, `scrollview`, `spacer`, `divider`,
`canvas`, `window`, `sheet`.

Type-specific extra fields in `/widget/{id}` responses:

- `toggle`: `"active": true | false`
- `slider`: `"value": <number>`
- `progressbar`: `"value": <number>` (0.0 – 1.0)

## Sealing widgets — marking them non-automatable

Your app can declare certain widgets off-limits for the test API. The
test server returns `403 Forbidden` for any mutation attempt against a
sealed widget. This is the "hide and seal" pattern: the app author
decides which capabilities the test harness is denied, not the other
way around.

```aether
danger = aether_ui.button("Delete Everything") callback { ... }
aether_ui.seal_widget(danger)
```

A sealed widget's JSON includes `"sealed": true` so tests can detect
the protected state rather than blindly retrying.

`aether_ui.seal_subtree(handle)` seals every descendant of a container
in one call — useful for admin-only sections.

## Writing a test suite

### Bash + curl (works anywhere — Linux, macOS, Git Bash on Windows)

```bash
#!/bin/bash
set -u

PORT=9222
AETHER_UI_HEADLESS=1 AETHER_UI_TEST_PORT="$PORT" ./build/counter &
APP_PID=$!
trap 'kill "$APP_PID" 2>/dev/null' EXIT

# Wait for the server
for _ in $(seq 1 50); do
    curl -sf -o /dev/null "http://127.0.0.1:$PORT/widgets" && break
    sleep 0.1
done

# Helper
call() { curl -sf --max-time 5 "$@"; }

# Find the "+1" button
BTN=$(call "http://127.0.0.1:$PORT/widgets?type=button&text=%2B1" \
      | grep -o '"id":[0-9]*' | head -1 | cut -d: -f2)

# Click it twice and read the counter state
call -X POST "http://127.0.0.1:$PORT/widget/$BTN/click"
call -X POST "http://127.0.0.1:$PORT/widget/$BTN/click"

VAL=$(call "http://127.0.0.1:$PORT/state/1")
[ "$VAL" = "2.00" ] && echo "PASS: counter went from 0 to 2" || echo "FAIL: got $VAL"
```

### Python

```python
import requests, time, subprocess

proc = subprocess.Popen(["./build/counter"],
                        env={**os.environ,
                             "AETHER_UI_TEST_PORT": "9222",
                             "AETHER_UI_HEADLESS": "1"})
try:
    # Wait for the server
    for _ in range(50):
        try:
            requests.get("http://127.0.0.1:9222/widgets", timeout=0.2).raise_for_status()
            break
        except Exception:
            time.sleep(0.1)

    widgets = requests.get("http://127.0.0.1:9222/widgets").json()
    plus_btn = next(w for w in widgets
                    if w["type"] == "button" and w["text"] == "+1")

    for _ in range(3):
        requests.post(f"http://127.0.0.1:9222/widget/{plus_btn['id']}/click")

    val = float(requests.get("http://127.0.0.1:9222/state/1").text)
    assert val == 3.0, f"expected 3, got {val}"
    print("PASS")
finally:
    proc.terminate()
```

### JavaScript (fetch, no dependencies)

```js
const base = "http://127.0.0.1:9222";

const widgets = await (await fetch(`${base}/widgets`)).json();
const plus = widgets.find(w => w.type === "button" && w.text === "+1");

for (let i = 0; i < 5; i++) {
    await fetch(`${base}/widget/${plus.id}/click`, {method: "POST"});
}

const val = parseFloat(await (await fetch(`${base}/state/1`)).text());
console.log(val === 5.0 ? "PASS" : `FAIL: got ${val}`);
```

## `AETHER_UI_HEADLESS=1` — running without a visible window

Set `AETHER_UI_HEADLESS=1` on any backend to create the window without
ever presenting it to the desktop. The widget tree is fully realized,
the event loop still pumps, and the test server still responds — it's
just that the user sees nothing. Required for:

- GitHub Actions `windows-latest` and equivalent unattended runners
- Docker containers without a virtual display
- Parallel test runs that would otherwise clutter the desktop

On Linux/macOS this skips the final `gtk_window_present` / `[window
makeKeyAndOrderFront:]`. On Windows it uses `ShowWindow(hwnd, SW_HIDE)`.

## `make contrib-aether-ui-check` — the reference test suite

The in-tree suite at `contrib/aether_ui/tests/` is the reference
implementation:

- `test_widgets.c` — 40 C-level smoke assertions covering every widget
  kind, roundtrips, unicode (Japanese, Cyrillic, emoji, French
  accents), 500-widget stress, 30-level deep nesting
- `test_driver.sh` — 14 HTTP integration assertions against a live
  driver (widget discovery, filters, click/toggle/set_value
  round-trips, sealed-banner 403 enforcement)
- `bench_widgets.c` — CSV microbenchmarks

Run them via:

```bash
make contrib-aether-ui-check   # builds backend + runs the whole suite
make benchmark-aether-ui       # CSV of widget create / layout / paint / state
```

Wired into `make ci` as step [10/10] so every CI run on every platform
verifies the backend is healthy.

## Architecture — why the driver is cross-platform

The HTTP server, URL routing, JSON emission, sealed-widget registry,
and cross-platform socket code live once in
[`contrib/aether_ui/aether_ui_test_server.c`](../contrib/aether_ui/aether_ui_test_server.c).
Each backend implements a small set of hooks (see
[`aether_ui_test_server.h`](../contrib/aether_ui/aether_ui_test_server.h)):

- Introspection: `widget_count`, `widget_type`, `widget_text_into`,
  `widget_visible`, `widget_parent`, `toggle_active`, `slider_value`,
  `progressbar_fraction`
- Mutation: `dispatch_action` — marshals a widget mutation onto the UI
  thread. GTK4 uses `g_idle_add`, AppKit uses
  `dispatch_async(dispatch_get_main_queue(), ...)`, Win32 uses
  `SendMessageW` to a hidden `AE_WM_DRIVER` window.

Adding a new endpoint means editing one file. Adding a new widget kind
means registering its handlers in one place per backend.

## Known limitations

- **Mutation is synchronous** — `dispatch_action` blocks the HTTP
  thread until the UI thread finishes the action. Long-running
  callbacks inside a button's on-click will block the HTTP response
  until they return. For most tests this is desirable (it ensures the
  action's side effects are visible before the next HTTP call); for
  deliberately long-running handlers, split them with a timer.
- **No test-harness authentication.** The server binds only to
  `127.0.0.1` (loopback), so it's unreachable from the network, but
  any local process on the machine can drive the app while the server
  is up. Don't enable `AETHER_UI_TEST_PORT` in production builds.
- **Screenshots require a visible-or-DWM-composed window.** The Win32
  backend captures with `BitBlt` from the window's DC, then encodes
  via GDI+. Works even in `AETHER_UI_HEADLESS=1` mode on modern
  Windows because DWM composes hidden windows, but expect blank
  captures on unsupported setups (Windows Server Core without GUI).

## Also see

- [`docs/aether-ui.md`](aether-ui.md) — main framework guide *(planned)*
- [`docs/aether-ui-windows.md`](aether-ui-windows.md) — Windows backend notes
- [`contrib/aether_ui/README.md`](../contrib/aether_ui/README.md) — project overview
