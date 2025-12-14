# Aether Language Test Suite

This directory contains the test infrastructure for the Aether language compiler and runtime.

## Test Framework

The test framework is a lightweight C test harness that provides:

- **Assertion Macros**: `ASSERT_TRUE`, `ASSERT_FALSE`, `ASSERT_EQ`, `ASSERT_NE`, `ASSERT_STREQ`, `ASSERT_STRNE`, `ASSERT_NULL`, `ASSERT_NOT_NULL`
- **Automatic Test Registration**: Tests are registered automatically using constructor attributes
- **Test Runner**: Automatically discovers and runs all registered tests

## Test Structure

### Test Harness (`test_harness.h` / `test_harness.c`)
Core test framework with assertion macros and test runner.

### Example Tests (`test_examples.c`)
Tests that compile and run example `.ae` files from the `examples/` directory.

### Integration Tests (`test_integration.c`)
Tests for the full compilation and execution pipeline, including:
- Loop compilation and execution
- Runtime shutdown behavior
- End-to-end workflows

### Unit Tests
- `test_lexer.c` - Lexer component tests
- `test_parser.c` - Parser component tests
- `test_codegen.c` - Code generator tests
- `test_typechecker.c` - Type checker tests

## Running Tests

### Run All Tests
```bash
make test
```

### Run Specific Test Categories
```bash
make test-examples    # Run example-based tests
make test-unit        # Run unit tests
make test-integration # Run integration tests
```

## Writing Tests

To write a new test, use the `TEST` macro:

```c
#include "test_harness.h"

TEST(my_test_name) {
    int x = 5;
    int y = 10;
    
    ASSERT_EQ(5, x);
    ASSERT_NE(x, y);
    ASSERT_TRUE(x < y);
}
```

The test will be automatically registered and run when you execute `make test`.

## Test Output

Tests output progress as they run:
```
[1/10] Running test: simple_for_loop
  ✓ PASSED
[2/10] Running test: test_for_loop
  ✓ PASSED
...

========================================
Test Results: 10 passed, 0 failed, 10 total
========================================
```

## Debugging Failed Tests

If a test fails, the assertion macros will print:
- The file and line number where the assertion failed
- The condition or values that failed
- The test will exit with code 1

## Notes

- Tests that compile and run example files require the compiler (`aetherc.exe`) to be built first
- Integration tests create temporary files in the `build/` directory
- Some unit tests are placeholders and need implementation when test APIs are added to compiler components


This directory contains the test infrastructure for the Aether language compiler and runtime.

## Test Framework

The test framework is a lightweight C test harness that provides:

- **Assertion Macros**: `ASSERT_TRUE`, `ASSERT_FALSE`, `ASSERT_EQ`, `ASSERT_NE`, `ASSERT_STREQ`, `ASSERT_STRNE`, `ASSERT_NULL`, `ASSERT_NOT_NULL`
- **Automatic Test Registration**: Tests are registered automatically using constructor attributes
- **Test Runner**: Automatically discovers and runs all registered tests

## Test Structure

### Test Harness (`test_harness.h` / `test_harness.c`)
Core test framework with assertion macros and test runner.

### Example Tests (`test_examples.c`)
Tests that compile and run example `.ae` files from the `examples/` directory.

### Integration Tests (`test_integration.c`)
Tests for the full compilation and execution pipeline, including:
- Loop compilation and execution
- Runtime shutdown behavior
- End-to-end workflows

### Unit Tests
- `test_lexer.c` - Lexer component tests
- `test_parser.c` - Parser component tests
- `test_codegen.c` - Code generator tests
- `test_typechecker.c` - Type checker tests

## Running Tests

### Run All Tests
```bash
make test
```

### Run Specific Test Categories
```bash
make test-examples    # Run example-based tests
make test-unit        # Run unit tests
make test-integration # Run integration tests
```

## Writing Tests

To write a new test, use the `TEST` macro:

```c
#include "test_harness.h"

TEST(my_test_name) {
    int x = 5;
    int y = 10;
    
    ASSERT_EQ(5, x);
    ASSERT_NE(x, y);
    ASSERT_TRUE(x < y);
}
```

The test will be automatically registered and run when you execute `make test`.

## Test Output

Tests output progress as they run:
```
[1/10] Running test: simple_for_loop
  ✓ PASSED
[2/10] Running test: test_for_loop
  ✓ PASSED
...

========================================
Test Results: 10 passed, 0 failed, 10 total
========================================
```

## Debugging Failed Tests

If a test fails, the assertion macros will print:
- The file and line number where the assertion failed
- The condition or values that failed
- The test will exit with code 1

## Notes

- Tests that compile and run example files require the compiler (`aetherc.exe`) to be built first
- Integration tests create temporary files in the `build/` directory
- Some unit tests are placeholders and need implementation when test APIs are added to compiler components


This directory contains the test infrastructure for the Aether language compiler and runtime.

## Test Framework

The test framework is a lightweight C test harness that provides:

- **Assertion Macros**: `ASSERT_TRUE`, `ASSERT_FALSE`, `ASSERT_EQ`, `ASSERT_NE`, `ASSERT_STREQ`, `ASSERT_STRNE`, `ASSERT_NULL`, `ASSERT_NOT_NULL`
- **Automatic Test Registration**: Tests are registered automatically using constructor attributes
- **Test Runner**: Automatically discovers and runs all registered tests

## Test Structure

### Test Harness (`test_harness.h` / `test_harness.c`)
Core test framework with assertion macros and test runner.

### Example Tests (`test_examples.c`)
Tests that compile and run example `.ae` files from the `examples/` directory.

