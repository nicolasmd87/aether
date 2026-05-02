# Aether Language Support

Visual Studio Code (and Cursor) support for the
[Aether programming language](https://github.com/nicolasmd87/aether).

## What you get

- **Syntax highlighting** for `.ae` files, scoped specifically for actor
  definitions, message types, struct types, pattern arrows (`->`),
  cons-cell patterns (`[h | t]`), and the actor send / ask operators
  (`!`, `?`).
- **Color theme** tuned for Aether's dispatch-heavy style. Message
  names render warm amber so they read as the compile-time tags they
  are, actor names cyan, control keywords magenta-italic, and pattern
  arrows / cons pipes coral-bold so the shape of a `receive` block
  pops at a distance.
- **File icon** — a yellow-on-dark "ae" badge for `.ae` files.
- **Language configuration** — auto-closing brackets, comment toggle,
  smart indent.
- **Language Server (LSP) auto-wired** — when you open a `.ae` file the
  extension finds your `aether-lsp` binary and starts a language
  client against it. No manual configuration: it tries
  `aether.lsp.path`, then the workspace's `build/aether-lsp`, then
  `PATH`, then common install dirs (`~/.local/bin`, `~/.aether/bin`,
  `/usr/local/bin`, `/opt/homebrew/bin`). Set
  `aether.lsp.enable: false` for syntax-only mode.

## Activating the theme

Themes are global in VS Code, not per-language, so you have to opt in:

1. Open a `.ae` file (loads the extension).
2. Command palette → **`Preferences: Color Theme`** → **Aether**.

The syntax grammar runs regardless of the active theme — every theme
gets at least the standard scope colors. The Aether theme adds the
finer-grained palette described above.

## Installation

### From source

```bash
./editor/vscode/install.sh
```

The script:

- Reads the version straight from `package.json` so the installed
  folder name tracks the manifest.
- Removes any prior `aether-language-*` directory before copying so
  stale assets from older releases can't shadow the new ones.
- Copies the full asset set (manifest, grammar, language config,
  theme, icon-theme, both icon files, README) into
  `~/.cursor/extensions/` (Cursor) or `~/.vscode/extensions/`
  (VS Code).
- Supports an explicit override target:
  `./install.sh /path/to/extensions`.

Restart your editor after installing.

### From a `.vsix`

Standard `vsce package` workflow if you've installed `vsce` (a
follow-up will publish official releases to the marketplace; for
now the install script is the supported path).

## Language Server Protocol

The extension auto-starts an LSP client on any `.ae` file open. The
language server is now embedded in `aetherc` itself (issue #327, the
`aetherc lsp` subcommand) so a single toolchain binary is enough —
the standalone `aether-lsp` is kept as a transitional alias for
editor configs that hardcode the older name.

The resolver tries each location in order, preferring `aetherc lsp`
over `aether-lsp` when both are present:

1. The `aether.lsp.path` setting if you've set it.
2. `<workspace>/build/aetherc lsp` (or `aether-lsp` as fallback) —
   the common case for working in the Aether repo itself; just
   `make compiler` and the extension finds it.
3. `aetherc lsp` (then `aether-lsp`) resolved through your shell
   `PATH`.
4. Common install directories: `~/.local/bin`, `~/.aether/bin`,
   `/usr/local/bin`, `/opt/homebrew/bin`.

If none of those find an executable, you'll see a one-time warning
with a link to the setting; the extension stays in syntax-only mode
until you provide a path or build the server. See
[`lsp/README.md`](../../lsp/README.md) for what the server currently
supports.

## Building the extension client from source (maintainers)

The bundled `out/extension.js` is committed so end users don't need
node. If you change `src/extension.ts`, regenerate it with:

```bash
cd editor/vscode
npm install
npm run build      # esbuild bundle to out/extension.js
npm run typecheck  # tsc --noEmit, sanity check
```

## Example

```aether
import std.string

message Tick { count: int }

actor Heartbeat {
    state ticks = 0

    receive {
        Tick(count) -> {
            ticks = ticks + 1
            if ticks % 10 == 0 {
                println("heartbeat ${ticks}")
            }
        }
    }
}

main() {
    h = spawn(Heartbeat())
    i = 0
    while i < 100 {
        h ! Tick { count: i }
        i = i + 1
    }
}
```

## Requirements

- Visual Studio Code 1.60.0+ (or Cursor on the same protocol level)

## Reporting issues

Open an issue at
[github.com/nicolasmd87/aether/issues](https://github.com/nicolasmd87/aether/issues).

## License

MIT — see the [LICENSE](../../LICENSE) at the repo root.
