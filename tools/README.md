# Aether REPL

Interactive Read-Eval-Print Loop for the Aether programming language.

## Features

- **Immediate evaluation** - Execute expressions and see results instantly
- **Multiline mode** - Define complex functions, actors, and structs
- **Session persistence** - All definitions remain available throughout the session
- **Command history** - Navigate previous commands (Unix/Linux with readline)
- **Color output** - Syntax-highlighted prompts and error messages
- **Auto-detection** - Automatically enters multiline mode for incomplete statements

## Building

### Windows
```powershell
.\tools\build_repl.ps1
```

### Linux/macOS
```bash
chmod +x tools/build_repl.sh
./tools/build_repl.sh
```

Note: On Linux/macOS, you need the `readline` library:
- Ubuntu/Debian: `sudo apt-get install libreadline-dev`
- macOS: `brew install readline`

## Usage

Start the REPL:
```bash
./aether-repl        # Linux/macOS
.\aether-repl.exe    # Windows
```

## Commands

| Command | Shortcut | Description |
|---------|----------|-------------|
| `:help` | `:h` | Show help message |
| `:quit` | `:q` | Exit the REPL |
| `:clear` | `:c` | Clear the screen |
| `:multi` | `:m` | Enter multiline mode |
| `:reset` | `:r` | Reset the session |
| `:show` | `:s` | Show current session code |

## Examples

### Simple Expressions
```aether
>>> 2 + 3
5

>>> 42 * 2
84

>>> "Hello, " + "Aether!"
Hello, Aether!
```

### Variables
```aether
>>> int x = 10
>>> int y = 20
>>> x + y
30
```

### Functions
```aether
>>> :multi
... func add(int a, int b): int {
...     return a + b
... }
...
OK

>>> add(5, 7)
12
```

### Actors (Multiline)
```aether
>>> actor Counter {
...     state count: int
...     
...     func init() {
...         count = 0
...     }
...     
...     receive {
...         "increment" -> {
...             count = count + 1
...             print(count)
...         }
...     }
... }
...
OK

>>> counter = spawn Counter()
>>> send counter, "increment"
1
```

### Collections
```aether
>>> import std.collections.HashMap

>>> map = HashMap.new()
>>> map.insert("key1", 100)
>>> map.get("key1")
100
```

## Tips

1. **Multiline Mode**: Press Enter on an empty line to execute multiline code
2. **Quick Test**: Use the REPL to quickly test type inference and expressions
3. **Debugging**: Use `:show` to see all code defined in the current session
4. **Reset**: Use `:reset` if something goes wrong

## Implementation Details

The REPL works by:
1. Taking user input
2. Wrapping expressions in a temporary `main()` function
3. Writing to a temporary `.ae` file
4. Compiling with `aetherc`
5. Running the compiled executable
6. Displaying the output

This means:
- Full Aether language support (whatever the compiler supports)
- Real compilation and execution (not interpreted)
- Same performance as compiled programs
- Proper error messages from the compiler

## Limitations

- Each line/block is compiled independently (slight overhead)
- Previous definitions are not carried between compilations (use `:multi` for related definitions)
- Requires `aetherc` compiler in PATH

## Future Enhancements

- [ ] Persistent session (save/load)
- [ ] Syntax highlighting in input
- [ ] Auto-completion
- [ ] Integration with LSP for better suggestions
- [ ] Better error recovery
- [ ] Incremental compilation mode