### Integration Tests (`test_integration.c`)
Tests for the full compilation and execution pipeline, including:
- Loop compilation and execution
- Runtime shutdown behavior
- End-to-end workflows

### Unit Tests
- `test_lexer.c` - Lexer component tests
- `test_parser.c` - Parser component tests
- `test_codegen.c` - Code generator tests
- `test_typechecker.c` - Type checker tests

## Running Tests

### Run All Tests
```bash
make test
```

### Run Specific Test Categories
```bash
make test-examples    # Run example-based tests
make test-unit        # Run unit tests
make test-integration # Run integration tests
```

## Writing Tests

To write a new test, use the `TEST` macro:

```c
#include "test_harness.h"

TEST(my_test_name) {
    int x = 5;
    int y = 10;
    
    ASSERT_EQ(5, x);
    ASSERT_NE(x, y);
    ASSERT_TRUE(x < y);
}
```

The test will be automatically registered and run when you execute `make test`.

## Test Output

Tests output progress as they run:
```
[1/10] Running test: simple_for_loop
  ✓ PASSED
[2/10] Running test: test_for_loop
  ✓ PASSED
...

========================================
Test Results: 10 passed, 0 failed, 10 total
========================================
```

## Debugging Failed Tests

If a test fails, the assertion macros will print:
- The file and line number where the assertion failed
- The condition or values that failed
- The test will exit with code 1

## Notes

- Tests that compile and run example files require the compiler (`aetherc.exe`) to be built first
- Integration tests create temporary files in the `build/` directory
- Some unit tests are placeholders and need implementation when test APIs are added to compiler components


This directory contains the test infrastructure for the Aether language compiler and runtime.

## Test Framework

The test framework is a lightweight C test harness that provides:

- **Assertion Macros**: `ASSERT_TRUE`, `ASSERT_FALSE`, `ASSERT_EQ`, `ASSERT_NE`, `ASSERT_STREQ`, `ASSERT_STRNE`, `ASSERT_NULL`, `ASSERT_NOT_NULL`
- **Automatic Test Registration**: Tests are registered automatically using constructor attributes
- **Test Runner**: Automatically discovers and runs all registered tests

## Test Structure

### Test Harness (`test_harness.h` / `test_harness.c`)
Core test framework with assertion macros and test runner.

### Example Tests (`test_examples.c`)
Tests that compile and run example `.ae` files from the `examples/` directory.

### Integration Tests (`test_integration.c`)
Tests for the full compilation and execution pipeline, including:
- Loop compilation and execution
- Runtime shutdown behavior
- End-to-end workflows

### Unit Tests
- `test_lexer.c` - Lexer component tests
- `test_parser.c` - Parser component tests
- `test_codegen.c` - Code generator tests
- `test_typechecker.c` - Type checker tests

## Running Tests

### Run All Tests
```bash
make test
```

### Run Specific Test Categories
```bash
make test-examples    # Run example-based tests
make test-unit        # Run unit tests
make test-integration # Run integration tests
```

## Writing Tests

To write a new test, use the `TEST` macro:

```c
#include "test_harness.h"

TEST(my_test_name) {
    int x = 5;
    int y = 10;
    
    ASSERT_EQ(5, x);
    ASSERT_NE(x, y);
    ASSERT_TRUE(x < y);
}
```

The test will be automatically registered and run when you execute `make test`.

## Test Output

Tests output progress as they run:
```
[1/10] Running test: simple_for_loop
  ✓ PASSED
[2/10] Running test: test_for_loop
  ✓ PASSED
...

========================================
Test Results: 10 passed, 0 failed, 10 total
========================================
```

## Debugging Failed Tests

If a test fails, the assertion macros will print:
- The file and line number where the assertion failed
- The condition or values that failed
- The test will exit with code 1

## Notes

- Tests that compile and run example files require the compiler (`aetherc.exe`) to be built first
- Integration tests create temporary files in the `build/` directory
- Some unit tests are placeholders and need implementation when test APIs are added to compiler components


This directory contains the test infrastructure for the Aether language compiler and runtime.

## Test Framework

The test framework is a lightweight C test harness that provides:

- **Assertion Macros**: `ASSERT_TRUE`, `ASSERT_FALSE`, `ASSERT_EQ`, `ASSERT_NE`, `ASSERT_STREQ`, `ASSERT_STRNE`, `ASSERT_NULL`, `ASSERT_NOT_NULL`
- **Automatic Test Registration**: Tests are registered automatically using constructor attributes
- **Test Runner**: Automatically discovers and runs all registered tests

## Test Structure

### Test Harness (`test_harness.h` / `test_harness.c`)
Core test framework with assertion macros and test runner.

### Example Tests (`test_examples.c`)
Tests that compile and run example `.ae` files from the `examples/` directory.

### Integration Tests (`test_integration.c`)
Tests for the full compilation and execution pipeline, including:
- Loop compilation and execution
- Runtime shutdown behavior
- End-to-end workflows

### Unit Tests
- `test_lexer.c` - Lexer component tests
- `test_parser.c` - Parser component tests
- `test_codegen.c` - Code generator tests
- `test_typechecker.c` - Type checker tests

## Running Tests

### Run All Tests
```bash
make test
```

### Run Specific Test Categories
```bash
make test-examples    # Run example-based tests
make test-unit        # Run unit tests
make test-integration # Run integration tests
```

## Writing Tests

To write a new test, use the `TEST` macro:

```c
#include "test_harness.h"

TEST(my_test_name) {
    int x = 5;
    int y = 10;
    
    ASSERT_EQ(5, x);
    ASSERT_NE(x, y);
    ASSERT_TRUE(x < y);
}
```

The test will be automatically registered and run when you execute `make test`.

