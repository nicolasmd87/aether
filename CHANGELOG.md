# Changelog

All notable changes to Aether are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

**Workflow**: New changes go under `## [0.18.0]`. When a PR merges to `main`,
the release pipeline automatically replaces `[current]` with the next version
number (e.g. `[0.18.0]`) before tagging the release.

## [0.22.0]

### Added

- **Release archive CI test (`test-release-archive`)**: New Makefile target and CI step [9/9] that packages a tarball exactly like the release pipeline, extracts it, and verifies `ae init` + `ae run` work from the extracted layout — catches archive structure bugs that `test-install` (which tests `install.sh`) would miss
- **4 regression tests for CLI helper battle-testing**: `test_actor_print_char.ae` (print_char/escapes in actor handlers, self-send animation), `test_box_drawing.ae` (ASCII boxes, ANSI escapes, progress bars, tab tables, nested boxes), `test_interp_escape_combo.ae` (10 hex/octal + interpolation combos), `test_file_io_char_return.ae` (file I/O roundtrip, char* returns, append, cleanup)

### Fixed

- **Stdlib functions returned `AetherString*` instead of `char*`**: All stdlib modules (fs, io, json, net) returned opaque `AetherString*` pointers from functions like `file_read_all()`, `io_read_file()`, `json_stringify()`, `tcp_receive()` — but Aether's native string type is `const char*`, so codegen generated `printf("%s", ...)` which interpreted the struct pointer as a string, producing garbage output on all platforms. Changed all public stdlib APIs to return `char*` directly; module.ae declarations updated from `-> ptr` to `-> string`
- **`file_write` / `file_size` ABI mismatch**: `file_write()` C signature used `size_t length` (8 bytes on 64-bit) but module.ae declared `int` (4 bytes) — misaligned stack on ARM64. `file_size()` returned `size_t` but module declared `int`. Fixed C signatures to use `int` matching the module declarations
- **`ae version install` extracted only one directory from release archives**: The POSIX extraction logic assumed release archives had a single wrapper directory and used `ls -d tmp/*/ | head -1` to find it — but release archives contain `bin/`, `lib/`, `share/`, `include/` at root with no wrapper. `head -1` picked only one directory (e.g. `bin/`), so `lib/libaether.a` and `share/aether/` were lost, causing "flat layout" detection and compilation failures when running `ae run` on an installed version
- **`ae version install` incomplete install detection**: Pre-existing version directories without actual binaries were treated as complete installations — now probes for `aetherc` binary and reinstalls if missing
- **Windows `ae version use` missing lib/share**: Only copied `bin/` subdirectory contents — now copies the entire version directory so `lib/`, `include/`, `share/` are available

### Changed

- **`make ci` expanded to 9 steps**: Added `test-release-archive` as step [9/9] — every CI run now verifies both `install.sh` and release archive extraction paths end-to-end

## [0.20.0]

### Added

- **6 new regression tests**: `test_actor_ref_message_fields.ae` (actor ref routing through messages), `test_format_specifier_typecheck.ae` (format specifier selection for all types), `test_stdlib_file_module.ae` (file module read/write), `test_stdlib_resolution.ae` (stdlib module resolution), `test_string_escape_sequences.ae` (escape sequence roundtrips), `test_win32_actor_self_scheduling.ae` (Windows actor self-scheduling)
- **2 new examples**: `actor-ref-routing.ae` (passing actor refs through messages), `self-scheduling.ae` (actor self-send patterns), `cross-platform-strings.ae` (cross-platform string handling)

### Fixed

- **Windows 11 thread compatibility**: Fixed `aether_thread.h` for Windows 11 — proper `CONDITION_VARIABLE` initialization and `SleepConditionVariableCS` usage
- **`--emit-c` flag handling**: Fixed flag parsing in `aetherc` for `--emit-c` output mode
- **Scheduler thread startup on Windows**: Added `scheduler_ensure_threads_running()` call in Windows codepath for actor self-scheduling

### Changed

- **`install.sh` improvements**: Better error handling, cleaner output, improved readline detection

## [0.19.0]

### Added

- **`--emit-c` compiler flag**: `aetherc --emit-c file.ae` prints the generated C code to stdout — useful for debugging codegen, inspecting optimizer output, and verifying MSVC compatibility guards
- **20 new integration tests** (46→66):
  - `test_print_null.ae` — 5 tests for `print`/`println` with NULL string values
  - `test_match_complex.ae` — 8 tests for match statement edge cases (NULL strings, many arms, sequential matches)
  - `test_series_long.ae` — 4 tests for series collapse optimizer with `long` (int64) types
  - `test_long_type.ae` — 7 tests for `long` type declarations, arithmetic, comparisons, and printing
  - `test_functions_advanced.ae` — 5 tests for recursive functions, nested calls, deep call chains
  - `test_while_edge_cases.ae` — 6 tests for zero-iteration loops, break, continue, nested loops
  - `test_nested_expressions.ae` — 7 tests for operator precedence, deep nesting, unary ops, if-expressions
  - `test_string_edge_cases.ae` — 7 tests for empty strings, escapes, interpolation with arithmetic
  - `test_type_coercion.ae` — 9 tests for long arithmetic, integer division, boolean logic, modulo
  - `test_control_flow.ae` — 6 tests for else-if chains, break/continue, nested loops, range-for, match default
  - `test_optimizer_booleans.ae` — 6 tests for `if true`/`if false` dead code elimination, constant folding type preservation
  - `test_reserved_words.ae` — 5 tests for C reserved word collision (functions named `double`, `auto`, `register`, `volatile`)
  - `test_edge_cases.ae` — 6 tests for nested loops, break, while-in-for, many variables, wildcard match, deep arithmetic
  - `test_actor_self_send.ae` — 4 tests for actor self-send pattern (finite loop, multiple self-sends, animation/stop, immediate exit)
  - `test_math_stdlib.ae` — 8 tests for math namespace functions (sqrt, abs, min/max, clamp, sin/cos, pow, floor/ceil/round, random)
  - `test_operator_precedence.ae` — 8 tests for operator precedence (mul/div before add/sub, parentheses, modulo, comparisons, logical ops, negation)
  - `test_string_compare_null.ae` — 3 tests for string comparison NULL safety (normal comparison, ordering, empty strings)
  - `test_nested_functions.ae` — 5 tests for nested/chained function calls (deep nesting, clamp composition, calls in conditions/arithmetic)
  - `test_logical_ops.ae` — 5 tests for logical operators (AND, OR, NOT, with comparisons, complex combinations)
  - `test_defer_advanced.ae` — 3 tests for defer edge cases (LIFO ordering, nested scopes, conditional defer)
