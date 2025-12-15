# Implementation Plan - State Machine Actors

**Status**: Implementation Phase  
**Start Date**: December 2024

## Phase 1: Language Features (Current)

### 1.1 Struct Types

**Goal**: Enable custom data types for actor state and messages

**Syntax**:
```aether
struct Point {
    int x;
    int y;
}

struct User {
    int id;
    string name;
    int age;
}
```

**Implementation Tasks**:
1. Add `TOKEN_STRUCT` to lexer
2. Add `AST_STRUCT_DEFINITION` node type
3. Parser: `parse_struct_definition()`
4. Type checker: Register struct types in symbol table
5. Codegen: Generate C struct definitions

**Files to Modify**:
- `src/tokens.h` - Add struct tokens
- `src/lexer.c` - Recognize `struct` keyword
- `src/ast.h` - Add struct AST nodes
- `src/parser.c` - Parse struct definitions
- `src/typechecker.c` - Type check struct fields
- `src/codegen.c` - Generate C structs

**Test Cases**:
```aether
// test_struct_basic.ae
struct Counter {
    int value;
}

main() {
    // TODO: Struct initialization syntax
    print("Struct compiled successfully\n");
}
```

### 1.2 Pattern Matching

**Goal**: Core feature for actor message handling

**Syntax**:
```aether
match message {
    Increment => { count += 1; }
    Decrement => { count -= 1; }
    GetValue(sender) => { send(sender, count); }
}
```

**Implementation Tasks**:
1. Add `TOKEN_MATCH`, `TOKEN_ARROW` to lexer
2. Add `AST_MATCH_EXPRESSION` node type
3. Parser: `parse_match_expression()`
4. Type checker: Verify all cases are exhaustive
5. Codegen: Generate switch/if-else chains

**Files to Modify**:
- Same as structs, plus match expression handling

### 1.3 Actor Syntax (Basic)

**Goal**: Define actors with state and receive blocks

**Syntax**:
```aether
actor Counter {
    state int count = 0;
    
    receive(msg) {
        match msg {
            Increment => { count += 1; }
        }
    }
}
```

**Implementation Tasks**:
1. Add `TOKEN_RECEIVE`, `TOKEN_STATE` to lexer
2. Add `AST_ACTOR_DEFINITION` node type
3. Parser: `parse_actor_definition()`
4. Type checker: Verify actor structure
5. Codegen: **Hand-written transformation** (not automatic yet)

## Phase 2: Manual State Machine Codegen

**Goal**: Prove the model works by hand-writing transformations

### 2.1 Simple Actor → C Struct

**Input** (Aether):
```aether
actor Counter {
    state int count = 0;
    receive(msg) { count += 1; }
}
```

**Output** (C - hand-written):
```c
typedef struct Counter {
    int count;
    int active;
    Mailbox mailbox;
} Counter;

void counter_step(Counter* self, Message* msg) {
    self->count += 1;
}
```

### 2.2 Benchmark Generated Code

Compare hand-written state machine performance to our benchmarks:
- Should match 125M msg/s throughput
- Should maintain 168 bytes/actor memory

If slower: investigate why before automating.

## Phase 3: Automatic State Machine Codegen (Future)

**Goal**: Compiler automatically generates state machines

### 3.1 Compiler Transformation Algorithm

```
For each actor definition:
1. Extract state variables → struct fields
2. Identify receive block → step function
3. Transform receive into:
   if (!receive(self, &msg)) { self->active = 0; return; }
4. Generate message handler (match → switch)
5. Lift local variables into struct (if needed)
```

### 3.2 Variable Lifting

**Challenge**: Local variables that persist across receives

**Solution**:
```aether
actor Example {
    receive(msg) {
        int temp = msg.value * 2;  // Used across await
        await send(other, temp);
        print("%d\n", temp);
    }
}

// Transform to:
struct Example {
    int count;
    int __temp;     // Lifted local
    int __state;    // State machine position
};
```

## Timeline

### Week 1-2: Structs
- Days 1-2: Lexer + Parser
- Days 3-4: Type checker
- Days 5-7: Codegen + Testing

### Week 3-4: Pattern Matching
- Days 8-9: Lexer + Parser
- Days 10-11: Type checker (exhaustiveness)
- Days 12-14: Codegen + Testing

### Week 5-6: Basic Actor Syntax
- Days 15-16: Parser for actor definitions
- Days 17-18: Type checker for actors
- Days 19-21: Hand-written state machine tests

### Month 2: Automated Codegen
- Weeks 5-8: Implement transformation algorithm
- Continuous benchmarking against hand-written

## Success Criteria

### Phase 1 Complete When:
- Structs compile to valid C structs
- Pattern matching generates correct C code
- Actor syntax parses correctly
- Can write and compile actor definitions

### Phase 2 Complete When:
- Hand-written state machine actors work
- Performance matches benchmarks (125M msg/s)
- Can demonstrate multiple actor types

### Phase 3 Complete When:
- Compiler auto-generates state machines
- Generated code matches hand-written performance
- Can compile and run actor examples from tests

## Current Status

- [x] Evidence gathered (EVIDENCE.md)
- [x] Documentation professional
- [ ] Struct implementation started
- [ ] Pattern matching started
- [ ] Actor syntax started
- [ ] State machine codegen started

## Next Immediate Steps

1. Add `TOKEN_STRUCT` to lexer
2. Test lexer recognizes `struct` keyword
3. Add `AST_STRUCT_DEFINITION` to AST
4. Implement `parse_struct_definition()`
5. Test with simple struct example