## Test Output

Tests output progress as they run:
```
[1/10] Running test: simple_for_loop
  ✓ PASSED
[2/10] Running test: test_for_loop
  ✓ PASSED
...

========================================
Test Results: 10 passed, 0 failed, 10 total
========================================
```

## Debugging Failed Tests

If a test fails, the assertion macros will print:
- The file and line number where the assertion failed
- The condition or values that failed
- The test will exit with code 1

## Notes

- Tests that compile and run example files require the compiler (`aetherc.exe`) to be built first
- Integration tests create temporary files in the `build/` directory
- Some unit tests are placeholders and need implementation when test APIs are added to compiler components


This directory contains the test infrastructure for the Aether language compiler and runtime.

## Test Framework

The test framework is a lightweight C test harness that provides:

- **Assertion Macros**: `ASSERT_TRUE`, `ASSERT_FALSE`, `ASSERT_EQ`, `ASSERT_NE`, `ASSERT_STREQ`, `ASSERT_STRNE`, `ASSERT_NULL`, `ASSERT_NOT_NULL`
- **Automatic Test Registration**: Tests are registered automatically using constructor attributes
- **Test Runner**: Automatically discovers and runs all registered tests

## Test Structure

### Test Harness (`test_harness.h` / `test_harness.c`)
Core test framework with assertion macros and test runner.

### Example Tests (`test_examples.c`)
Tests that compile and run example `.ae` files from the `examples/` directory.

### Integration Tests (`test_integration.c`)
Tests for the full compilation and execution pipeline, including:
- Loop compilation and execution
- Runtime shutdown behavior
- End-to-end workflows

### Unit Tests
- `test_lexer.c` - Lexer component tests
- `test_parser.c` - Parser component tests
- `test_codegen.c` - Code generator tests
- `test_typechecker.c` - Type checker tests

## Running Tests

### Run All Tests
```bash
make test
```

### Run Specific Test Categories
```bash
make test-examples    # Run example-based tests
make test-unit        # Run unit tests
make test-integration # Run integration tests
```

## Writing Tests

To write a new test, use the `TEST` macro:

```c
#include "test_harness.h"

TEST(my_test_name) {
    int x = 5;
    int y = 10;
    
    ASSERT_EQ(5, x);
    ASSERT_NE(x, y);
    ASSERT_TRUE(x < y);
}
```

The test will be automatically registered and run when you execute `make test`.

## Test Output

Tests output progress as they run:
```
[1/10] Running test: simple_for_loop
  ✓ PASSED
[2/10] Running test: test_for_loop
  ✓ PASSED
...

========================================
Test Results: 10 passed, 0 failed, 10 total
========================================
```

## Debugging Failed Tests

If a test fails, the assertion macros will print:
- The file and line number where the assertion failed
- The condition or values that failed
- The test will exit with code 1

## Notes

- Tests that compile and run example files require the compiler (`aetherc.exe`) to be built first
- Integration tests create temporary files in the `build/` directory
- Some unit tests are placeholders and need implementation when test APIs are added to compiler components


This directory contains the test infrastructure for the Aether language compiler and runtime.

## Test Framework

The test framework is a lightweight C test harness that provides:

- **Assertion Macros**: `ASSERT_TRUE`, `ASSERT_FALSE`, `ASSERT_EQ`, `ASSERT_NE`, `ASSERT_STREQ`, `ASSERT_STRNE`, `ASSERT_NULL`, `ASSERT_NOT_NULL`
- **Automatic Test Registration**: Tests are registered automatically using constructor attributes
- **Test Runner**: Automatically discovers and runs all registered tests

## Test Structure

### Test Harness (`test_harness.h` / `test_harness.c`)
Core test framework with assertion macros and test runner.

### Example Tests (`test_examples.c`)
Tests that compile and run example `.ae` files from the `examples/` directory.

### Integration Tests (`test_integration.c`)
Tests for the full compilation and execution pipeline, including:
- Loop compilation and execution
- Runtime shutdown behavior
- End-to-end workflows

### Unit Tests
- `test_lexer.c` - Lexer component tests
- `test_parser.c` - Parser component tests
- `test_codegen.c` - Code generator tests
- `test_typechecker.c` - Type checker tests

## Running Tests

### Run All Tests
```bash
make test
```

### Run Specific Test Categories
```bash
make test-examples    # Run example-based tests
make test-unit        # Run unit tests
make test-integration # Run integration tests
```

## Writing Tests

To write a new test, use the `TEST` macro:

```c
#include "test_harness.h"

TEST(my_test_name) {
    int x = 5;
    int y = 10;
    
    ASSERT_EQ(5, x);
    ASSERT_NE(x, y);
    ASSERT_TRUE(x < y);
}
```

The test will be automatically registered and run when you execute `make test`.

## Test Output

Tests output progress as they run:
```
[1/10] Running test: simple_for_loop
  ✓ PASSED
[2/10] Running test: test_for_loop
  ✓ PASSED
...

========================================
Test Results: 10 passed, 0 failed, 10 total
========================================
```

## Debugging Failed Tests

If a test fails, the assertion macros will print:
- The file and line number where the assertion failed
- The condition or values that failed
- The test will exit with code 1

## Notes

- Tests that compile and run example files require the compiler (`aetherc.exe`) to be built first
- Integration tests create temporary files in the `build/` directory
- Some unit tests are placeholders and need implementation when test APIs are added to compiler components


This directory contains the test infrastructure for the Aether language compiler and runtime.

## Test Framework

The test framework is a lightweight C test harness that provides:

