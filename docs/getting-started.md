# Getting Started with Aether

Welcome to Aether! This guide will help you get up and running with the language.

## Installation

### Windows (Cygwin)

1. Install [Cygwin](https://www.cygwin.com/) with GCC
2. Clone the repository:
   ```bash
   git clone https://github.com/yourusername/aether.git
   cd aether
   ```
3. Build the compiler:
   ```powershell
   .\build_compiler.ps1
   ```

### Linux/Mac

1. Ensure you have GCC installed
2. Clone and build:
   ```bash
   git clone https://github.com/yourusername/aether.git
   cd aether
   make
   ```

## Your First Program

Create a file called `hello.ae`:

```aether
main() {
    print("Hello, Aether!\n")
}
```

Compile and run:

```bash
# Compile Aether to C
./build/aetherc hello.ae hello.c

# Compile C to executable
gcc hello.c -Iruntime runtime/*.c -o hello

# Run
./hello
```

## Language Basics

### Type Inference

Aether automatically infers types - no annotations needed!

```aether
x = 42              // int
pi = 3.14           // float
name = "Alice"      // string
```

### Functions

Functions are simple and clean:

```aether
add(a, b) {
    return a + b
}

greet(name) {
    print("Hello, ")
    print(name)
    print("!\n")
}

main() {
    result = add(10, 20)
    greet("World")
}
```

### Control Flow

Standard control structures work as expected:

```aether
// If statements
check_age(age) {
    if (age >= 18) {
        print("Adult\n")
    } else {
        print("Minor\n")
    }
}

// While loops
count_to_ten() {
    i = 0
    while (i < 10) {
        print(i)
        i = i + 1
    }
}

// For loops
sum_numbers() {
    sum = 0
    for (i = 0; i < 100; i = i + 1) {
        sum = sum + i
    }
    return sum
}
```

### Structs

Organize data with structs:

```aether
struct Point {
    int x;
    int y;
}

main() {
    p = Point{ x: 10, y: 20 }
    print(p.x)
    print(p.y)
}
```

### Actors

Aether's killer feature - easy concurrency with actors:

```aether
actor Counter {
    state count = 0;
    
    receive(msg) {
        if (msg.type == 1) {
            count = count + 1
        }
    }
}

main() {
    // Spawn an actor
    counter = spawn_Counter()
    
    // Send messages
    send_Counter(counter, 1, 0)
    send_Counter(counter, 1, 0)
    
    // Process messages
    Counter_step(counter)
    Counter_step(counter)
    
    print("Counter updated!\n")
}
```

## Next Steps

- Check out the [examples/](../examples/) directory for more programs
- Read the [Language Reference](language-reference.md) for complete syntax
- Learn about [Actors](runtime.md) for concurrent programming
- See [Type Inference](type-inference-guide.md) for advanced type system features

## Common Issues

### Compilation Errors

If you get "undefined reference" errors, make sure you're linking all runtime files:

```bash
gcc output.c -Iruntime runtime/multicore_scheduler.c runtime/memory.c runtime/aether_string.c -o program
```

### Type Errors

Aether infers types automatically, but sometimes you need explicit types in struct fields:

```aether
struct Point {
    int x;    // Explicit type required
    int y;
}
```

## Getting Help

- Check the [docs/](../docs/) directory for detailed documentation
- Look at [examples/](../examples/) for working code samples
- File issues on GitHub for bugs or questions

Happy coding with Aether!



