#ifndef AETHER_POLLER_H
#define AETHER_POLLER_H

// Platform-agnostic I/O event notification.
//
// Backends:
//   Linux:       epoll
//   macOS/BSD:   kqueue
//   Other POSIX: poll()
//   No network:  no-op stubs
//
// Usage:
//   AetherPoller* p = aether_poller_create();
//   aether_poller_add(p, server_fd, AETHER_POLL_READ, my_context);
//   AetherPollEvent events[64];
//   int n = aether_poller_wait(p, events, 64, 1000);
//   for (int i = 0; i < n; i++) { /* handle events[i] */ }
//   aether_poller_destroy(p);

#include "../config/aether_optimization_config.h"

// Event flags
#define AETHER_POLL_READ   1
#define AETHER_POLL_WRITE  2
#define AETHER_POLL_ERROR  4

typedef struct {
    int   fd;
    int   events;     // Bitmask of AETHER_POLL_READ | AETHER_POLL_WRITE | AETHER_POLL_ERROR
    void* user_data;  // Caller-supplied context (actor ref, connection state, etc.)
} AetherPollEvent;

typedef struct AetherPoller AetherPoller;

// Create a new poller instance.  Returns NULL on failure.
AetherPoller* aether_poller_create(void);

// Destroy a poller and release all resources.
void aether_poller_destroy(AetherPoller* p);

// Register a file descriptor for monitoring.
// events: AETHER_POLL_READ, AETHER_POLL_WRITE, or both (bitwise OR).
// user_data: opaque pointer returned in AetherPollEvent on readiness.
// Returns 0 on success, -1 on error.
int aether_poller_add(AetherPoller* p, int fd, int events, void* user_data);

// Stop monitoring a file descriptor.
// Returns 0 on success, -1 on error.
int aether_poller_remove(AetherPoller* p, int fd);

// Change the monitored events or user_data for a file descriptor.
// Returns 0 on success, -1 on error.
int aether_poller_modify(AetherPoller* p, int fd, int events, void* user_data);

// Wait for events.
// out:        caller-allocated array to receive ready events.
// max_events: capacity of the out array.
// timeout_ms: -1 = block indefinitely, 0 = non-blocking, >0 = milliseconds.
// Returns the number of ready events (0 on timeout, -1 on error).
int aether_poller_wait(AetherPoller* p, AetherPollEvent* out, int max_events, int timeout_ms);

#endif // AETHER_POLLER_H