- **Assertion Macros**: `ASSERT_TRUE`, `ASSERT_FALSE`, `ASSERT_EQ`, `ASSERT_NE`, `ASSERT_STREQ`, `ASSERT_STRNE`, `ASSERT_NULL`, `ASSERT_NOT_NULL`
- **Automatic Test Registration**: Tests are registered automatically using constructor attributes
- **Test Runner**: Automatically discovers and runs all registered tests

## Test Structure

### Test Harness (`test_harness.h` / `test_harness.c`)
Core test framework with assertion macros and test runner.

### Example Tests (`test_examples.c`)
Tests that compile and run example `.ae` files from the `examples/` directory.

### Integration Tests (`test_integration.c`)
Tests for the full compilation and execution pipeline, including:
- Loop compilation and execution
- Runtime shutdown behavior
- End-to-end workflows

### Unit Tests
- `test_lexer.c` - Lexer component tests
- `test_parser.c` - Parser component tests
- `test_codegen.c` - Code generator tests
- `test_typechecker.c` - Type checker tests

## Running Tests

### Run All Tests
```bash
make test
```

### Run Specific Test Categories
```bash
make test-examples    # Run example-based tests
make test-unit        # Run unit tests
make test-integration # Run integration tests
```

## Writing Tests

To write a new test, use the `TEST` macro:

```c
#include "test_harness.h"

TEST(my_test_name) {
    int x = 5;
    int y = 10;
    
    ASSERT_EQ(5, x);
    ASSERT_NE(x, y);
    ASSERT_TRUE(x < y);
}
```

The test will be automatically registered and run when you execute `make test`.

## Test Output

Tests output progress as they run:
```
[1/10] Running test: simple_for_loop
  ✓ PASSED
[2/10] Running test: test_for_loop
  ✓ PASSED
...

========================================
Test Results: 10 passed, 0 failed, 10 total
========================================
```

## Debugging Failed Tests

If a test fails, the assertion macros will print:
- The file and line number where the assertion failed
- The condition or values that failed
- The test will exit with code 1

## Notes

- Tests that compile and run example files require the compiler (`aetherc.exe`) to be built first
- Integration tests create temporary files in the `build/` directory
- Some unit tests are placeholders and need implementation when test APIs are added to compiler components


This directory contains the test infrastructure for the Aether language compiler and runtime.

## Test Framework

The test framework is a lightweight C test harness that provides:

- **Assertion Macros**: `ASSERT_TRUE`, `ASSERT_FALSE`, `ASSERT_EQ`, `ASSERT_NE`, `ASSERT_STREQ`, `ASSERT_STRNE`, `ASSERT_NULL`, `ASSERT_NOT_NULL`
- **Automatic Test Registration**: Tests are registered automatically using constructor attributes
- **Test Runner**: Automatically discovers and runs all registered tests

## Test Structure

### Test Harness (`test_harness.h` / `test_harness.c`)
Core test framework with assertion macros and test runner.

### Example Tests (`test_examples.c`)
Tests that compile and run example `.ae` files from the `examples/` directory.

### Integration Tests (`test_integration.c`)
Tests for the full compilation and execution pipeline, including:
- Loop compilation and execution
- Runtime shutdown behavior
- End-to-end workflows

### Unit Tests
- `test_lexer.c` - Lexer component tests
- `test_parser.c` - Parser component tests
- `test_codegen.c` - Code generator tests
- `test_typechecker.c` - Type checker tests

## Running Tests

### Run All Tests
```bash
make test
```

### Run Specific Test Categories
```bash
make test-examples    # Run example-based tests
make test-unit        # Run unit tests
make test-integration # Run integration tests
```

## Writing Tests

To write a new test, use the `TEST` macro:

```c
#include "test_harness.h"

TEST(my_test_name) {
    int x = 5;
    int y = 10;
    
    ASSERT_EQ(5, x);
    ASSERT_NE(x, y);
    ASSERT_TRUE(x < y);
}
```

The test will be automatically registered and run when you execute `make test`.

## Test Output

Tests output progress as they run:
```
[1/10] Running test: simple_for_loop
  ✓ PASSED
[2/10] Running test: test_for_loop
  ✓ PASSED
...

========================================
Test Results: 10 passed, 0 failed, 10 total
========================================
```

## Debugging Failed Tests

If a test fails, the assertion macros will print:
- The file and line number where the assertion failed
- The condition or values that failed
- The test will exit with code 1

## Notes

- Tests that compile and run example files require the compiler (`aetherc.exe`) to be built first
- Integration tests create temporary files in the `build/` directory
- Some unit tests are placeholders and need implementation when test APIs are added to compiler components


This directory contains the test infrastructure for the Aether language compiler and runtime.

## Test Framework

The test framework is a lightweight C test harness that provides:

- **Assertion Macros**: `ASSERT_TRUE`, `ASSERT_FALSE`, `ASSERT_EQ`, `ASSERT_NE`, `ASSERT_STREQ`, `ASSERT_STRNE`, `ASSERT_NULL`, `ASSERT_NOT_NULL`
- **Automatic Test Registration**: Tests are registered automatically using constructor attributes
- **Test Runner**: Automatically discovers and runs all registered tests

## Test Structure

### Test Harness (`test_harness.h` / `test_harness.c`)
Core test framework with assertion macros and test runner.

### Example Tests (`test_examples.c`)
Tests that compile and run example `.ae` files from the `examples/` directory.

### Integration Tests (`test_integration.c`)
Tests for the full compilation and execution pipeline, including:
- Loop compilation and execution
- Runtime shutdown behavior
- End-to-end workflows

### Unit Tests
- `test_lexer.c` - Lexer component tests
- `test_parser.c` - Parser component tests
- `test_codegen.c` - Code generator tests
- `test_typechecker.c` - Type checker tests

