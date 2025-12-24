# Aether Language Examples

This directory contains comprehensive examples demonstrating Aether's features, performance, and capabilities.

## Example Programs

### 1. `simple_demo.ae` - Basic Working Example
**Purpose**: Simple demonstration that compiles and runs
**Features**: Variables, arithmetic, control flow, loops, functions
**Usage**: Perfect for first-time users

```aether
main() {
    print("=== Aether Simple Demo ===\n");
    int x = 42;
    int y = 8;
    print("Variables: x = %d, y = %d\n", x, y);
    // ... more examples
}
```

### 2. `hello_world.ae` - Minimal Hello World
**Purpose**: Absolute minimal example
**Features**: Basic print statement
**Usage**: Quick verification that compiler works

### 3. `main_example.ae` - Comprehensive Example
**Purpose**: Complete feature demonstration
**Features**: All language constructs, control flow, loops, arrays
**Usage**: Full language reference

### 4. `performance_demo.ae` - Performance Testing
**Purpose**: Demonstrates performance capabilities with timing
**Features**: 
- Arithmetic performance testing
- String processing benchmarks
- Array operations timing
- Function call performance
- Memory allocation profiling

**Usage**: Performance evaluation and benchmarking

### 5. `feature_showcase.ae` - Complete Feature Demo
**Purpose**: Comprehensive feature demonstration with timing
**Features**:
- All basic types (int, float, bool, string)
- Control flow (if/else, switch, loops)
- Functions and recursion
- Arrays and string operations
- Performance timing
- Error handling
- Memory management

**Usage**: Complete language feature reference

### 6. `hello_actors.ae` - Actor System Demo
**Purpose**: Demonstrates actor-based concurrency
**Features**: Actor creation, message passing, state management
**Usage**: Concurrency and actor model examples

### 7. `ring_benchmark.ae` - Performance Benchmark
**Purpose**: Ring topology actor performance test
**Features**: Actor message passing in ring configuration
**Usage**: Concurrency performance testing

### 8. `supervisor.ae` - Actor Supervision Pattern
**Purpose**: Demonstrates fault tolerance with actors
**Features**: Actor supervision, error handling, recovery
**Usage**: Advanced actor patterns

## Running Examples

### Compile an Example
```bash
# Compile Aether to C
./build/aetherc.exe examples/simple_demo.ae examples/simple_demo.c

# Compile C to executable
gcc examples/simple_demo.c ../runtime/*.c -o simple_demo -lpthread

# Run the program
./simple_demo
```

### Quick Test
```bash
# Test the simple demo
./build/aetherc.exe examples/simple_demo.ae examples/simple_demo.c
gcc examples/simple_demo.c ../runtime/*.c -o simple_demo -lpthread
./simple_demo
```

## Example Categories

### **Basic Examples**
- `hello_world.ae` - Minimal example
- `simple_demo.ae` - Working demonstration

### **Feature Examples**
- `main_example.ae` - Complete language features
- `feature_showcase.ae` - Comprehensive feature demo

### **Performance Examples**
- `performance_demo.ae` - Performance testing with timing
- `ring_benchmark.ae` - Actor performance benchmark

### **Actor Examples**
- `hello_actors.ae` - Basic actor system
- `supervisor.ae` - Actor supervision patterns

## Performance Features Demonstrated

### Timing and Profiling
- **Execution time measurement** - Microsecond precision
- **Memory allocation profiling** - Track memory usage
- **Function call timing** - Measure function performance
- **Loop performance** - Iteration timing
- **String operation timing** - Text processing performance

### Benchmarking
- **Arithmetic operations** - CPU-intensive calculations
- **Array processing** - Data structure operations
- **String concatenation** - Text manipulation
- **Recursive algorithms** - Function call overhead
- **Memory management** - Allocation/deallocation patterns

## Expected Output

### Simple Demo Output
```
=== Aether Simple Demo ===
This is a working example that compiles and runs.

Variables: x = 42, y = 8
Arithmetic: x + y = 50, x * y = 336
x is greater than y
Counting from 1 to 5: 1 2 3 4 5
Function result: calculate(10, 20) = 50

=== Demo Complete ===
Aether is working perfectly!
```

### Performance Demo Output
```
=== Aether Performance Demonstration ===
This example showcases Aether's features with timing and profiling.

1. Arithmetic Performance Test:
   Sum of 1,000,000 integers: 499999500000
   Execution time: 15 ms

2. String Processing Test:
   Concatenated 10,000 strings
   Execution time: 8 ms

3. Array Operations Test:
   Processed array of 10,000 elements
   Array sum: 333283335000
   Execution time: 3 ms

4. Function Call Performance Test:
   Fibonacci(25): 75025
   Execution time: 12 ms

5. Memory Allocation Test:
   1,000 memory allocations/deallocations
   Execution time: 2 ms

=== Performance Demo Complete ===
```

## Notes

- All examples are designed to compile and run successfully
- Performance examples include timing measurements
- Examples demonstrate both basic and advanced features
- Actor examples show concurrency capabilities
- All examples include comprehensive comments and documentation




