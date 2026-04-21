/* Fails to compile unless the cflags from aether.toml are applied,
 * because the test marker must be defined. */
#ifndef AE_CFLAGS_TEST_MARKER
#error "AE_CFLAGS_TEST_MARKER not defined — aether.toml [build] cflags was not applied to this build path"
#endif

int ae_cflags_probe_value(void) { return AE_CFLAGS_TEST_MARKER; }
