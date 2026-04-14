# Aeocha / TinyWeb Test TODO

## Blocking: Compiler trailing block codegen

The TinyWeb unit tests and integration tests are blocked by a compiler issue
where trailing blocks are silently dropped when many `_ctx: ptr` functions
are defined in the same file.

### Observed behavior
- `web_server(8080) { end_point(GET, "/hello") |...| { } }` works in a small file
- The same pattern produces `web_server(8080);` (no trailing block) when 6+ `_ctx` functions exist
- The generated C has no `_aether_ctx_push`/`_aether_ctx_pop` for these calls

### Reproduction
```
build/ae build tests/syntax/test_tinyweb_spec.ae -o build/test_tinyweb_spec
build/aetherc tests/syntax/test_tinyweb_spec.ae build/test_tinyweb_spec.c
grep "web_server\|_aether_ctx_push" build/test_tinyweb_spec.c
# Shows web_server(8080) without ctx_push — trailing block dropped
```

### Also blocking
- Deeply nested callback → function → trailing block → closure handler
  segfaults (callback variables lose scope in generated C)
- `after` is a reserved keyword — Aeocha uses `after_each` instead

## When fixed
- Migrate test_tinyweb_spec.ae to use the TinyWeb DSL directly
- Add integration tests (server in actor + HTTP client round-trips)
- Add Aeocha before/after hooks to TinyWeb tests
