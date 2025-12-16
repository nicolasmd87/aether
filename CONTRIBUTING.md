# Contributing to Aether

Thank you for your interest in contributing to Aether!

## Development Setup

### Prerequisites

- GCC compiler
- Git
- Make (optional)

### Building

```bash
git clone <repository-url>
cd aether
make
```

### Running Tests

```bash
make test
```

## Code Style

- Use 4 spaces for indentation
- Function names: `snake_case`
- Type names: `PascalCase`
- Constants: `UPPER_SNAKE_CASE`
- Comments: Explain *why*, not *what*

## Project Structure

```
aether/
├── compiler/      # Compiler source
├── runtime/       # Runtime libraries
├── examples/      # Example programs
├── tests/         # Test suite
├── docs/          # Documentation
└── experiments/   # Performance experiments
```

## Making Changes

1. Create a feature branch
2. Make your changes
3. Add tests if applicable
4. Update documentation
5. Submit a pull request

## Documentation

- User-facing docs go in `docs/`
- Code comments explain implementation details
- Examples should be working and tested

## Questions?

Open an issue or start a discussion.
