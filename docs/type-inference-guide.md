# Aether Type Inference Guide

## How Type Inference Works

Aether implements a type inference system that automatically deduces types from context, reducing the need for explicit type annotations in most cases.

## Basic Inference

### From Literals

```aether
let x = 42;           // inferred: int
let pi = 3.14;        // inferred: float
let name = "Alice";   // inferred: string
let flag = true;      // inferred: bool
```

### From Expressions

```aether
let a = 10;
let b = 20;
let sum = a + b;      // inferred: int (both operands are int)

let x = 3.14;
let y = 2.0;
let result = x * y;   // inferred: float
```

### From Functions

```aether
// Return type inferred from return statement
add(a, b) {
    return a + b;     // If used with ints → int, if with floats → float
}

// Parameters inferred from usage
multiply(x, y) {
    return x * y;
}

main() {
    let n = add(10, 20);        // add inferred as: int -> int -> int
    let f = multiply(3.14, 2.0); // multiply inferred as: float -> float -> float
}
```

## Advanced Inference

### Struct Fields

```aether
struct Point {
    x,    // Type inferred from initialization
    y
}

struct Player {
    name,
    health,
    score
}

main() {
    let p = Point{ x: 10, y: 20 };  // x, y inferred as int
    
    let player = Player{
        name: "Alice",    // string
        health: 100,      // int
        score: 0          // int
    };
}
```

### Array Elements

```aether
let nums = [1, 2, 3, 4, 5];        // inferred: int[]
let names = ["Alice", "Bob"];       // inferred: string[]
let mixed_ok = [1.0, 2.0, 3.0];    // inferred: float[]
```

### Through Assignments

```aether
let x = 42;       // x: int
let y = x;        // y: int (inferred from x)
let z = y + 10;   // z: int (inferred from y + int)
```

## When to Use Explicit Types

While inference handles most cases, explicit types are useful for:

### 1. Function Signatures (Documentation)

```aether
// Explicit types make intent clear
calculate_damage(base: int, defense: int): int {
    return base - defense;
}

// vs inferred (less clear to reader)
calculate_damage(base, defense) {
    return base - defense;
}
```

### 2. Complex Scenarios

```aether
// When inference might be ambiguous
process_data(input: float[]) {
    // Explicit type ensures correct interpretation
}
```

### 3. Public APIs

```aether
// Export with explicit types for clarity
export calculate_score(kills: int, deaths: int, assists: int): float {
    return (kills + assists / 2.0) / (deaths + 1);
}
```

## Mixed Explicit/Inferred

You can mix explicit and inferred types:

```aether
// Some parameters explicit, some inferred
process(data, threshold: int) {
    return data > threshold;  // data type inferred from usage
}

// Explicit return, inferred parameters
format_message(user, message): string {
    return user + ": " + message;  // parameters inferred as string
}
```

## How Inference Works Internally

### Phase 1: Constraint Collection

The compiler walks the AST and collects type constraints:

```aether
let x = 42;
// Constraint: x must be int (from literal 42)

let y = x + 10;
// Constraint: y must be int (from x:int + 10:int)
```

### Phase 2: Constraint Solving

Constraints are propagated iteratively until all types are known:

```
Iteration 1: x = int (from literal)
Iteration 2: y = int (from x + int)
Done: All types resolved
```

### Phase 3: Validation

Once inferred, types are validated for consistency:

```aether
let x = 42;
x = "hello";  // ERROR: Can't assign string to int
```

## Edge Cases

### Ambiguous Types

```aether
// This FAILS - can't infer type without usage
let x;
x = get_value();  // ERROR: Type of x unknown

// Solution: Provide explicit type
let x: int;
x = get_value();  // OK
```

### Generic Functions

```aether
// Currently not supported - specify explicit types
identity(x: int): int {
    return x;
}

// Future: Generic type parameters
// identity<T>(x: T): T {
//     return x;
// }
```

## Performance Impact

Type inference happens at **compile-time only**:

- IMPLEMENTED Zero runtime overhead
- IMPLEMENTED Same generated C code as explicit types
- IMPLEMENTED Full type safety maintained
- IMPLEMENTED No performance difference

## Limitations

Current limitations (v1.0):

1. **No higher-rank types** - Functions must have monomorphic types
2. **No type classes** - No Haskell-style constraints
3. **Limited polymorphism** - No generic functions yet
4. **Struct fields** - Must initialize to infer type

## Best Practices

### IMPLEMENTED Do:
- Let inference handle obvious cases (literals, simple expressions)
- Use explicit types for public APIs and exports
- Use explicit types when it improves readability
- Mix explicit/inferred for balance

### NOT IMPLEMENTED Don't:
- Over-annotate everything (defeats the purpose)
- Rely on inference for complex logic (document with types)
- Leave ambiguous types un-annotated

## Examples

### Good: Minimal but Clear

```aether
struct Player {
    name: string,    // Explicit for documentation
    health,          // Inferred from init
    score            // Inferred from init
}

calculate_total(base, bonus) {  // Inferred from usage
    return base + bonus;
}

main() {
    let p = Player{ name: "Alice", health: 100, score: 0 };
    let total = calculate_total(p.score, 10);
}
```

### Better: Strategic Explicit Types

```aether
struct Player {
    name: string,
    health: int,     // Explicit: important field
    score: int       // Explicit: important field
}

// Explicit return: public function
calculate_total(base, bonus): int {
    return base + bonus;
}

main() {
    let p = Player{ name: "Alice", health: 100, score: 0 };
    let total = calculate_total(p.score, 10);
}
```

## Error Messages

When inference fails, Aether provides helpful errors:

```aether
let x;  // Type cannot be inferred

// Error: Type inference failed
//   Line 1: Variable 'x' has ambiguous type
//   help: Add type annotation: let x: int;
```

## Summary

- **Inference is powerful** - Handles 90% of cases automatically
- **Explicit types are documentation** - Use strategically
- **No performance cost** - Compile-time only
- **Fully type-safe** - Same guarantees as explicit types

Type inference gives Aether the expressiveness of ML/Haskell while maintaining C-level type safety and performance.