## Running Tests

### Run All Tests
```bash
make test
```

### Run Specific Test Categories
```bash
make test-examples    # Run example-based tests
make test-unit        # Run unit tests
make test-integration # Run integration tests
```

## Writing Tests

To write a new test, use the `TEST` macro:

```c
#include "test_harness.h"

TEST(my_test_name) {
    int x = 5;
    int y = 10;
    
    ASSERT_EQ(5, x);
    ASSERT_NE(x, y);
    ASSERT_TRUE(x < y);
}
```

The test will be automatically registered and run when you execute `make test`.

## Test Output

Tests output progress as they run:
```
[1/10] Running test: simple_for_loop
  ✓ PASSED
[2/10] Running test: test_for_loop
  ✓ PASSED
...

========================================
Test Results: 10 passed, 0 failed, 10 total
========================================
```

## Debugging Failed Tests

If a test fails, the assertion macros will print:
- The file and line number where the assertion failed
- The condition or values that failed
- The test will exit with code 1

## Notes

- Tests that compile and run example files require the compiler (`aetherc.exe`) to be built first
- Integration tests create temporary files in the `build/` directory
- Some unit tests are placeholders and need implementation when test APIs are added to compiler components


This directory contains the test infrastructure for the Aether language compiler and runtime.

## Test Framework

The test framework is a lightweight C test harness that provides:

- **Assertion Macros**: `ASSERT_TRUE`, `ASSERT_FALSE`, `ASSERT_EQ`, `ASSERT_NE`, `ASSERT_STREQ`, `ASSERT_STRNE`, `ASSERT_NULL`, `ASSERT_NOT_NULL`
- **Automatic Test Registration**: Tests are registered automatically using constructor attributes
- **Test Runner**: Automatically discovers and runs all registered tests

## Test Structure

### Test Harness (`test_harness.h` / `test_harness.c`)
Core test framework with assertion macros and test runner.

### Example Tests (`test_examples.c`)
Tests that compile and run example `.ae` files from the `examples/` directory.

### Integration Tests (`test_integration.c`)
Tests for the full compilation and execution pipeline, including:
- Loop compilation and execution
- Runtime shutdown behavior
- End-to-end workflows

### Unit Tests
- `test_lexer.c` - Lexer component tests
- `test_parser.c` - Parser component tests
- `test_codegen.c` - Code generator tests
- `test_typechecker.c` - Type checker tests

## Running Tests

### Run All Tests
```bash
make test
```

### Run Specific Test Categories
```bash
make test-examples    # Run example-based tests
make test-unit        # Run unit tests
make test-integration # Run integration tests
```

## Writing Tests

To write a new test, use the `TEST` macro:

```c
#include "test_harness.h"

TEST(my_test_name) {
    int x = 5;
    int y = 10;
    
    ASSERT_EQ(5, x);
    ASSERT_NE(x, y);
    ASSERT_TRUE(x < y);
}
```

The test will be automatically registered and run when you execute `make test`.

## Test Output

Tests output progress as they run:
```
[1/10] Running test: simple_for_loop
  ✓ PASSED
[2/10] Running test: test_for_loop
  ✓ PASSED
...

========================================
Test Results: 10 passed, 0 failed, 10 total
========================================
```

## Debugging Failed Tests

If a test fails, the assertion macros will print:
- The file and line number where the assertion failed
- The condition or values that failed
- The test will exit with code 1

## Notes

- Tests that compile and run example files require the compiler (`aetherc.exe`) to be built first
- Integration tests create temporary files in the `build/` directory
- Some unit tests are placeholders and need implementation when test APIs are added to compiler components


This directory contains the test infrastructure for the Aether language compiler and runtime.

## Test Framework

The test framework is a lightweight C test harness that provides:

- **Assertion Macros**: `ASSERT_TRUE`, `ASSERT_FALSE`, `ASSERT_EQ`, `ASSERT_NE`, `ASSERT_STREQ`, `ASSERT_STRNE`, `ASSERT_NULL`, `ASSERT_NOT_NULL`
- **Automatic Test Registration**: Tests are registered automatically using constructor attributes
- **Test Runner**: Automatically discovers and runs all registered tests

## Test Structure

### Test Harness (`test_harness.h` / `test_harness.c`)
Core test framework with assertion macros and test runner.

### Example Tests (`test_examples.c`)
Tests that compile and run example `.ae` files from the `examples/` directory.

### Integration Tests (`test_integration.c`)
Tests for the full compilation and execution pipeline, including:
- Loop compilation and execution
- Runtime shutdown behavior
- End-to-end workflows

### Unit Tests
- `test_lexer.c` - Lexer component tests
- `test_parser.c` - Parser component tests
- `test_codegen.c` - Code generator tests
- `test_typechecker.c` - Type checker tests

## Running Tests

### Run All Tests
```bash
make test
```

### Run Specific Test Categories
```bash
make test-examples    # Run example-based tests
make test-unit        # Run unit tests
make test-integration # Run integration tests
```

## Writing Tests

To write a new test, use the `TEST` macro:

```c
#include "test_harness.h"

TEST(my_test_name) {
    int x = 5;
    int y = 10;
    
    ASSERT_EQ(5, x);
    ASSERT_NE(x, y);
    ASSERT_TRUE(x < y);
}
```

The test will be automatically registered and run when you execute `make test`.

## Test Output

Tests output progress as they run:
```
[1/10] Running test: simple_for_loop
  ✓ PASSED
[2/10] Running test: test_for_loop
  ✓ PASSED
...

========================================
Test Results: 10 passed, 0 failed, 10 total
========================================
```

## Debugging Failed Tests

