// Platform poller — compile-time backend selection
//
// Provides a unified interface for I/O event notification across platforms.
// This is the foundation for non-blocking I/O in the actor system.
//
// See aether_poller.h for the API.

#include "aether_poller.h"

#if !AETHER_HAS_NETWORKING

#include "aether_poller_noop.c"

#elif defined(__linux__)

#include "aether_poller_epoll.c"

#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)

#include "aether_poller_kqueue.c"

#else

#include "aether_poller_poll.c"

#endif
