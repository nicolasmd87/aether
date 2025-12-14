# Getting Started with Aether

## Quick Start

### 1. Build the Compiler

```bash
cd src
make
```

This will create `aetherc_new`, the new Aether compiler.

### 2. Write Your First Program

Create a file called `hello.ae`:

```aether
main() {
    print("Hello, Aether!\n");
}
```

### 3. Compile and Run

```bash
./aetherc_new hello.ae hello.c
gcc hello.c ../runtime/*.c -o hello -lpthread
./hello
```

## Actor Example

Create `counter.ae`:

```aether
actor Counter {
    state int count = 0;
    
    receive(msg) {
        match msg {
            Increment => {
                count += 1;
                print("Count is now: %d\n", count);
            }
            GetValue(sender) => {
                send(sender, count);
            }
        }
    }
}

main() {
    let counter = spawn_actor(Counter);
    
    send(counter, Increment);
    send(counter, Increment);
    send(counter, Increment);
}
```

Compile and run:

```bash
./aetherc_new counter.ae counter.c
gcc counter.c ../runtime/*.c -o counter -lpthread
./counter
```

## Project Structure

```
aether/
├── src/                    # Compiler source
│   ├── aetherc_new.c      # Main compiler
│   ├── lexer.c            # Lexical analysis
│   ├── parser.c           # Syntax analysis
│   ├── ast.c              # Abstract syntax tree
│   ├── typechecker.c      # Type checking
│   └── codegen.c          # Code generation
├── runtime/               # Runtime library
│   ├── aether_runtime.h   # Runtime header
│   ├── actor.c            # Actor implementation
│   ├── scheduler.c        # Actor scheduler
│   └── memory.c           # Memory management
├── stdlib/                # Standard library
│   ├── io.ae              # I/O functions
│   ├── collections.ae     # Collection types
│   └── actors.ae          # Actor utilities
├── examples/              # Example programs
│   ├── hello_actors.ae    # Basic actors
│   ├── ring_benchmark.ae  # Performance test
│   └── supervisor.ae      # Fault tolerance
└── docs/                  # Documentation
    ├── LANGUAGE_SPEC.md   # Language specification
    └── GETTING_STARTED.md # This file
```

## Development Workflow

### 1. Write Aether Code

Use any text editor to write `.ae` files.

### 2. Compile

```bash
./aetherc_new your_program.ae output.c
```

### 3. Build C Code

```bash
gcc output.c ../runtime/*.c -o your_program -lpthread
```

### 4. Run

```bash
./your_program
```

## Debugging

### Compile-time Errors

The compiler will show errors with line numbers:

```
Type error at line 5, column 10: Undefined variable 'x'
```

### Runtime Errors

Runtime errors are printed to stderr:

```
Actor error: Actor not found
```

### Debug Output

Add debug prints to your actors:

```aether
receive(msg) {
    print("Actor received message\n");
    match msg {
        // handle message
    }
}
```

## Common Patterns

### Actor Communication

```aether
actor Sender {
    receive(msg) {
        match msg {
            Start => {
                let receiver = spawn_actor(Receiver);
                send(receiver, Hello("World"));
            }
        }
    }
}

actor Receiver {
    receive(msg) {
        match msg {
            Hello(name) => {
                print("Hello, %s!\n", name);
            }
        }
    }
}
```

### State Management

```aether
actor BankAccount {
    state int balance = 0;
    state string owner = "";
    
    receive(msg) {
        match msg {
            Deposit(amount) => {
                balance += amount;
            }
            Withdraw(amount) => {
                if (balance >= amount) {
                    balance -= amount;
                }
            }
            GetBalance(sender) => {
                send(sender, balance);
            }
        }
    }
}
```

### Error Handling

```aether
actor Worker {
    receive(msg) {
        match msg {
            DoWork => {
                if (rand() % 10 == 0) {
                    send(supervisor, WorkerFailed(self()));
                } else {
                    // Do work
                }
            }
        }
    }
}
```

## Performance Tips

1. **Minimize message copying**: Use references when possible
2. **Batch operations**: Group related messages
3. **Profile your code**: Use timing measurements
4. **Optimize actor granularity**: Not too small, not too large

## Next Steps

1. Read the [Language Specification](LANGUAGE_SPEC.md)
2. Try the examples in `examples/`
3. Explore the standard library in `stdlib/`
4. Build your own concurrent applications!

## Troubleshooting

### Compilation Errors

- Check syntax against the language specification
- Ensure all variables are declared
- Verify type compatibility

### Runtime Errors

- Check actor references are valid
- Ensure proper message handling
- Verify memory allocation

### Performance Issues

- Profile message passing overhead
- Check actor scheduling
- Optimize critical paths