If a test fails, the assertion macros will print:
- The file and line number where the assertion failed
- The condition or values that failed
- The test will exit with code 1

## Notes

- Tests that compile and run example files require the compiler (`aetherc.exe`) to be built first
- Integration tests create temporary files in the `build/` directory
- Some unit tests are placeholders and need implementation when test APIs are added to compiler components


This directory contains the test infrastructure for the Aether language compiler and runtime.

## Test Framework

The test framework is a lightweight C test harness that provides:

- **Assertion Macros**: `ASSERT_TRUE`, `ASSERT_FALSE`, `ASSERT_EQ`, `ASSERT_NE`, `ASSERT_STREQ`, `ASSERT_STRNE`, `ASSERT_NULL`, `ASSERT_NOT_NULL`
- **Automatic Test Registration**: Tests are registered automatically using constructor attributes
- **Test Runner**: Automatically discovers and runs all registered tests

## Test Structure

### Test Harness (`test_harness.h` / `test_harness.c`)
Core test framework with assertion macros and test runner.

### Example Tests (`test_examples.c`)
Tests that compile and run example `.ae` files from the `examples/` directory.

### Integration Tests (`test_integration.c`)
Tests for the full compilation and execution pipeline, including:
- Loop compilation and execution
- Runtime shutdown behavior
- End-to-end workflows

### Unit Tests
- `test_lexer.c` - Lexer component tests
- `test_parser.c` - Parser component tests
- `test_codegen.c` - Code generator tests
- `test_typechecker.c` - Type checker tests

## Running Tests

### Run All Tests
```bash
make test
```

### Run Specific Test Categories
```bash
make test-examples    # Run example-based tests
make test-unit        # Run unit tests
make test-integration # Run integration tests
```

## Writing Tests

To write a new test, use the `TEST` macro:

```c
#include "test_harness.h"

TEST(my_test_name) {
    int x = 5;
    int y = 10;
    
    ASSERT_EQ(5, x);
    ASSERT_NE(x, y);
    ASSERT_TRUE(x < y);
}
```

The test will be automatically registered and run when you execute `make test`.

## Test Output

Tests output progress as they run:
```
[1/10] Running test: simple_for_loop
  ✓ PASSED
[2/10] Running test: test_for_loop
  ✓ PASSED
...

========================================
Test Results: 10 passed, 0 failed, 10 total
========================================
```

## Debugging Failed Tests

If a test fails, the assertion macros will print:
- The file and line number where the assertion failed
- The condition or values that failed
- The test will exit with code 1

## Notes

- Tests that compile and run example files require the compiler (`aetherc.exe`) to be built first
- Integration tests create temporary files in the `build/` directory
- Some unit tests are placeholders and need implementation when test APIs are added to compiler components


This directory contains the test infrastructure for the Aether language compiler and runtime.

## Test Framework

The test framework is a lightweight C test harness that provides:

- **Assertion Macros**: `ASSERT_TRUE`, `ASSERT_FALSE`, `ASSERT_EQ`, `ASSERT_NE`, `ASSERT_STREQ`, `ASSERT_STRNE`, `ASSERT_NULL`, `ASSERT_NOT_NULL`
- **Automatic Test Registration**: Tests are registered automatically using constructor attributes
- **Test Runner**: Automatically discovers and runs all registered tests

## Test Structure

### Test Harness (`test_harness.h` / `test_harness.c`)
Core test framework with assertion macros and test runner.

### Example Tests (`test_examples.c`)
Tests that compile and run example `.ae` files from the `examples/` directory.

### Integration Tests (`test_integration.c`)
Tests for the full compilation and execution pipeline, including:
- Loop compilation and execution
- Runtime shutdown behavior
- End-to-end workflows

### Unit Tests
- `test_lexer.c` - Lexer component tests
- `test_parser.c` - Parser component tests
- `test_codegen.c` - Code generator tests
- `test_typechecker.c` - Type checker tests

## Running Tests

### Run All Tests
```bash
make test
```

### Run Specific Test Categories
```bash
make test-examples    # Run example-based tests
make test-unit        # Run unit tests
make test-integration # Run integration tests
```

## Writing Tests

To write a new test, use the `TEST` macro:

```c
#include "test_harness.h"

TEST(my_test_name) {
    int x = 5;
    int y = 10;
    
    ASSERT_EQ(5, x);
    ASSERT_NE(x, y);
    ASSERT_TRUE(x < y);
}
```

The test will be automatically registered and run when you execute `make test`.

## Test Output

Tests output progress as they run:
```
[1/10] Running test: simple_for_loop
  ✓ PASSED
[2/10] Running test: test_for_loop
  ✓ PASSED
...

========================================
Test Results: 10 passed, 0 failed, 10 total
========================================
```

## Debugging Failed Tests

If a test fails, the assertion macros will print:
- The file and line number where the assertion failed
- The condition or values that failed
- The test will exit with code 1

## Notes

- Tests that compile and run example files require the compiler (`aetherc.exe`) to be built first
- Integration tests create temporary files in the `build/` directory
- Some unit tests are placeholders and need implementation when test APIs are added to compiler components


This directory contains the test infrastructure for the Aether language compiler and runtime.

## Test Framework

The test framework is a lightweight C test harness that provides:

- **Assertion Macros**: `ASSERT_TRUE`, `ASSERT_FALSE`, `ASSERT_EQ`, `ASSERT_NE`, `ASSERT_STREQ`, `ASSERT_STRNE`, `ASSERT_NULL`, `ASSERT_NOT_NULL`
- **Automatic Test Registration**: Tests are registered automatically using constructor attributes
- **Test Runner**: Automatically discovers and runs all registered tests

## Test Structure

