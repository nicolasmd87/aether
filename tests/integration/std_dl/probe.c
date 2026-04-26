// Tiny library used by tests/integration/std_dl/test_std_dl.sh.
// Built with -shared into a .so/.dylib so std.dl can dlopen it
// and resolve the exported symbols.

int probe_answer(void) {
    return 42;
}

int probe_add(int a, int b) {
    return a + b;
}

const char* probe_greeting(void) {
    return "hello from probe";
}