- **3 new examples**: `recursion.ae` (factorial, fibonacci, GCD, fast exponentiation), `long-arithmetic.ae` (64-bit values, nanosecond timing, large multiplication), `string-processing.ae` (interpolation, escapes, multi-type printing)
- **17 new tests** (66→83):
  - `test_actor_communication.ae` — 4 tests for actor-to-actor messaging (bidirectional ping-pong, multi-phase wait_for_idle, actor ref in message fields)
  - `test_ask_reply.ae` — ask/reply pattern tests
  - `test_defer_loops.ae` — defer inside loops and nested scopes
  - `test_escape_hex_octal.ae` — hex (`\xNN`) and octal (`\NNN`) escape sequences
  - `test_escape_sequences.ae` — escape sequence coverage (hex, octal, interpolated strings, print_char)
  - `test_extern_functions.ae` — extern function declarations and calls
  - `test_float_operations.ae` — float arithmetic, comparisons, and printing
  - `test_io_operations.ae` — io module read/write/append/exists/delete
  - `test_list_operations.ae` — list create/add/get/size/remove operations
  - `test_map_operations.ae` — map put/get/has/remove/size operations
  - `test_pattern_guards.ae` — pattern matching with guard clauses
  - `test_print_char.ae` — print_char builtin for ASCII byte output
  - `test_print_format.ae` — print/println format specifiers for all types
  - `test_scope_shadowing.ae` — variable shadowing across scopes
  - `test_string_stdlib.ae` — string module functions (length, contains, split, etc.)
  - `test_struct_usage.ae` — struct creation, field access, nested structs
  - `test_typed_params.ae` — typed function parameters and return types
- **5 new examples**: `formatted-output.ae` (print formatting), `escape-codes.ae` (ANSI escape sequences), `ascii-art.ae` (character art with print_char), `io-demo.ae` (file I/O), `log-demo.ae` (logging module)
- **`print_char()` builtin**: `print_char(65)` emits a single byte by ASCII value — enables ANSI escape codes and character-level output without extern functions
- **Hex and octal escape sequences**: `\xNN` (hex) and `\NNN` (octal) in string literals and interpolated strings — enables ANSI terminal control codes like `"\x1b[1;32m"` directly in Aether strings

### Changed

- **Locality-aware actor placement**: Actors are now placed on the caller's core at spawn time instead of round-robin distribution. Main thread spawns default to core 0, keeping top-level actor groups co-located for efficient local messaging. Actors spawned from within actor handlers inherit the parent's core. This benefits tightly-coupled communication patterns such as ring and chain topologies where actors communicate with their immediate neighbors.
- **Aggressive message-driven migration**: Cross-core sends now set `migrate_to` to the sender's core directly, rather than migrating to the lower of the two core IDs. This produces faster convergence for communicating actor pairs.
- **Targeted migration checks**: The scheduler now checks migration hints immediately after processing messages from the coalesce buffer (O(batch_size)), in addition to the existing full actor scan during idle periods. This provides faster migration response without adding overhead to the idle scan path.
- **Non-destructive `scheduler_wait()`**: `scheduler_wait()` (backing `wait_for_idle()`) now only waits for quiescence without stopping or joining threads — programs can call `wait_for_idle()` multiple times to synchronize between phases of actor messaging. New `scheduler_shutdown()` handles final teardown (wait + stop + join) and is emitted once at program exit.
- **`Message.payload_int` widened to `intptr_t`**: Changed from `int` to `intptr_t` in the runtime Message struct — prevents 64-bit pointer truncation when actor refs are passed through single-int message fields on 64-bit platforms

### Fixed