### Test Harness (`test_harness.h` / `test_harness.c`)
Core test framework with assertion macros and test runner.

### Example Tests (`test_examples.c`)
Tests that compile and run example `.ae` files from the `examples/` directory.

### Integration Tests (`test_integration.c`)
Tests for the full compilation and execution pipeline, including:
- Loop compilation and execution
- Runtime shutdown behavior
- End-to-end workflows

### Unit Tests
- `test_lexer.c` - Lexer component tests
- `test_parser.c` - Parser component tests
- `test_codegen.c` - Code generator tests
- `test_typechecker.c` - Type checker tests

## Running Tests

### Run All Tests
```bash
make test
```

### Run Specific Test Categories
```bash
make test-examples    # Run example-based tests
make test-unit        # Run unit tests
make test-integration # Run integration tests
```

## Writing Tests

To write a new test, use the `TEST` macro:

```c
#include "test_harness.h"

TEST(my_test_name) {
    int x = 5;
    int y = 10;
    
    ASSERT_EQ(5, x);
    ASSERT_NE(x, y);
    ASSERT_TRUE(x < y);
}
```

The test will be automatically registered and run when you execute `make test`.

## Test Output

Tests output progress as they run:
```
[1/10] Running test: simple_for_loop
  ✓ PASSED
[2/10] Running test: test_for_loop
  ✓ PASSED
...

========================================
Test Results: 10 passed, 0 failed, 10 total
========================================
```

## Debugging Failed Tests

If a test fails, the assertion macros will print:
- The file and line number where the assertion failed
- The condition or values that failed
- The test will exit with code 1

## Notes

- Tests that compile and run example files require the compiler (`aetherc.exe`) to be built first
- Integration tests create temporary files in the `build/` directory
- Some unit tests are placeholders and need implementation when test APIs are added to compiler components


This directory contains the test infrastructure for the Aether language compiler and runtime.

## Test Framework

The test framework is a lightweight C test harness that provides:

- **Assertion Macros**: `ASSERT_TRUE`, `ASSERT_FALSE`, `ASSERT_EQ`, `ASSERT_NE`, `ASSERT_STREQ`, `ASSERT_STRNE`, `ASSERT_NULL`, `ASSERT_NOT_NULL`
- **Automatic Test Registration**: Tests are registered automatically using constructor attributes
- **Test Runner**: Automatically discovers and runs all registered tests

## Test Structure

### Test Harness (`test_harness.h` / `test_harness.c`)
Core test framework with assertion macros and test runner.

### Example Tests (`test_examples.c`)
Tests that compile and run example `.ae` files from the `examples/` directory.

### Integration Tests (`test_integration.c`)
Tests for the full compilation and execution pipeline, including:
- Loop compilation and execution
- Runtime shutdown behavior
- End-to-end workflows

### Unit Tests
- `test_lexer.c` - Lexer component tests
- `test_parser.c` - Parser component tests
- `test_codegen.c` - Code generator tests
- `test_typechecker.c` - Type checker tests

## Running Tests

### Run All Tests
```bash
make test
```

### Run Specific Test Categories
```bash
make test-examples    # Run example-based tests
make test-unit        # Run unit tests
make test-integration # Run integration tests
```

## Writing Tests

To write a new test, use the `TEST` macro:

```c
#include "test_harness.h"

TEST(my_test_name) {
    int x = 5;
    int y = 10;
    
    ASSERT_EQ(5, x);
    ASSERT_NE(x, y);
    ASSERT_TRUE(x < y);
}
```

The test will be automatically registered and run when you execute `make test`.

## Test Output

Tests output progress as they run:
```
[1/10] Running test: simple_for_loop
  ✓ PASSED
[2/10] Running test: test_for_loop
  ✓ PASSED
...

========================================
Test Results: 10 passed, 0 failed, 10 total
========================================
```

## Debugging Failed Tests

If a test fails, the assertion macros will print:
- The file and line number where the assertion failed
- The condition or values that failed
- The test will exit with code 1

## Notes

- Tests that compile and run example files require the compiler (`aetherc.exe`) to be built first
- Integration tests create temporary files in the `build/` directory
- Some unit tests are placeholders and need implementation when test APIs are added to compiler components


This directory contains the test infrastructure for the Aether language compiler and runtime.

## Test Framework

The test framework is a lightweight C test harness that provides:

- **Assertion Macros**: `ASSERT_TRUE`, `ASSERT_FALSE`, `ASSERT_EQ`, `ASSERT_NE`, `ASSERT_STREQ`, `ASSERT_STRNE`, `ASSERT_NULL`, `ASSERT_NOT_NULL`
- **Automatic Test Registration**: Tests are registered automatically using constructor attributes
- **Test Runner**: Automatically discovers and runs all registered tests

## Test Structure

### Test Harness (`test_harness.h` / `test_harness.c`)
Core test framework with assertion macros and test runner.

### Example Tests (`test_examples.c`)
Tests that compile and run example `.ae` files from the `examples/` directory.

### Integration Tests (`test_integration.c`)
Tests for the full compilation and execution pipeline, including:
- Loop compilation and execution
- Runtime shutdown behavior
- End-to-end workflows

### Unit Tests
- `test_lexer.c` - Lexer component tests
- `test_parser.c` - Parser component tests
- `test_codegen.c` - Code generator tests
- `test_typechecker.c` - Type checker tests

## Running Tests

### Run All Tests
```bash
make test
```

### Run Specific Test Categories
```bash
make test-examples    # Run example-based tests
make test-unit        # Run unit tests
make test-integration # Run integration tests
```

## Writing Tests

To write a new test, use the `TEST` macro:

