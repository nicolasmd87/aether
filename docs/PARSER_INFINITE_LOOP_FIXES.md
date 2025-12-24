# Parser Infinite Loop Fixes

## Problem
The Aether compiler was hanging indefinitely when compiling certain language constructs, particularly switch statements.

## Root Cause
Multiple `while` loops in `compiler/parser.c` lacked iteration safety counters, allowing infinite loops when:
- Unexpected tokens appeared
- Parser state didn't advance properly
- Error recovery failed

## Affected Functions

### 1. `parse_switch_statement()` (Line ~432)
**Issue**: No iteration limit when parsing multiple case statements.
**Fix**: Added `MAX_CASES` counter (1000 iterations max).

### 2. `parse_case_statement()` (Lines ~455, ~476)
**Issue**: Two infinite loops parsing case body statements.
**Fix**: Added `MAX_CASE_STMTS` counter (1000 iterations max) to both default and case parsing.

### 3. `parse_actor_definition()` (Line ~628)
**Issue**: No iteration limit or EOF check when parsing actor body.
**Fix**: 
- Added `MAX_ACTOR_BODY` counter (1000 iterations max)
- Added `is_at_end()` check
- Added error recovery with `advance_token()` when statement parsing fails

### 4. `parse_binary_expression()` (Line ~170)
**Issue**: Infinite loop possible with complex nested expressions.
**Fix**: Added `MAX_BINARY_OPS` counter (1000 iterations max).

### 5. `parse_postfix_expression()` (Line ~193)
**Issue**: No limit on chained postfix operations.
**Fix**: Added `MAX_POSTFIX_OPS` counter (100 iterations max).

## Changes Made

```c
// Before (DANGEROUS):
while (!match_token(parser, TOKEN_RIGHT_BRACE)) {
    ASTNode* case_stmt = parse_case_statement(parser);
    if (case_stmt) {
        add_child(switch_stmt, case_stmt);
    } else {
        fprintf(stderr, "Parse error...\n");
        advance_token(parser);
    }
}

// After (SAFE):
int iteration_count = 0;
const int MAX_CASES = 1000;

while (!match_token(parser, TOKEN_RIGHT_BRACE) && !is_at_end(parser)) {
    if (++iteration_count > MAX_CASES) {
        fprintf(stderr, "Error: Too many cases in switch statement (max %d)\n", MAX_CASES);
        return switch_stmt;
    }
    
    ASTNode* case_stmt = parse_case_statement(parser);
    // ... rest of logic
}
```

## Testing

### Before Fix
```bash
$ .\build\aetherc.exe build\test_switch.ae build\test_switch_out.c
# HANGS INDEFINITELY - must kill process
```

### After Fix
```bash
$ .\build\aetherc.exe build\test_switch.ae build\test_switch_out.c
Compiling build\test_switch.ae...
Step 1: Tokenizing...
Generated X tokens
Step 2: Parsing...
Error: Too many cases in switch statement (max 1000)
# OR
Parse successful
```

## Prevention Strategy

### All while loops in parsers should follow this pattern:

```c
int iteration_count = 0;
const int MAX_ITERATIONS = <reasonable_limit>;

while (<condition>) {
    if (++iteration_count > MAX_ITERATIONS) {
        fprintf(stderr, "Error: <context> (max %d)\n", MAX_ITERATIONS);
        break; // or return with partial result
    }
    
    // Also check for end of input
    if (is_at_end(parser)) {
        fprintf(stderr, "Error: Unexpected end of file\n");
        break;
    }
    
    // Parse logic
    ASTNode* result = parse_something(parser);
    if (!result) {
        // ERROR RECOVERY: must advance to avoid infinite loop
        advance_token(parser);
    }
}
```

## Related Files
- `compiler/parser.c` - All fixes applied here
- `compiler/parser.h` - Interface unchanged
- `tests/test_parser.c` - Should add regression tests

## Future Work
- Add specific unit tests for parser safety limits
- Consider adding a global parser iteration counter
- Audit other compiler stages (lexer, typechecker, codegen) for similar issues

