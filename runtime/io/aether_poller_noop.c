// No-op stub for platforms without networking (WASM, embedded, bare-metal)
// Included by aether_poller.c when AETHER_HAS_NETWORKING is not defined

#include <stdlib.h>

struct AetherPoller { int dummy; };

AetherPoller* aether_poller_create(void) { return NULL; }
void aether_poller_destroy(AetherPoller* p) { (void)p; }
int aether_poller_add(AetherPoller* p, int fd, int events, void* user_data) {
    (void)p; (void)fd; (void)events; (void)user_data; return -1;
}
int aether_poller_remove(AetherPoller* p, int fd) { (void)p; (void)fd; return -1; }
int aether_poller_modify(AetherPoller* p, int fd, int events, void* user_data) {
    (void)p; (void)fd; (void)events; (void)user_data; return -1;
}
int aether_poller_wait(AetherPoller* p, AetherPollEvent* out, int max_events, int timeout_ms) {
    (void)p; (void)out; (void)max_events; (void)timeout_ms; return -1;
}
