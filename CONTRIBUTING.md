# Contributing to Aether

Thank you for your interest in contributing to Aether. This document outlines the guidelines for contributing code, tests, and documentation.

## Code Style

### General Guidelines

- Use 2-space indentation (no tabs)
- Maximum line length: 100 characters
- Use descriptive variable and function names
- Add comments for complex logic

### Naming Conventions

```c
// Types: PascalCase
typedef struct AetherModule { ... } AetherModule;

// Functions: snake_case
void parse_expression(Parser* parser);
ASTNode* create_ast_node(ASTNodeType type);

// Variables: snake_case
int token_count = 0;
const char* module_name = "std.io";

// Constants: UPPER_SNAKE_CASE
#define MAX_TOKENS 1000
#define DEFAULT_BUFFER_SIZE 4096

// Private functions: static with leading underscore (optional)
static void _internal_helper(void);
```

### File Organization

```c
// 1. Includes (system headers first, then local)
#include <stdio.h>
#include <stdlib.h>
#include "aether_types.h"
#include "aether_parser.h"

// 2. Constants and macros
#define MAX_BUFFER 256

// 3. Type definitions
typedef struct { ... } MyStruct;

// 4. Forward declarations
static void helper_function(void);

// 5. Global variables (avoid when possible)
static int global_counter = 0;

// 6. Function implementations
void public_function(void) {
    // Implementation
}

static void helper_function(void) {
    // Implementation
}
```

### Memory Management

- Always check for `NULL` returns from allocation functions
- Free all allocated memory
- Use `defer` statement when available for automatic cleanup
- Run Valgrind to verify no memory leaks

```c
// Good
char* buffer = malloc(256);
if (!buffer) {
    return NULL;
}
// ... use buffer ...
free(buffer);

// Better (when available)
char* buffer = malloc(256);
defer(free(buffer));
if (!buffer) {
    return NULL;
}
// ... use buffer ...
// Automatically freed on scope exit
```

## Adding Tests

### Test Structure

Tests are located in the `tests/` directory and use the test harness framework.

```c
#include "test_harness.h"

// Simple test
TEST(my_feature) {
    ASSERT_EQ(add(2, 3), 5);
    ASSERT_TRUE(is_valid("test"));
}

// Test with category
TEST_CATEGORY(hashmap_insert, TEST_CATEGORY_COLLECTIONS) {
    HashMap* map = hashmap_create(16);
    ASSERT_NOT_NULL(map);
    
    hashmap_insert(map, "key", "value");
    ASSERT_STREQ(hashmap_get(map, "key"), "value");
    
    hashmap_free(map);
}
```

### Test Categories

- `TEST_CATEGORY_COMPILER` - Lexer, parser, type checker, code generator
- `TEST_CATEGORY_RUNTIME` - Actor system, scheduler, message passing
- `TEST_CATEGORY_COLLECTIONS` - HashMap, Set, Vector, PriorityQueue
- `TEST_CATEGORY_NETWORK` - HTTP, TCP, networking utilities
- `TEST_CATEGORY_MEMORY` - Arena allocators, memory pools, leak detection
- `TEST_CATEGORY_STDLIB` - Standard library functions
- `TEST_CATEGORY_PARSER` - Parser-specific tests
- `TEST_CATEGORY_OTHER` - Miscellaneous tests

### Assertion Macros

```c
ASSERT_TRUE(condition)          // Assert condition is true
ASSERT_FALSE(condition)         // Assert condition is false
ASSERT_EQ(expected, actual)     // Assert equality (integers)
ASSERT_NE(expected, actual)     // Assert inequality
ASSERT_STREQ(expected, actual)  // Assert string equality
ASSERT_STRNE(expected, actual)  // Assert string inequality
ASSERT_NULL(ptr)                // Assert pointer is NULL
ASSERT_NOT_NULL(ptr)            // Assert pointer is not NULL
```

### Running Tests

```bash
# All tests
make test

# Specific category (when implemented)
./build/test_runner --category=collections

# With Valgrind
make test-valgrind

# With AddressSanitizer
make test-asan
```

## Pull Request Requirements

### Before Submitting

1. **Code compiles without warnings**
   ```bash
   make clean
   make compiler
   ```

2. **All tests pass**
   ```bash
   make test
   ```

3. **No memory leaks**
   ```bash
   valgrind ./build/test_runner
   # Should report: "definitely lost: 0 bytes"
   ```

4. **Add tests for new features**
   - New feature = new test
   - Bug fix = regression test