- **Batch send buffer overflow on actor migration**: `scheduler_send_batch_flush()` re-read each actor's `assigned_core` at flush time, but the per-core counts (`by_core[]`) were recorded at `batch_add` time. If an actor migrated between buffering and flushing, the count/core mismatch caused the radix-sort to write past the end of the `sorted_actors[]` stack buffer — a stack buffer overflow confirmed by AddressSanitizer. Fixed by snapshotting `assigned_core` once per message at flush time and recomputing per-core counts from the consistent snapshot.
- **`printf("%s", NULL)` crash from `print(getenv(...))`**: Printing a NULL string (e.g. from `getenv()` on an unset variable) caused undefined behavior — now uses `_aether_safe_str()` inline helper in generated C that returns `"(null)"` for NULL pointers; helper evaluates the expression exactly once (no double-evaluation of side-effecting expressions like function calls)
- **Series collapse optimizer int32 overflow**: The closed-form formula `N*(N-1)/2` emitted by the loop optimizer overflowed for large N (e.g. 100000) because it used int32 arithmetic — now casts to `(int64_t)` for both linear-sum and constant-addend formulas; the int64 result correctly truncates back to the variable's declared type
- **Match expression evaluated multiple times**: `match (expr) { ... }` re-evaluated `expr` for every arm — if `expr` was a function call, the function ran N times with potential side effects; now emits `T _match_val = expr;` once and uses `_match_val` in all arm comparisons
- **Match `strcmp` crash on NULL strings**: String match arms used bare `strcmp()` which crashes on NULL input (e.g. `match (getenv("X")) { "a" -> ... }`) — now emits `_match_val && strcmp(_match_val, ...)` with proper NULL guard
- **Triple-evaluation of send target in actor handlers**: Inside actor receive handlers, `actor ! Msg { ... }` evaluated the target actor expression 3 times (condition check + local send + remote send) — if the target was a function call, it ran 3 times; now stores in `ActorBase* _send_target` once
- **NULL dereference in compound assignment codegen**: `stmt->children[0]->value` accessed without null check in `AST_COMPOUND_ASSIGNMENT` handler — added guard for `stmt->children[0] && stmt->children[0]->value`
- **NULL dereference on return type with `defer`**: `stmt->children[0]->node_type` could be NULL when type inference failed, causing crash in `get_c_type()` — added fallback to `int` when `node_type` is NULL or unresolved
- **`print`/`println` string literal overhead**: String literals (never NULL) were unnecessarily wrapped in `_aether_safe_str()` — literals now use `puts()` or `printf()` directly; only runtime string values go through the NULL-safe path
- **Message pool pre-allocation ignores OOM**: `message_pool_create()` called `malloc(256)` in a loop without checking return values — a failed allocation stored NULL in the pool, causing later NULL dereference; now cleans up and returns NULL on allocation failure
- **Overflow buffer out-of-bounds access**: `overflow_append(target, ...)` did not validate `target` against array bounds — an invalid core ID could write beyond `tls_overflow[MAX_CORES+1]`; now returns early with diagnostic on out-of-range target
- **Partial `realloc` failure corrupts overflow buffer**: Two sequential `realloc()` calls for `actors` and `msgs` arrays — if the second failed, `actors` pointed to the new allocation while `msgs` still pointed to the old (freed) memory; now updates `b->actors`/`b->msgs` immediately after each successful realloc so `abort()` on the second failure leaves consistent state
- **`strdup` return unchecked in message registry**: `register_message_type()` used `malloc()` and `strdup()` without checking return values — NULL results propagated silently, causing later `strcmp()` crash; now returns `-1` on allocation failure
- **Parser `is_at_end()` NULL dereference**: `peek_token()` returns NULL when past end of tokens, but `is_at_end()` immediately dereferenced it to check `->type == TOKEN_EOF` — now checks for NULL first
- **Parser `peek_ahead` negative index**: `peek_ahead(parser, -N)` could pass a negative `pos` to the token array — now returns NULL for negative positions
- **Parser direct token array access without bounds check**: Two call sites used `parser->tokens[current_token + 1]` directly — replaced with bounds-checked `peek_ahead(parser, 1)`
- **Lexer buffer overflow in error token**: Unknown characters created tokens with `&c` (pointer to single stack char) which `create_token()` passed to `strlen()`/`strcpy()` — now uses properly null-terminated 2-char array
- **Parser unchecked `realloc` in string interpolation**: Two `realloc()` calls during interpolation literal buffer growth did not check for NULL — realloc failure caused NULL pointer write; now checks and returns partial result on failure
- **Parser format string bugs**: Four `parser_message()` calls contained `%d` format specifiers without corresponding arguments — replaced with literal numbers
- **Lexer `create_token` missing malloc checks**: `malloc()` for token struct and value string not checked — now returns NULL on allocation failure
- **Parser `create_parser` missing malloc check**: `malloc(sizeof(Parser))` not checked — now returns NULL on failure
- **Type checker NULL dereference on `symbol->type`**: Module aliases have `symbol->type = NULL`, but 6 call sites in typechecker and type inference used `symbol->type` without NULL guards — crash when identifiers resolved to module aliases; added `!msg_sym->type ||` guards on send validation, `symbol->type ? ... : create_type(TYPE_UNKNOWN)` on identifier/function/compound-assignment type assignment
- **Missing semicolons in `sleep()` codegen**: Generated `Sleep()`/`usleep()` calls inside `#ifdef _WIN32`/`#else` blocks lacked semicolons — the preprocessor removed the `#endif` line leaving bare function calls without terminators; added `;` after both paths
- **Non-printable characters in string literal codegen**: Control characters (0x00-0x1F except \\n/\\t/\\r, and 0x7F) were emitted as raw bytes in generated C strings — could produce invalid C or compiler warnings; now escaped as `\\xHH`
- **Command injection in `ae test` and `ae examples`**: User-supplied directory paths passed directly to `popen(find "..." ...)` without validation — shell metacharacters in paths could execute arbitrary commands; now validates paths reject `` ` ``, `$`, `|`, `;`, `&` and other shell metacharacters
- **Unsafe `strcpy` in toolchain discovery**: `strcpy(tc.compiler, standard_paths[i])` used without bounds checking — replaced with `strncpy` + explicit null termination
- **`ftell()` failure causes `malloc(-1)` in `ae add`**: `ftell()` returns -1 on error, then `malloc(sz + 1)` with `sz=-1` causes `malloc(0)` and subsequent `fread()` with negative size (undefined behavior) — now checks `sz < 0` and returns error
- **TOML parser `strdup` NULL checks**: Three `strdup()` calls for section names and key/value pairs did not check for NULL returns — allocation failure stored NULL pointers later dereferenced by `strcmp()`; now checks all `strdup` returns and rolls back partial entries
- **TOML parser `realloc` capacity tracking**: Section capacity was incremented before `realloc()` — if `realloc` failed, the capacity variable was already wrong, causing OOB access on next insert; now only updates capacity after successful `realloc`
- **TOML parser `toml_get_value` NULL dereference**: `strcmp()` called on section/key names without NULL check — if a section had NULL name (from failed `strdup`), lookup would crash; added NULL guards in comparison loops
- **`if true { ... }` body silently eliminated by optimizer**: The dead code optimizer called `atof("true")` which returns `0.0`, treating `true` as falsy and removing the entire `if true` body — added `is_constant_condition()` helper that handles boolean literals separately from numeric constants; `true` → truthy, `false` → falsy
- **Constant folding always produced `TYPE_FLOAT`**: `create_numeric_literal()` unconditionally set `TYPE_FLOAT` for all folded constants, so `3 + 4` produced a float `7.0` — now takes an `is_int` parameter and preserves `TYPE_INT` when both operands are integers
- **C reserved word collision in function names**: Aether functions named `double`, `auto`, `register`, `volatile`, etc. generated invalid C because the function name is a C keyword — added `safe_c_name()` that prefixes colliding names with `ae_`; applied to function definitions, forward declarations, and call sites; `extern` functions are excluded since they refer to actual C symbols
- **AST `add_child()` silent failure on OOM**: `realloc()` failure in `add_child()` silently returned without adding the child — a corrupted AST caused unpredictable codegen; now calls `exit(1)` on allocation failure
- **Pattern variable mapping array bounds**: Guard clause codegen used a fixed `mapping[32]` array without bounds checking — more than 32 pattern variables silently overwrote the stack; now guards against overflow
- **Type checker namespace leak**: `imported_namespaces[]` strings allocated via `strdup()` were never freed on early return from `typecheck_program()` — added cleanup on all return paths
- **Extern registry unchecked `realloc`**: `register_extern_func()` updated capacity before confirming `realloc()` success — a failed realloc left capacity wrong and `extern_registry` pointing to freed memory; now only updates after success
- **Optimizer NULL checks in tail call detection**: `optimize_tail_calls()` accessed `node->children[i]` without NULL guards — a NULL child caused segfault during recursive optimization; added NULL checks before recursing
- **Runtime non-atomic message counters**: `messages_sent` and `messages_processed` in the scheduler were plain `uint64_t` read from the main thread while being written by worker threads (data race) — changed to `_Atomic uint64_t`
- **Codegen NULL dereference in `AST_IDENTIFIER`**: `strcmp(expr->value, ...)` in actor state variable lookup crashed if `expr->value` was NULL — added NULL guard before the loop
- **Codegen NULL dereference in `AST_ACTOR_REF`**: `strcmp(expr->value, "self")` crashed on NULL value — added NULL guard
- **Codegen NULL dereference in match arm iteration**: `match_arm->type` dereferenced without checking if `match_arm` was NULL — added `!match_arm ||` guard
- **Codegen NULL dereference in reply statement**: `reply_expr->value` passed to `lookup_message()` without NULL check — added `&& reply_expr->value` guard
- **Codegen NULL message name in error comments**: Two error-path `fprintf` calls used `message->value` which could be NULL — added ternary fallback to `"<?>"`
- **float/double ABI mismatch in std.math**: All 18 math functions (`sqrt`, `sin`, `cos`, `pow`, `floor`, `ceil`, `round`, `abs_float`, etc.) used C `float` with `f`-suffixed implementations (`sqrtf`, `sinf`, etc.) but Aether's `float` type maps to C `double` — caused wrong return values on ARM64 (e.g. `math.sqrt(16.0)` returned `0.0`); changed all signatures and implementations to `double`
- **float/double mismatch in `io_print_float`**: `io_print_float` took `float` parameter but received `double` from compiled Aether code — changed to `double`
- **`log_get_stats()` struct-by-value ABI mismatch**: Function returned `LogStats` struct by value but Aether's codegen expects pointer returns for non-primitive types — changed to return `LogStats*` (pointer to static storage)
- **Unary `!` operator precedence bug**: `!(a || b)` generated `!a || b` in C — the `!` only applied to the first operand because `AST_UNARY_EXPRESSION` codegen did not parenthesize complex subexpressions; now wraps binary/unary subexpressions in parentheses
- **String comparison NULL crash**: `strcmp()` in string equality/ordering codegen crashed on NULL string values — wrapped both operands with `_aether_safe_str()` to return `""` for NULL
- **io module.ae param type mismatches**: 8 functions (`io_print`, `io_print_line`, `io_read_file`, `io_write_file`, `io_append_file`, `io_file_exists`, `io_delete_file`, `io_file_info`) declared `ptr` parameters in module.ae but the C implementations take `const char*` — changed to `string` to match
- **collections module.ae map key type mismatch**: `map_put`, `map_get`, `map_has`, `map_remove` declared `ptr` for key parameter but the C implementations take `const char*` — changed to `string`
- **Lexer silently accepts unterminated strings**: Strings missing a closing `"` were lexed without error — now returns `TOKEN_ERROR` with "unterminated string literal"
- **Lexer silently accepts unterminated multi-line comments**: `/* ...` without `*/` was silently ignored — now prints error to stderr
- **Lexer missing `\0` escape sequence**: `\0` in string literals was not recognized — added null byte escape
- **Array index type not validated**: Array access like `arr["hello"]` passed through the type checker without error — added validation that array indices must be `int` or `long`
- **Extern function argument types not validated**: Calling an extern function with wrong argument types (e.g. passing `int` to a `string` parameter) produced no type error — added type validation for extern function arguments using declared parameter types
- **Type inference missing `long` (int64) arithmetic promotion**: `infer_from_binary_op()` only handled `TYPE_INT` and `TYPE_FLOAT` — mixed `int`/`long` arithmetic silently inferred as `TYPE_INT` instead of `TYPE_INT64`; now promotes to `TYPE_INT64` when either operand is int64
- **Type inference memory leak in binary expression**: `node->node_type` was overwritten without freeing the old type when reassigning from `infer_from_binary_op()` — added `free_type()` before reassignment
- **Type inference NULL guard in `has_unresolved_types`**: `ctx->constraints` could be NULL if no constraints were collected — added defensive NULL check
- **Parser match statement now supports optional parentheses**: `match val { ... }` and `match (val) { ... }` both work — parentheses are consumed if present but no longer required
- **Parser 10 unchecked `expect_token` calls**: Missing tokens (colons, parens, arrows, braces) caused the parser to continue with corrupted state — added return/break on failure in `parse_for_loop`, `parse_case_statement` (2x), `parse_match_statement` (3x), `parse_match_case`, `parse_message_definition`, `parse_reply_statement`, `parse_message_constructor`, `parse_struct_pattern` (2x)
- **Parser NULL dereference in `parse_for_loop`**: `name->value` accessed without checking if `expect_token(TOKEN_IDENTIFIER)` returned NULL — added NULL guard
- **Parser unchecked `malloc` in for-loop children**: Two `malloc(4 * sizeof(ASTNode*))` calls for for-loop child arrays did not check for NULL — OOM caused NULL pointer dereference when assigning children; now checks and returns NULL
- **`print()` not flushing stdout**: `print(".")` in a loop did not show dots immediately because `printf` buffers partial lines — now emits `fflush(stdout)` after every `print()` call in generated C
- **`self` in actor handler generated undefined `aether_self()`**: The codegen for `AST_ACTOR_REF` with value `"self"` emitted `aether_self()` which doesn't exist in the runtime — now emits `(ActorBase*)self` when inside an actor handler context
- **Actor self-send crashed in main-thread mode**: `self ! Message {}` from inside a handler in main-thread mode caused either a double-free (recursive `g_skip_free` flag corruption) or an infinite blocking loop (drain loop) or silent message loss (scheduler threads never started) — now properly transitions out of main-thread mode on self-send: disables `main_thread_mode`, clears `main_thread_only`, and starts scheduler threads on demand via `scheduler_ensure_threads_running()` so self-sent messages are processed asynchronously by the scheduler
- **`scheduler_wait()` destroyed threads after first call**: `scheduler_wait()` (backing `wait_for_idle()`) called `scheduler_stop()` + `pthread_join()`, permanently destroying scheduler threads — second call to `wait_for_idle()` hung forever because no threads existed to process messages; split into non-destructive `scheduler_wait()` (quiescence only) and `scheduler_shutdown()` (final teardown at program exit)
- **Actor refs truncated in message fields on 64-bit**: Passing an actor ref through a single-int message field (e.g. `PingWithRef { sender_ref: counter }`) truncated the 64-bit pointer to 32-bit `int` — `Message.payload_int` widened to `intptr_t`, codegen emits `intptr_t` for single-int message struct fields and proper `(intptr_t)` casts when storing actor refs
- **`void*`/`int` comparison warnings in generated C**: Comparing `list.get()` return (`void*`) with an `int` value produced C compiler warnings — codegen now auto-detects ptr/int mixed comparisons and emits `(intptr_t)` cast on the pointer side
- **`set_clear()` / `hashmap_clear()` left stale entries**: `hashmap_clear()` only set `occupied = false` per entry, leaving stale PSL, hash, key, and value data that could cause phantom entries on reinsertion — now uses `memset` to zero the entire entries array after freeing keys/values
- **`main_thread_sent` counter not reset between scheduler lifecycles**: The atomic `main_thread_sent` counter was never reset in `scheduler_init()` — stale sent count from a previous lifecycle caused `count_pending_messages()` to return permanently non-zero, spinning `scheduler_wait()` forever in C unit tests
- **Type inference not propagating parameter types through call chains**: Function parameters resolved from one call site were not propagated when the function was called from other sites — added constraint propagation pass that infers parameter types from all call sites

