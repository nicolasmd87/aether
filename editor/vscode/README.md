# Aether Language Support

Official Visual Studio Code extension for the [Aether programming language](https://github.com/aether-lang/aether).

## Features

- **Syntax Highlighting** - Full syntax highlighting for Aether source files (`.ae`)
- **Erlang-Inspired Color Theme** - Custom color theme designed specifically for Aether's actor-based syntax
- **File Icons** - Distinctive yellow "ae" icons for Aether source files
- **Language Configuration** - Auto-closing brackets, comments, and intelligent indentation

## Aether Language

Aether is a modern programming language featuring:

- **Actor-based Concurrency** - Built-in support for concurrent programming with the actor model
- **Type Inference** - Smart type inference that reduces boilerplate while maintaining type safety
- **Pattern Matching** - Powerful pattern matching for elegant control flow
- **Memory Safety** - Arena-based memory management for predictable performance

## Syntax Highlighting

The extension provides comprehensive syntax highlighting with Erlang-inspired colors:

- **Control Keywords** (`if`, `else`, `for`, `while`, `return`) - Red
- **Actor Keywords** (`receive`, `send`, `spawn`, `actor`) - Red
- **Function Names** - Red (except `main()` which is yellow)
- **Declarations** (`message`, `actor`, `struct`, `state`) - Purple
- **Strings** - Dark green
- **Numbers & Constants** - Yellow
- **Comments** - Gray-green (italic)
- **Types** - Cyan

## Getting Started

1. Install this extension
2. Open or create an `.ae` file
3. Select the **"Aether Erlang"** color theme (Ctrl+K Ctrl+T)
4. Start coding!

## Example Code

```aether
struct Point {
    x,
    y
}

distance(p1, p2) {
    dx = p2.x - p1.x
    dy = p2.y - p1.y
    return dx * dx + dy * dy
}

main() {
    p1 = Point { x: 0, y: 0 }
    p2 = Point { x: 3, y: 4 }
    d = distance(p1, p2)
    print(d)  // 25
}
```

## Requirements

- Visual Studio Code 1.60.0 or higher

## Installation

Install from VSIX:
1. Download the latest `.vsix` file
2. Open VS Code
3. Go to Extensions (Ctrl+Shift+X)
4. Click the "..." menu → "Install from VSIX..."
5. Select the downloaded file

## Contribution

Found a bug or have a feature request? Please open an issue on the [GitHub repository](https://github.com/aether-lang/aether).

## License

This extension is licensed under the MIT License. See the [LICENSE](../../LICENSE) file for details.

## Links

- [Aether GitHub Repository](https://github.com/aether-lang/aether)
- [Language Documentation](../../docs/language-reference.md)
- [Getting Started Guide](../../docs/getting-started.md)

---

**Enjoy coding with Aether!** 