5. **Update documentation**
   - Add/update comments in code
   - Update README.md if adding user-facing features
   - Update docs/ if changing language behavior

### PR Template

```markdown
## Description
Brief description of changes

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Performance improvement
- [ ] Documentation update

## Testing
- [ ] Added tests for new functionality
- [ ] All tests pass (make test)
- [ ] Valgrind reports no leaks
- [ ] Tested on Linux/macOS/Windows (if applicable)

## Performance Impact
- [ ] No performance impact
- [ ] Performance improvement (include benchmarks)
- [ ] Performance regression (explain why acceptable)

## Checklist
- [ ] Code follows style guidelines
- [ ] Self-review completed
- [ ] Comments added for complex logic
- [ ] Documentation updated
```

### Review Process

1. Automated CI checks must pass (Linux/macOS, memory safety, benchmarks)
2. Code review by maintainer
3. Address feedback
4. Merge when approved

## Common Pitfalls

### Memory Management

```c
// BAD: Memory leak
char* create_string(void) {
    char* str = malloc(100);
    strcpy(str, "test");
    return str;  // Caller must remember to free
}

// GOOD: Document ownership
// Returns: Newly allocated string (caller must free)
char* create_string(void) {
    char* str = malloc(100);
    strcpy(str, "test");
    return str;
}

// BETTER: Use arena allocation
char* create_string(Arena* arena) {
    char* str = arena_alloc(arena, 100);
    strcpy(str, "test");
    return str;  // Freed when arena is freed
}
```

### Error Handling

```c
// BAD: Ignoring errors
FILE* f = fopen("file.txt", "r");
fread(buffer, 1, 100, f);  // Crashes if f is NULL

// GOOD: Check for errors
FILE* f = fopen("file.txt", "r");
if (!f) {
    fprintf(stderr, "Failed to open file\n");
    return -1;
}
defer(fclose(f));
```

### Platform-Specific Code

```c
// BAD: Linux-specific
#include <unistd.h>
usleep(1000);

// GOOD: Cross-platform
#ifdef _WIN32
    #include <windows.h>
    Sleep(1);  // milliseconds
#else
    #include <unistd.h>
    usleep(1000);  // microseconds
#endif
```

## Versioning and Release Process

Aether uses [Semantic Versioning](https://semver.org/). Releases are fully automated.

### How it works

1. **Source of truth**: Git tags (`v*.*.*`). The `VERSION` file is kept in sync for non-git contexts (release tarballs, binary installs).

2. **Automatic release**: Every merge to `main` triggers `.github/workflows/release.yml`, which:
   - Computes the next version from the highest existing `v*.*.*` tag
   - Updates the `VERSION` file on `main` and commits `chore: release X.Y.Z`
   - Tags the commit and pushes both to `main` and the tag
   - Builds binaries for Linux, macOS (arm64 + x86_64), and Windows
   - Creates a GitHub Release with all artifacts

3. **Version bump rules**:
   - Commit message starts with `major` → bumps MAJOR (e.g., 0.17.0 → 1.0.0)
   - Anything else → bumps MINOR (e.g., 0.17.0 → 0.18.0)

4. **Race prevention**: The workflow uses a `concurrency` group — if two PRs merge in quick succession, the second release queues until the first completes.

### Where the version appears

| Component | How it gets the version |
|-----------|------------------------|
| `make ae` / `make compiler` | Makefile reads `git tag -l`, falls back to `VERSION` file |
| `ae version` | Compiled-in via `-DAETHER_VERSION` from Makefile |
| `aetherc --version` | Compiled-in via `-DAETHER_VERSION` from Makefile |
| `install.sh` | Reads `VERSION` file |
| Release tarballs | `VERSION` file baked into the archive |
| Windows native builds | Reads `VERSION` file (no git dependency) |

### For contributors

- **Never edit the `VERSION` file manually** — it's updated automatically by the release workflow
- **Never create `v*.*.*` tags manually** — let the workflow handle it
- **Update `CHANGELOG.md`** when adding features or fixes; the release workflow does not auto-generate changelogs

### Building locally

```bash
make ae          # Picks up version from git tags automatically
./build/ae version   # Verify: should show the latest tag
```

If you're building outside a git repo (e.g., from a release tarball):

```bash
make ae          # Falls back to VERSION file
```

## Getting Help

- GitHub Issues: Report bugs and request features
- Discussions: Ask questions and share ideas
- Code Comments: Explain complex implementations
- Documentation: Check docs/ folder for language details

## License

By contributing to Aether, you agree that your contributions will be licensed under the MIT License.