## [0.18.0]

### Fixed

- **Type checker did not validate message constructor field types**: Message constructors like `Greet { name: 42 }` were accepted even when the field was declared as `string` — now validates each field against the message definition's declared types
- **Missing `getpid()` header on Windows**: `<process.h>` is needed for `getpid()` on Windows, `<unistd.h>` on POSIX — added proper platform-guarded includes

## [0.17.0]

### Added

- **`exit()` builtin**: `exit(code)` terminates the program with the given exit code — no `extern` declaration needed; `exit()` with no argument defaults to 0
- **`--dump-ast` compiler flag**: `aetherc --dump-ast file.ae` prints the parsed AST tree and exits without generating C code — useful for debugging parser behavior and understanding program structure
- **`-g` debug symbols in dev builds**: `ae run` now compiles with `-g` flag, enabling `gdb`/`lldb` debugging on crashes without requiring manual gcc flags
- **`NO_COLOR` and `isatty()` support**: Error/warning output respects the `NO_COLOR` environment variable ([no-color.org](https://no-color.org/)) and automatically disables ANSI colors when stderr is not a terminal (e.g. piped to a file or CI log)
- **`null` keyword**: `null` is now a first-class literal typed as `ptr` — eliminates the need for C `null_ptr()` helpers; `x = null` and `if x == null` work as expected
- **Bitwise operators**: `&`, `|`, `^`, `~`, `<<`, `>>` with C-compatible precedence — enables bitmask flags, hash functions, and protocol parsing without C shim functions
- **Top-level constants**: `const NAME = value` at module scope — codegen emits `#define`; supports int, float, and string constant values
- **Compound assignment operators**: `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=` — emit directly to C compound assignments; work with both regular variables and actor state
- **Hex, octal, and binary numeric literals**: `0xFF`, `0o755`, `0b1010` with underscore separators (`0xFF_FF`, `0b1111_0000`) — converted to decimal at lex time so all downstream code (atoi, codegen) works unchanged
- **If-expressions**: `result = if cond { a } else { b }` produces a value — codegen emits C ternary `(cond) ? (a) : (b)`; works inline in function arguments and assignments
- **Range-based for loops**: `for i in 0..n { body }` — desugars to C-style `for (int i = start; i < end; i++)` at parse time; supports variable bounds and nests with other loop forms
- **Multi-statement arrow function bodies**: `f(x) -> { stmt1; stmt2; expr }` — the last expression is the implicit return value; supports intermediate variables, if/return, and loops inside arrow bodies

### Fixed

- **`state` keyword context-awareness**: `state` is now a regular identifier outside actor bodies — previously it was globally reserved, preventing common variable names like `state = 42` in non-actor code
- **int(0) to ptr type widening**: Variables initialized with `0` and later assigned a `ptr` value no longer cause type mismatch errors — `int` / `ptr` compatibility added for null-initialization patterns
- **Sibling if block variable scoping**: Reusing a variable name in sibling `if` blocks no longer causes "undeclared identifier" errors in generated C — each block now gets a fresh scope with `declared_var_count` properly restored after the entire if/else chain
- **String interpolation returns string pointer**: `"text ${expr}"` now produces a heap-allocated `char*` (via `snprintf` + `malloc`) instead of a `printf()` return value (`int`) — interpolated strings can now be passed to `extern` functions expecting `ptr` arguments; `print`/`println` retain the optimized `printf` path
- **`ae.c` buffer safety**: All `strncpy` calls into stack buffers now have explicit null termination; `get_exe_dir` buffer handling hardened against truncation
- **`ae.c` mixed-path separators on Windows**: `get_basename()` now handles both `/` and `\` path separators correctly, preventing incorrect toolchain discovery on Windows
- **`ae.c` source fallback completeness**: Added missing `std/io/aether_io.c` to both dev-mode and installed-mode source fallback lists; installed-mode include flags now cover all `std/` subdirectories and `share/aether/` fallback paths
- **`ae.c` temp directory consistency**: All temp file operations now use `get_temp_dir()` which checks `$TMPDIR` on POSIX and `%TEMP%` on Windows, instead of inconsistent inline checks
- **`install.sh` portability**: Changed shebang from `#!/bin/sh` to `#!/usr/bin/env bash` (script uses `local` keyword); added `cd "$(dirname "$0")"` so installer works when invoked from any directory
- **`install.sh` header completeness**: Added `std` and `std/io` directories to the header installation loop — `ae.c` generates `-I` flags for these paths
- **`install.sh` readline probe**: Uses the detected compiler (`$CC`) instead of hardcoded `gcc` for the readline compilation test
- **`install.sh` re-install handling**: Running installer again now updates the existing `AETHER_HOME` value via `sed` instead of skipping silently
- **Release archives missing headers**: Both Unix and Windows release packaging now include the `include/aether/` directory with all runtime and stdlib headers, matching the layout expected by `ae.c` in installed mode
- **Release pipeline command injection**: Commit message in the bump job is now passed via `env:` block instead of inline `${{ }}` expansion, preventing shell injection through crafted commit messages
- **`find` command quoting**: `ae test` discovery now quotes the search directory in `find` commands, preventing failures on paths with spaces
- **SPSCQueue codegen struct layout mismatch**: Codegen emitted `SPSCQueue spsc_queue` (3KB by-value) but runtime `ActorBase` uses `SPSCQueue* spsc_queue` (8-byte pointer) — caused struct layout corruption when the scheduler cast actor pointers; fixed codegen to emit pointer type
- **Codegen emits MSVC-incompatible GCC-isms**: Generated C contained bare `__thread`, `__attribute__((aligned(64)))`, `__attribute__((hot))`, and `__asm__` — replaced with portable `AETHER_TLS`, `AETHER_ALIGNED`, `AETHER_HOT`, and `AETHER_CPU_PAUSE()` macros from `aether_compiler.h`; generated code now includes `aether_compiler.h` for cross-platform builds
- **GCC statement expressions in generated C**: Three uses of `({ ... })` (clock_ns, string interpolation, ask operator) prevented MSVC compilation — each now guarded with `#if AETHER_GCC_COMPAT` with portable helper functions (`_aether_clock_ns`, `_aether_interp`, `_aether_ask_helper`) emitted in the generated preamble for the `#else` path
- **Computed goto dispatch in generated C**: Actor message dispatch used `&&label` / `goto *ptr` (GCC/Clang extension) — now guarded with `#if AETHER_GCC_COMPAT`; the `#else` path emits an equivalent `switch(_msg_id)` with `case N: goto handle_Msg;` entries; fast computed-goto path preserved for GCC/Clang
- **Message struct `__attribute__((aligned(64)))` unguarded**: Large message structs emitted bare GCC alignment attribute — now guarded with `__declspec(align(64))` for MSVC and `__attribute__((aligned(64)))` for GCC/Clang
- **`AETHER_GCC_COMPAT` macro override support**: Both the generated C preamble and `aether_compiler.h` now use `#ifndef AETHER_GCC_COMPAT` so users and tests can force a specific value via `-D` flag or early `#define`
- **std.http segfault on string arguments**: `http_get`, `http_post`, `http_put`, `http_delete` took `AetherString*` but the compiler passed raw `const char*` string literals — dereferencing `url->data` on a plain `char*` caused immediate segfault; all four functions changed to accept `const char*`
- **std.io, std.net, std.json, std.collections `AetherString*` mismatch**: 37 stdlib functions across IO, networking, JSON, and collections modules took `AetherString*` parameters but received `const char*` from compiled `.ae` code — all public APIs changed to `const char*`; internal storage (e.g. HashMap keys) still uses `AetherString*` with wrapping at the boundary
- **`defer` return type truncation**: `defer` blocks stored the return value in `int _defer_ret` regardless of actual return type — functions returning `ptr`, `long`, or `float` had values silently truncated; now uses `get_c_type()` to emit the correct type
- **String `==`/`!=` pointer comparison instead of content comparison**: `a == b` on two `string` variables emitted C pointer comparison (`==`) instead of `strcmp()` — two strings with identical content but different addresses compared as unequal; now emits `strcmp(a, b) == 0` for `TYPE_STRING` operands
- **`match` on strings used pointer comparison**: Match arms comparing string expressions used `==` instead of `strcmp()` — match against string literals always fell through to the default arm; now emits `strcmp()` for `TYPE_STRING` match expressions
- **`TYPE_UNKNOWN` silent fallback to `int`**: `get_c_type()` silently returned `"int"` for unresolved types, masking upstream inference failures — now emits a compiler warning with suggestion to add explicit type annotations
- **Pattern list element bindings hardcoded to `int`**: List pattern match codegen (`[h|t]`, `[a,b,c]`) always emitted `int` element types regardless of actual list element type — now uses `get_c_type()` from the element's `node_type`
- **Token overflow silent truncation**: Source files exceeding `MAX_TOKENS` (10,000) silently stopped lexing with no error — now emits a clear error message with suggestion to split into multiple files
- **`ae fmt` stub shown in help output**: `ae help` listed `fmt [file]` as a command even though the formatter is not implemented — removed from help output until the feature is ready
- **String interp return type lost through implicit arrow returns**: `f(x) -> { msg = "text ${x}"; msg }` generated C with `int` return type instead of `const char*` — type inference now unwraps `AST_EXPRESSION_STATEMENT` in implicit return nodes and resolves local variable types by scanning preceding block statements
- **`install.sh` silent build failures**: `make` errors piped through `wc -l` were silently swallowed; added `set -eo pipefail` so pipe failures propagate correctly
- **`install.sh` unsolicited sudo**: Installer ran `sudo apt-get install libreadline-dev` without asking — replaced with a printed install instruction so users retain control
- **`ae.c` missing dev-mode include flags**: `-I` flags for `std/fs` and `std/log` directories were present in installed-mode but missing in dev-mode, causing header-not-found errors during development
- **Runtime scheduler portability**: Replaced bare `__thread` with `AETHER_TLS`, removed duplicate `likely`/`unlikely` macros, replaced bare inline asm with `AETHER_CPU_PAUSE()`, guarded `<immintrin.h>` for MSVC (`<intrin.h>`), wrapped `_Static_assert` for 32-bit compatibility
- **`aether_actor_thread.c` bare inline asm**: Replaced GCC `__asm__ __volatile__("pause"/"yield")` with portable `AETHER_CPU_PAUSE()` macro
- **`aether_thread.h` missing EBUSY fallback**: Added `#define EBUSY 16` fallback for platforms that don't define it (alongside existing `ETIMEDOUT` and `ENOMEM` fallbacks)
- **`aether_http_server.c` Windows compat**: Added `socklen_t`, `strcasecmp` (`_stricmp`), and `strdup` (`_strdup`) fallback defines for MSVC
- **`aether_io.c` Windows compat**: Added `S_ISDIR` macro and `stat`/`_stat` mapping for MSVC
- **`aether_log.c` thread safety**: Replaced `localtime()` (returns shared static buffer) with `localtime_r()` (POSIX) / `localtime_s()` (MSVC)
- **Test struct layouts**: Updated 4 runtime test files (`test_scheduler_stress.c`, `test_scheduler_correctness.c`, `test_worksteal_race.c`, `test_scheduler.c`) to use `SPSCQueue*` pointer matching the current `ActorBase` layout

### Changed

- **Windows CI upgraded to full CI suite**: `windows.yml` now runs `make ci` (8-step suite) instead of a minimal build-only check
- **`make ci` includes install smoke test**: `test-install` folded into `make ci` as step 8/8 — every CI run verifies the installed toolchain end-to-end
- **WinLibs GCC updated to 14.2.0**: Auto-download for Windows users without GCC now fetches GCC 14.2.0 (from 13.2.0) for better C11/C17 support and codegen improvements
- **Release pipeline workflow_dispatch guard**: Manual workflow dispatch no longer triggers build/publish jobs without a valid version tag
- **MSVC compat codegen regression test**: New `tests/compiler/test_msvc_compat.sh` — compiles generated C with `AETHER_GCC_COMPAT=0` under `-std=c11 -pedantic` to verify all GCC-extension fallback paths; covers string interpolation, actor dispatch, and helper functions

## [0.16.0]

### Fixed

- **macOS x86_64 release built on ARM runner**: GitHub's `macos-latest` shifted to ARM64 images, causing x86_64 release binaries to be ARM binaries mislabeled as x86_64 — fixed by explicitly using `macos-15-intel` for x86_64 builds in both `release.yml` and `ci.yml`

## [0.15.0]

### Fixed

- **`ae run` fails after install** (`Error opening output file: No such file or directory`): Installed `ae` binary falsely detected dev mode because `aetherc` sits next to `ae` in both `build/` (repo) and `bin/` (installed prefix). The tool then tried to write to a non-existent `build/` directory. Fixed by requiring the dev-mode heuristic to verify `../runtime/` exists (only true in the repo root).
- **`install.sh` corrupts shell RC files**: Appending PATH/AETHER_HOME export lines without checking for a trailing newline caused the first export to concatenate with the last existing line in `.zshrc`/`.bash_profile`. Fixed with a `tail -c 1` check that inserts a newline before appending when needed.
- **`install.sh` Fish shell syntax**: Fish shell was configured with bash `export` syntax which Fish doesn't understand. Fixed to use `set -gx` and `fish_add_path`.
- **`make install` missing source files**: The `install` target created empty `share/aether/` directories but never copied runtime/std source files into them, breaking the source-fallback compilation path when `libaether.a` was absent. Fixed by copying all `.c`/`.h` files from runtime and std subdirectories.
- **Installed `ae` missing source fallback**: When `libaether.a` was not present in an installed prefix, `ae` had no fallback to compile from individual source files. Added source-fallback path using `share/aether/{runtime,std}/*.c` and dual include paths for both `include/aether/` and `share/aether/`.

### Changed

- **CI install smoke test**: Added `make test-install` target and CI step across all 4 platform variants (Linux/GCC, Linux/Clang, macOS/Clang, Windows/MSYS2) — installs to a temp directory, runs `ae init` + `ae run`, and verifies correct output.

## [0.14.0]

### Fixed

- **Scheduler migration race — actor setup phase**: Freshly-spawned actors could be migrated to a new core before their `Setup` message was delivered — if a subsequent message arrived on the same core, the work-inlining path executed `step()` before initialization, causing null dereference / state corruption. Fixed by snapshotting `was_active` before message processing: actors where `was_active=0` and whose mailbox is empty are not migrated (setup still in flight).
- **Portable `usleep`/`sched_yield` wrappers for Windows**: Scheduler code used POSIX `usleep()` and `sched_yield()` which are unavailable on Windows. Added platform wrappers using `Sleep()` and `SwitchToThread()` on Windows, fixing MSYS2/MinGW CI failures.
- **Overflow buffer back-pressure and adaptive wait**: Overflow buffers could grow unbounded under sustained queue saturation. Added head-index drain (O(drained) instead of O(total) memmove), amplification limiter for own-core overflow, and adaptive `scheduler_wait` that spins tight when few messages remain.
- **Codegen if/else variable scoping**: Variables declared in an `if` branch were suppressing re-declaration in the `else` branch — the `declared_vars` tracking was function-level instead of block-scoped. Fixed by saving/restoring `declared_var_count` around if/else branches.
- **Skynet benchmark non-power-of-10 leaf counts**: Fixed incorrect sums when leaf count is not a power of 10; remainder leaves now distributed correctly; root with `size=1` no longer crashes.

## [0.13.0]

### Added

- **`long` type (64-bit integer)**: `long` keyword maps to `int64_t` in generated C — use `long x = 0` for values that exceed 32-bit range; arithmetic with `long` promotes to 64-bit; `print(long_val)` uses `%lld`; actor state fields and message fields support `long`; full support in typechecker, codegen, and pattern-variable extraction
- **Skynet benchmark**: Added the [atemerev/skynet](https://github.com/atemerev/skynet) recursive actor tree benchmark across Aether, Go, Rust, Erlang, Elixir, C (pthreads), Zig (std.Thread), and C++ (std::thread) — 6 levels x 10 = 1,111,111 actors, measures actor creation + aggregation throughput; controlled via `SKYNET_LEAVES` env var
- **Actor state type inference**: Member access on actor refs (`root.total`) now correctly resolves the state field's declared type — enables proper format specifier selection for `print` and type-safe arithmetic on long state fields

### Fixed

- **Scheduler queue back-pressure deadlock** (skynet N>=100k): All scheduler threads could permanently deadlock in a circular-wait. Fixed with bounded 8-retry loop, per-target-core thread-local overflow buffers, and flush-before-drain ordering.
- **Scheduler thread startup race**: Race between `scheduler_start()` returning and threads actually polling. Fixed with `g_threads_ready` barrier that guarantees all threads are actively polling before returning.
- **Work-stealing TOCTOU race on ARM64** (skynet crash at N>=150k): Data race between work-stealing and same-core fast path. Fixed by making `Mailbox.count` atomic with acquire/release ordering.
- **Actor ref state field type propagation**: `infer_type` and `typecheck_expression` now look up state declarations in the actor's AST definition when resolving member access on actor refs
- **`clock_ns()` return type**: Corrected from `TYPE_INT` to `TYPE_INT64` — nanosecond timestamps overflow `int32` after ~2.1 seconds
- **Actor ref / int/ptr type compatibility**: `is_type_compatible` now allows assigning an actor ref to a state field declared as `int` or `ptr`
- **Premature `scheduler_wait` termination**: Fixed by including the `messages_sent - messages_processed` counter delta in the pending count

### Changed

- **SPSCQueue lazy allocation**: `ActorBase.spsc_queue` changed from embedded 3,136-byte struct to an 8-byte pointer, lazily allocated only for `auto_process` actors (66% per-actor memory reduction)
- **QUEUE_SIZE reduced from 4096 to 1024**: Saves ~24 MB per core; overflow buffers handle burst cases
- **Diagnostic prints gated behind AETHER_DEBUG_ORDERING**: `scheduler_wait` progress diagnostics no longer printed in production builds
- **Benchmark README fairness labeling**: Transparent synchronization primitive labels for C, C++, and Zig benchmarks

## [0.12.0]

### Added

- **`cflags` in `aether.toml`**: `[build] cflags = "-O3 -march=native"` is now honoured by `ae build` — previously it was silently ignored; `ae run` continues to use `-O0` for fast development builds
- **`extra_sources` in `[[bin]]`**: Declare extra C files to compile alongside your Aether program directly in the project file — `extra_sources = ["src/ffi.c"]`; merged additively with any `--extra` flags passed on the command line
- **`aether_scheduler_poll(int max_per_actor)`**: New runtime API for C-hosted event loops (e.g. raylib, SDL, game loops) — drains pending actor messages without blocking; call between frames to keep actors alive while C owns the main thread; declared in `runtime/scheduler/multicore_scheduler.h`
- **`ae run` now accepts `--extra`**: The `--extra src/ffi.c` flag previously only worked with `ae build`; now works with both

### Fixed

- **Type inference through call chains**: `propagate_call_types_in_tree` was returning early on every `AST_FUNCTION_DEFINITION` node, making all function bodies invisible to the propagator — calls inside functions (e.g. `seed_glider` calling `set_cell`) were never found
- **Single-pass propagation**: Propagation ran exactly once; a call chain `main → f → g → h` needed 3 passes but only got 1, causing unresolved types to silently fall back to `int`; the pipeline now interleaves propagation with constraint solving in a loop until stable
- **Parameter type annotations now genuinely optional**: As a result of the above two fixes, explicit `: int` / `: ptr` annotations on function parameters are no longer required — types propagate correctly from call sites through arbitrarily deep chains
- **Clear error for `?` in expression context**: Writing `x > 0 ? a : b` (expecting a ternary) now produces an actionable diagnostic — "`?` is the actor ask operator; use if/else for conditional values" — instead of a confusing parse failure

## [0.11.0]

### Changed

- **Auto-free removed**: The experimental compiler-injected auto-free system (`@manual` annotation, `--no-auto-free` flag, `[memory] mode` in `aether.toml`) has been removed. The memory model is now `defer`-first exclusively — explicit, composable, and predictable. See [docs/memory-management.md](docs/memory-management.md)
- **Improved compiler diagnostics**: Better source-context error messages across type checker and parser; structured error codes with help suggestions

### Fixed

- **Module orchestrator**: Improved module import resolution and handling of dot-qualified function calls (`module.func()`)
- **Stale documentation**: Multiple doc files corrected to match actual implemented behaviour

## [0.10.0]

### Fixed

- **Scheduler race on ARM64**: Fixed a work-stealing TOCTOU race in the drain path; added `test_worksteal_race.c` to CI
- **`assigned_core` atomicity**: Changed `assigned_core` field from `int` to `atomic_int` in `ActorBase` for safe cross-core reads; updated all codegen, tests, and examples
- GCC/Clang warnings in `tools/ae.c` eliminated (clean at `-Wall -Wextra`)

## [0.9.0]

### Added

- **`defer`-first memory model**: Explicit `defer type.free(x)` is now the primary and recommended memory management pattern; stdlib types follow consistent `type.new()` / `type.free()` naming
- **Dynamic destructor registry**: Codegen tracks which stdlib constructors (`map_new`, `list_new`, etc.) map to their destructors — used at scope exit
- **`ae` temp directory**: `ae` now uses a stable temp directory for intermediate C files instead of cwd
- **`cmd_examples`**: `ae` CLI gained an examples command for discovering and running bundled examples

### Changed

- Memory management documentation overhauled — `defer` pattern documented comprehensively with multiple allocation, actor state, and escape patterns
- `aether.toml` dot-style module imports enforced as the only import syntax
- Windows: `ae.c` library path now uses mixed separators correctly; codegen adds `NOMINMAX`, `Sleep`, `windows.h` guards

### Fixed

- `realloc` failure handling in AST, module registry, and type inference (prevents leaks on OOM)
- Actor state init type inference in typechecker
- HTTP server: explicit free of `build_response` buffer; correct `Content-Length` in responses

## [0.8.0]

### Added

- **Windows CI**: Full CI pipeline on Windows via MSYS2 MinGW; `make ci` runs compiler, ae, stdlib, REPL, C tests, .ae tests, and examples with no skips

### Fixed

- **Windows build**: Fixed MSYS2/MinGW build failures (`NOMINMAX`, `Sleep`, `windows.h` guards, mixed path separators)
- **NUMA allocator**: `aether_numa_alloc` fallback now uses `numa_alloc_local()` instead of `malloc()` when libnuma is present — prevents allocator mismatch
- **macOS `-Werror` quoting**: Fixed shell quoting in Makefile for macOS clang strict mode
- `fread` return value warnings resolved

## [0.7.0]

### Added

- **Ask/reply (`?` operator)**: Production-ready typed ask — compiler infers the reply message type from the actor's receive block; reply slot travels with the message (not per-actor) for correct concurrent asks; reply payload is properly freed after use
- **Error handling**: `error-handling.ae` example with structured error propagation patterns; typechecker handles error result types

### Fixed

- **Toolchain resolver**: Dev builds (`./build/ae`) now take priority over `$AETHER_HOME`, preventing stale installed compilers from shadowing fresh builds

## [0.6.0]

### Added

- **`ae` CLI tool**: Single entry point for building, running, testing, and managing Aether projects (`ae run`, `ae build`, `ae test`, `ae init`)
- **Version manager**: `ae version list/install/use` to install and switch between Aether releases
- **Project tooling (`apkg`)**: Project scaffolding with `aether.toml`, dependency declarations, and GitHub-based `ae add`
- **`println` and string interpolation**: `println("Hello ${name}!")` with `${}` expressions in strings
- **`defer` statement**: Deferred cleanup in LIFO order, matching Go-style resource management
- **`switch` statement**: C-style switch with fall-through and break
- **Match expressions**: Pattern matching with literal, wildcard, list, and head|tail patterns
- **List patterns**: `[]`, `[x]`, `[h|t]`, `[a, b]` destructuring in match arms
- **Standard library**: Collections (HashMap, Vector, Set, List), JSON parser, HTTP server, TCP/UDP networking, file I/O
- **REPL**: Interactive read-eval-print loop (`ae repl`)
- **LSP server**: Diagnostics from lexer and parser errors for editor integration
- **VS Code / Cursor extension**: Syntax highlighting, file icons, and custom theme
- **Cross-language benchmarks**: Actor-model benchmark suite comparing 11 languages with interactive visualization UI
- **Docker support**: Development container with all toolchains pre-installed

### Changed

- **Ask/reply (`?` operator)**: Now production-ready — typed results (compiler infers reply message type from actor receive blocks), concurrent asks (reply slot travels with the message, not per-actor), proper `free()` of reply payload, configurable timeout via AST
- **HTTP server**: Multi-connection support via thread-per-connection model (POSIX `pthread`); accept loop no longer blocks on a single client
- **Error messages**: Source-context error reporting across lexer, parser, and type checker — errors now show the source line, a caret pointing to the exact column, error codes (`E0100` syntax, `E0300` undefined variable, etc.), and contextual help suggestions
- **Tail call optimization**: Honest reporting — Aether detects tail calls, GCC/Clang optimize them into loops at `-O2` (used by `ae build`)
- **Toolchain resolver**: Dev builds (`./build/ae`) now take priority over `$AETHER_HOME`, preventing stale installed compilers from shadowing fresh builds
- **Type inference**: Function return types are now inferred only from explicit `return` statements and arrow-body expressions; no longer incorrectly inferred from arguments to `print()` or other nested expressions
- **Codegen split**: `codegen.c` refactored into `codegen_expr.c`, `codegen_stmt.c`, `codegen_actor.c`, `codegen_func.c` for maintainability
- **Valgrind CI target**: Reports all Valgrind errors (uninitialised reads, not just leaks)
- **Benchmark visualization**: All methodology descriptions and message counts are dynamic from JSON data (no hardcoded values)
- **Latency metric**: Benchmarks report `ns/msg` (nanoseconds per message) instead of architecture-dependent `cycles/msg`

### Fixed

- Functions with no return statement now correctly generate `void` in C (previously defaulted to `const char*`)
- Match expressions returning strings now generate correct return types
- `ae version use` now actually switches the active compiler on POSIX (copies binaries to `~/.aether/bin/`)
- `snprintf` truncation warnings in type checker and codegen path buffers
- Valgrind uninitialized-memory reads in scheduler tests (replaced `malloc` with `calloc`, aligned test actor structs to `ActorBase` layout)
- NUMA allocator mismatch: `aether_numa_alloc` fallback now uses `numa_alloc_local()` instead of `malloc()` when libnuma is available
- JSON parser string buffer: replaced fixed 4KB stack buffer with dynamically growing heap allocation
- HTTP server response buffer: replaced static 64KB buffer with heap-allocated, correctly sized buffer
- HTTP server request reading: now reads body up to `Content-Length` with dynamic reallocation
- `realloc` failure handling in AST, module registry, and type inference (prevents memory leaks on OOM)

## [0.5.0] 

### Added

- **Main Thread Actor Mode**: Single-actor programs now bypass the scheduler entirely for synchronous message processing
  - Zero-copy message passing using caller's stack pointer
  - Automatic detection when only one actor exists
  - Manual control via `AETHER_NO_INLINE` and `AETHER_INLINE` environment variables
- **Memory Profiles**: Configure pool sizes via `AETHER_PROFILE` environment variable (micro/small/medium/large)
- **New Benchmarks**: counting, thread_ring, fork_join benchmark patterns
- **wait_for_idle()**: Block until all actors have finished processing messages
- **sleep(ms)**: Pause execution for specified milliseconds
- **Cross-platform thread affinity**: Linux hard binding, macOS advisory with QoS hints, Windows SetThreadAffinityMask

### Changed

- Scheduler threads now check `main_thread_only` flag with atomic operations to prevent data races
- LSP server uses `snprintf()` instead of `strcat()` for buffer safety
- LSP document management includes proper error handling for memory allocation
- Documentation updated across architecture, scheduler, runtime optimization guides

### Fixed

- Pattern variable renaming in receive blocks
- Race condition when second actor spawns during main thread mode transition
- LSP buffer overflow vulnerability in diagnostics publishing
- LSP memory allocation error handling

### Security

- Fixed potential buffer overflow in LSP diagnostics
- Added bounds checking to all LSP string operations

## [0.4.0]

### Added

- Thread affinity support for all architectures
- Apple Silicon P-core detection for consistent performance
- C interoperability improvements
- NUMA-aware memory allocation
- Computed goto dispatch for message handlers
- Thread-local message pools

### Changed

- Updated documentation for runtime optimizations
- Improved install script with platform detection

### Fixed

- Install script fixes for various platforms

## [0.3.0]

### Added

- Multicore scheduler with work stealing
- Lock-free SPSC queues for cross-core messaging
- Adaptive batch processing
- Message coalescing

### Changed

- Scheduler redesign for partitioned per-core processing

## [0.2.0]

### Added

- Basic actor system
- Message passing primitives
- Type inference
- Pattern matching in receive blocks

## [0.1.0]

### Added

- Initial release
- Lexer, parser, type checker
- Code generation to C
- Basic runtime with single-threaded scheduler