```c
#include "test_harness.h"

TEST(my_test_name) {
    int x = 5;
    int y = 10;
    
    ASSERT_EQ(5, x);
    ASSERT_NE(x, y);
    ASSERT_TRUE(x < y);
}
```

The test will be automatically registered and run when you execute `make test`.

## Test Output

Tests output progress as they run:
```
[1/10] Running test: simple_for_loop
  ✓ PASSED
[2/10] Running test: test_for_loop
  ✓ PASSED
...

========================================
Test Results: 10 passed, 0 failed, 10 total
========================================
```

## Debugging Failed Tests

If a test fails, the assertion macros will print:
- The file and line number where the assertion failed
- The condition or values that failed
- The test will exit with code 1

## Notes

- Tests that compile and run example files require the compiler (`aetherc.exe`) to be built first
- Integration tests create temporary files in the `build/` directory
- Some unit tests are placeholders and need implementation when test APIs are added to compiler components


This directory contains the test infrastructure for the Aether language compiler and runtime.

## Test Framework

The test framework is a lightweight C test harness that provides:

- **Assertion Macros**: `ASSERT_TRUE`, `ASSERT_FALSE`, `ASSERT_EQ`, `ASSERT_NE`, `ASSERT_STREQ`, `ASSERT_STRNE`, `ASSERT_NULL`, `ASSERT_NOT_NULL`
- **Automatic Test Registration**: Tests are registered automatically using constructor attributes
- **Test Runner**: Automatically discovers and runs all registered tests

## Test Structure

### Test Harness (`test_harness.h` / `test_harness.c`)
Core test framework with assertion macros and test runner.

### Example Tests (`test_examples.c`)
Tests that compile and run example `.ae` files from the `examples/` directory.

### Integration Tests (`test_integration.c`)
Tests for the full compilation and execution pipeline, including:
- Loop compilation and execution
- Runtime shutdown behavior
- End-to-end workflows

### Unit Tests
- `test_lexer.c` - Lexer component tests
- `test_parser.c` - Parser component tests
- `test_codegen.c` - Code generator tests
- `test_typechecker.c` - Type checker tests

## Running Tests

### Run All Tests
```bash
make test
```

### Run Specific Test Categories
```bash
make test-examples    # Run example-based tests
make test-unit        # Run unit tests
make test-integration # Run integration tests
```

## Writing Tests

To write a new test, use the `TEST` macro:

```c
#include "test_harness.h"

TEST(my_test_name) {
    int x = 5;
    int y = 10;
    
    ASSERT_EQ(5, x);
    ASSERT_NE(x, y);
    ASSERT_TRUE(x < y);
}
```

The test will be automatically registered and run when you execute `make test`.

## Test Output

Tests output progress as they run:
```
[1/10] Running test: simple_for_loop
  ✓ PASSED
[2/10] Running test: test_for_loop
  ✓ PASSED
...

========================================
Test Results: 10 passed, 0 failed, 10 total
========================================
```

## Debugging Failed Tests

If a test fails, the assertion macros will print:
- The file and line number where the assertion failed
- The condition or values that failed
- The test will exit with code 1

## Notes

- Tests that compile and run example files require the compiler (`aetherc.exe`) to be built first
- Integration tests create temporary files in the `build/` directory
- Some unit tests are placeholders and need implementation when test APIs are added to compiler components


This directory contains the test infrastructure for the Aether language compiler and runtime.

## Test Framework

The test framework is a lightweight C test harness that provides:

- **Assertion Macros**: `ASSERT_TRUE`, `ASSERT_FALSE`, `ASSERT_EQ`, `ASSERT_NE`, `ASSERT_STREQ`, `ASSERT_STRNE`, `ASSERT_NULL`, `ASSERT_NOT_NULL`
- **Automatic Test Registration**: Tests are registered automatically using constructor attributes
- **Test Runner**: Automatically discovers and runs all registered tests

## Test Structure

### Test Harness (`test_harness.h` / `test_harness.c`)
Core test framework with assertion macros and test runner.

### Example Tests (`test_examples.c`)
Tests that compile and run example `.ae` files from the `examples/` directory.

### Integration Tests (`test_integration.c`)
Tests for the full compilation and execution pipeline, including:
- Loop compilation and execution
- Runtime shutdown behavior
- End-to-end workflows

### Unit Tests
- `test_lexer.c` - Lexer component tests
- `test_parser.c` - Parser component tests
- `test_codegen.c` - Code generator tests
- `test_typechecker.c` - Type checker tests

## Running Tests

### Run All Tests
```bash
make test
```

### Run Specific Test Categories
```bash
make test-examples    # Run example-based tests
make test-unit        # Run unit tests
make test-integration # Run integration tests
```

## Writing Tests

To write a new test, use the `TEST` macro:

```c
#include "test_harness.h"

TEST(my_test_name) {
    int x = 5;
    int y = 10;
    
    ASSERT_EQ(5, x);
    ASSERT_NE(x, y);
    ASSERT_TRUE(x < y);
}
```

The test will be automatically registered and run when you execute `make test`.

## Test Output

Tests output progress as they run:
```
[1/10] Running test: simple_for_loop
  ✓ PASSED
[2/10] Running test: test_for_loop
  ✓ PASSED
...

========================================
Test Results: 10 passed, 0 failed, 10 total
========================================
```

## Debugging Failed Tests

If a test fails, the assertion macros will print:
- The file and line number where the assertion failed
- The condition or values that failed
- The test will exit with code 1

## Notes

- Tests that compile and run example files require the compiler (`aetherc.exe`) to be built first
- Integration tests create temporary files in the `build/` directory
- Some unit tests are placeholders and need implementation when test APIs are added to compiler components




