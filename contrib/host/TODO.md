# contrib/host TODO

## All host modules
- [ ] Capture stdout/stderr from hosted code back to Aether (pipe + shared map `_stdout`/`_stderr` keys, or let pass through as today)
- [ ] Shared map `aether_map_get`/`aether_map_put` bindings for Perl and Ruby currently use eval-injected hashes — outputs stay in the hosted language. Need XS (Perl) or C extension (Ruby) to write outputs back to the C map.
- [ ] `string:bytes` mode for shared map — binary data without base64

## Python
- [ ] Re-add `examples/host-python-demo.ae` after heredoc-strings merges. Recover with: `git show lazy-evaluation:examples/host-python-demo.ae`
- [ ] `os.environ` is cached at CPython startup — sandbox `getenv` interception only works via `ctypes.CDLL(None).getenv`, not `os.environ.get()`. Document workaround.

## Lua
- [ ] Tested and working well. Cleanest host module.

## JS (Duktape)
- [ ] Purest containment — no LD_PRELOAD needed. Only exposed functions are available. Consider adding more bindings (writeFile, exec) with sandbox checks.

## Perl
- [ ] Function names prefixed `aether_perl_` to avoid conflict with Perl's own `perl_run`/`perl_init`. Awkward but necessary.
- [ ] `%ENV` scrubbed at sandbox entry. Shared interpreter means unsandboxed `run()` after sandboxed `run_sandboxed()` sees scrubbed ENV.

## Ruby
- [ ] Same `ENV` scrub issue as Perl.
- [ ] `Fiddle.dlopen("libc.so.6")` succeeds but calls are still intercepted — not a real escape but looks alarming in tests.

## Java
- [ ] Separate process via JVM — uses Panama FFI for shared memory, not in-process embedding.
- [ ] JVM startup requires extensive grants (`/sys/*`, `/proc/*`, many env vars). These are JVM overhead, not application grants. Consider a `grant_jvm_runtime()` convenience.
- [ ] IPv6-mapped addresses (`::ffff:x.x.x.x`) need separate TCP grants from IPv4. Consider normalizing in the checker.
