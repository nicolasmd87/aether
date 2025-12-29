# Contributing to Aether

Thank you for your interest in contributing to Aether! This document provides guidelines for contributing to the project.

## Code of Conduct

By participating in this project, you agree to abide by our [Code of Conduct](CODE_OF_CONDUCT.md). Please read it before contributing.

## Getting Started

### Prerequisites

- **GCC compiler** (MinGW on Windows)
- **Git** for version control
- **Make** (optional, can use build scripts)
- Basic knowledge of C and compilers (helpful but not required)

### Setting Up Development Environment

1. **Fork and clone the repository:**
```bash
git clone https://github.com/YOUR_USERNAME/aether.git
cd aether
```

2. **Build the compiler:**
```bash
# Linux/macOS
make

# Windows
.\build_compiler.ps1
```

3. **Run tests to verify:**
```bash
make test
# or
.\test_all_examples.ps1
```

## Development Workflow

### Before You Start

1. **Check existing issues** - Someone might already be working on it
2. **Open an issue** - Discuss major changes before implementing
3. **Create a branch** - Use descriptive names: `feature/add-pattern-matching` or `fix/parser-infinite-loop`

### Making Changes

1. **Write clean code:**
   - Follow the code style guide (below)
   - Add comments for complex logic
   - Keep functions focused and small

2. **Add tests:**
   - Add test cases for new features in `tests/`
   - Add example programs in `examples/`
   - Ensure all tests pass

3. **Update documentation:**
   - Update relevant docs in `docs/`
   - Add docstrings to new functions
   - Update README if needed

4. **Commit messages:**
   - Use clear, descriptive commit messages
   - Format: `type: brief description`
   - Types: `feat`, `fix`, `docs`, `test`, `refactor`, `perf`, `chore`
   - Example: `feat: add pattern matching for match statements`

### Submitting Changes

1. **Push your branch:**
```bash
git push origin feature/your-feature-name
```

2. **Create a Pull Request:**
   - Use the PR template
   - Link related issues
   - Describe your changes clearly
   - Add screenshots if applicable

3. **Code Review:**
   - Respond to feedback promptly
   - Make requested changes
   - Be open to suggestions

## Code Style

### C Code

- **Indentation:** 4 spaces (no tabs)
- **Function names:** `snake_case`
- **Type names:** `PascalCase`
- **Constants:** `UPPER_SNAKE_CASE`
- **Macros:** `UPPER_SNAKE_CASE`
- **Brace style:** K&R (opening brace on same line)

```c
// Good
int calculate_sum(int a, int b) {
    return a + b;
}

// Bad
int CalculateSum(int a, int b)
{
  return a+b;
}
```

### Aether Code

- Clean, readable syntax
- Meaningful variable names
- Comments for complex logic
- Follow examples in `examples/`

### Comments

```c
// Good: Explains WHY
// Use binary search because the array is sorted
int result = binary_search(arr, target);

// Bad: Explains WHAT (obvious from code)
// Call binary search function
int result = binary_search(arr, target);
```

## Project Structure

```
aether/
├── compiler/           # Compiler source code
│   ├── lexer.c/.h     # Tokenization
│   ├── parser.c/.h    # AST generation
│   ├── typechecker.c  # Type checking
│   ├── codegen.c/.h   # C code generation
│   └── ...
├── runtime/           # Runtime libraries
│   ├── aether_string.c/.h
│   ├── aether_math.c/.h
│   ├── multicore_scheduler.c/.h
│   └── ...
├── examples/          # Example Aether programs
├── tests/            # Test suite (C tests)
├── docs/             # Documentation
├── lsp/              # Language server
└── editor/           # Editor integrations
```

## Areas to Contribute

### For Beginners

- **Documentation:** Fix typos, improve clarity, add examples
- **Examples:** Create new example programs
- **Tests:** Add test cases for edge cases
- **Bug reports:** File detailed bug reports with reproducible examples

### Intermediate

- **Bug fixes:** Fix reported bugs
- **Error messages:** Improve error reporting
- **Standard library:** Add new stdlib functions
- **Tooling:** Improve build scripts, testing

### Advanced

- **Compiler features:** New language features
- **Optimizations:** Performance improvements
- **Runtime:** Actor system enhancements
- **LSP:** Language server implementation

## Testing

### Running Tests

```bash
# Run all tests
make test

# Run specific example
aether run examples/basic/hello_world.ae

# Test on multiple platforms (CI will do this)
```

### Writing Tests

1. **Unit tests** go in `tests/test_*.c`
2. **Integration tests** are `.ae` files in `examples/tests/`
3. **Add to test suite** in `test_all_examples.ps1`

Example test:
```c
TEST(example_feature) {
    // Arrange
    int input = 42;
    
    // Act
    int result = my_function(input);
    
    // Assert
    ASSERT_EQ(result, expected_value);
}
```

## Documentation

### What to Document

- **New features:** Add to `docs/language-reference.md`
- **API changes:** Update `docs/runtime-guide.md`
- **Tutorials:** Add to `docs/tutorial.md`
- **Code:** Add docstrings to functions

### Documentation Style

- Clear and concise
- Include code examples
- Explain the "why" not just the "what"
- Keep it up-to-date with code changes

## Issue Labels

- `bug` - Something isn't working
- `enhancement` - New feature or request
- `documentation` - Documentation improvements
- `good first issue` - Good for newcomers
- `help wanted` - Extra attention needed
- `question` - Further information requested

## Release Process

(For maintainers)

1. Update version in `compiler/aetherc.c`
2. Update CHANGELOG.md
3. Create git tag: `git tag v0.1.0`
4. Push tag: `git push origin v0.1.0`
5. GitHub Actions will create release

## Getting Help

- **Questions:** Open an issue with the `question` label
- **Discussions:** Use GitHub Discussions
- **Discord:** [Coming soon]
- **Email:** [Coming soon]

## License

By contributing, you agree that your contributions will be licensed under the MIT License.

## Recognition

Contributors will be recognized in:
- GitHub contributors page
- CONTRIBUTORS.md file (planned)
- Release notes for significant contributions

Thank you for contributing to Aether! 🚀
