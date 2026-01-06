# Tests

Cross-platform test suite for Aether runtime.

## Running Tests

```bash
# PowerShell (all platforms)
./run_tests.ps1

# Bash (Unix/Linux/macOS)
./run_tests.sh
```

## Test Organization

```
tests/
├── compiler/      # Compiler tests
├── runtime/       # Runtime tests
├── integration/   # Integration tests
├── memory/        # Memory tests
└── stdlib/        # Standard library tests
```

## Requirements

- GCC or Clang
- AVX2 support (optional, auto-detected)

## Test Organization

### Unit Tests
- `test_lexer.c` - Lexical analyzer tests
- `test_parser.c` - Parser tests
- `test_typechecker.c` - Type checker tests
- `test_codegen.c` - Code generator tests
- `test_structs.c` - Struct parsing and type checking tests

### Integration Tests
- `test_integration.c` - End-to-end compilation pipeline tests
- `test_examples.c` - Example program compilation and execution tests

### Test Infrastructure
- `test_harness.h` - Test framework header
- `test_harness.c` - Test framework implementation
- `test_main.c` - Test runner entry point

## Running Tests

### Prerequisites
- GCC compiler must be in PATH for integration tests
- Aether compiler must be built at `build/aetherc.exe`

### Build and Run
```bash
# Build test runner
gcc tests/test_*.c compiler/lexer.c compiler/parser.c compiler/ast.c compiler/typechecker.c compiler/codegen.c -Icompiler -o build/test_runner.exe

# Run all tests
./build/test_runner.exe
```

## Test Framework

The test suite uses a simple custom test framework with automatic test registration.

### Writing Tests

```c
#include "test_harness.h"

TEST(test_name) {
    ASSERT_TRUE(condition);
    ASSERT_FALSE(condition);
    ASSERT_EQ(expected, actual);
    ASSERT_NE(not_expected, actual);
    ASSERT_STREQ(expected_str, actual_str);
    ASSERT_STRNE(not_expected_str, actual_str);
    ASSERT_NULL(ptr);
    ASSERT_NOT_NULL(ptr);
}
```

Tests are automatically registered using GCC constructor attributes and executed in registration order.

## Platform Support

Tests are designed to be cross-platform and use preprocessor directives to handle differences between:
- Windows (`_WIN32`)
- Unix-like systems (Linux, macOS)

Path separators and system commands are adjusted automatically based on the platform.

## Known Limitations

### Integration Tests
Integration tests that compile generated C code require `gcc` to be available in the system PATH. If `gcc` is not in PATH, these tests will fail with "command not found" errors.

To run only unit tests without integration tests, exclude the integration test files:
```bash
gcc tests/test_lexer.c tests/test_parser.c tests/test_typechecker.c tests/test_codegen.c tests/test_structs.c tests/test_harness.c tests/test_main.c compiler/lexer.c compiler/parser.c compiler/ast.c compiler/typechecker.c compiler/codegen.c -Icompiler -o build/test_runner_unit.exe
```

