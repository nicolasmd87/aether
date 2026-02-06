# C Interoperability

Aether provides seamless interoperability with C code, allowing you to leverage the entire C ecosystem including existing libraries like SQLite, libcurl, and OpenSSL.

## Standard C Functions

Standard C library functions (printf, strlen, malloc, etc.) are automatically available through the included headers. No special declarations are needed:

```aether
main() {
    // print() uses printf internally
    print("Hello from Aether!\n")

    x = 42
    print("Value: ")
    print(x)
    print("\n")
}
```

## The `extern` Keyword

Use `extern` to declare C functions you want to call from Aether code. This is useful for:
- Your own C functions in separate `.c` files
- Third-party C libraries (SQLite, libcurl, etc.)
- System APIs

### Syntax

```aether
extern function_name(param1: type1, param2: type2) -> return_type
extern void_function(param: type)  // No return type = void
```

### Type Mapping

| Aether Type | C Type |
|-------------|--------|
| `int` | `int` |
| `float` | `double` |
| `string` | `const char*` |
| `bool` | `int` |
| `ptr` | `void*` |

### Example: Custom C Functions

**my_math.c:**
```c
#include <math.h>

int my_add(int a, int b) {
    return a + b;
}

double my_power(double base, double exp) {
    return pow(base, exp);
}

void my_greet(const char* name) {
    printf("Hello, %s!\n", name);
}
```

**main.ae:**
```aether
// Declare our C functions
extern my_add(a: int, b: int) -> int
extern my_power(base: float, exp: float) -> float
extern my_greet(name: string)

main() {
    result = my_add(10, 20)
    print("10 + 20 = ")
    print(result)
    print("\n")

    power = my_power(2.0, 10.0)
    print("2^10 = ")
    print(power)
    print("\n")

    my_greet("Aether")
}
```

**Build and run:**
```bash
# Compile C code
gcc -c my_math.c -o my_math.o -lm

# Compile Aether code to C
aetherc main.ae main.c

# Link everything together
gcc -I$HOME/.aether/include/aether/runtime main.c my_math.o \
    -L$HOME/.aether/lib -laether -lm -o myapp

./myapp
```

## Linking External Libraries

Use `link_flags` in your `aether.toml` to link external C libraries:

```toml
[project]
name = "my-project"
version = "1.0.0"

[build]
link_flags = ["-lsqlite3", "-lcurl", "-lm"]
```

### Example: Using SQLite

**database.ae:**
```aether
// SQLite C API
extern sqlite3_open(path: string, db: ptr) -> int
extern sqlite3_close(db: ptr) -> int
extern sqlite3_exec(db: ptr, sql: string, callback: ptr, arg: ptr, errmsg: ptr) -> int

main() {
    db = 0  // Will hold database pointer

    // Open database
    result = sqlite3_open("test.db", db)
    if (result != 0) {
        print("Failed to open database\n")
        return
    }

    // Execute SQL
    sql = "CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY, name TEXT)"
    sqlite3_exec(db, sql, 0, 0, 0)

    // Close database
    sqlite3_close(db)
    print("Database operations complete\n")
}
```

**aether.toml:**
```toml
[project]
name = "sqlite-demo"
version = "1.0.0"

[build]
link_flags = ["-lsqlite3"]
```

## Best Practices

1. **Prefer Aether's standard library** for common operations when available
2. **Use `extern` sparingly** - only for external C code, not standard library functions
3. **Document your C dependencies** in your project's README
4. **Handle errors** - C functions often return error codes
5. **Memory management** - Be careful with C memory; use Aether's memory management where possible

## Working with Pointers

The `ptr` type maps to `void*` in C, useful for opaque handles and callbacks:

```aether
extern create_handle() -> ptr
extern use_handle(h: ptr) -> int
extern destroy_handle(h: ptr)

main() {
    handle = create_handle()
    if (handle != 0) {
        use_handle(handle)
        destroy_handle(handle)
    }
}
```

## Embedding Aether in C Applications

If you want to embed Aether actors in your existing C application (the reverse direction), see the [C Embedding Guide](c-embedding.md). This covers:

- Compiling Aether to C and linking with your application
- Using the Aether runtime API directly from C
- The `--emit-header` compiler flag for generating C headers

## See Also

- [Getting Started](getting-started.md)
- [C Embedding Guide](c-embedding.md) - Embed Aether actors in C applications
- [Standard Library Reference](stdlib-reference.md)
- [Language Reference](language-reference.md)
