// Portable poll() fallback
// Included by aether_poller.c when no platform-specific backend is available

#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define POLLER_POLL_INITIAL_CAP 64

typedef struct {
    struct pollfd pfd;
    void* user_data;
} PollEntry;

struct AetherPoller {
    PollEntry* entries;
    int count;
    int capacity;
};

AetherPoller* aether_poller_create(void) {
    AetherPoller* p = calloc(1, sizeof(AetherPoller));
    if (!p) return NULL;
    p->capacity = POLLER_POLL_INITIAL_CAP;
    p->entries = calloc(p->capacity, sizeof(PollEntry));
    if (!p->entries) { free(p); return NULL; }
    return p;
}

void aether_poller_destroy(AetherPoller* p) {
    if (!p) return;
    free(p->entries);
    free(p);
}

static int poller_find(AetherPoller* p, int fd) {
    for (int i = 0; i < p->count; i++) {
        if (p->entries[i].pfd.fd == fd) return i;
    }
    return -1;
}

int aether_poller_add(AetherPoller* p, int fd, int events, void* user_data) {
    if (poller_find(p, fd) >= 0) return -1;  // already registered

    if (p->count >= p->capacity) {
        int nc = p->capacity * 2;
        PollEntry* ne = realloc(p->entries, nc * sizeof(PollEntry));
        if (!ne) return -1;
        p->entries = ne;
        p->capacity = nc;
    }

    PollEntry* e = &p->entries[p->count++];
    e->pfd.fd = fd;
    e->pfd.events = 0;
    if (events & AETHER_POLL_READ)  e->pfd.events |= POLLIN;
    if (events & AETHER_POLL_WRITE) e->pfd.events |= POLLOUT;
    e->pfd.revents = 0;
    e->user_data = user_data;
    return 0;
}

int aether_poller_remove(AetherPoller* p, int fd) {
    int idx = poller_find(p, fd);
    if (idx < 0) return -1;

    // Swap with last entry
    p->entries[idx] = p->entries[--p->count];
    return 0;
}

int aether_poller_modify(AetherPoller* p, int fd, int events, void* user_data) {
    int idx = poller_find(p, fd);
    if (idx < 0) return -1;

    PollEntry* e = &p->entries[idx];
    e->pfd.events = 0;
    if (events & AETHER_POLL_READ)  e->pfd.events |= POLLIN;
    if (events & AETHER_POLL_WRITE) e->pfd.events |= POLLOUT;
    e->user_data = user_data;
    return 0;
}

int aether_poller_wait(AetherPoller* p, AetherPollEvent* out, int max_events, int timeout_ms) {
    // Build pollfd array
    struct pollfd* pfds = (struct pollfd*)alloca(p->count * sizeof(struct pollfd));
    for (int i = 0; i < p->count; i++) {
        pfds[i] = p->entries[i].pfd;
    }

    int n = poll(pfds, p->count, timeout_ms);
    if (n < 0) return (errno == EINTR) ? 0 : -1;
    if (n == 0) return 0;

    int out_count = 0;
    for (int i = 0; i < p->count && out_count < max_events; i++) {
        if (pfds[i].revents == 0) continue;

        out[out_count].fd = pfds[i].fd;
        out[out_count].events = 0;
        out[out_count].user_data = p->entries[i].user_data;

        if (pfds[i].revents & POLLIN)  out[out_count].events |= AETHER_POLL_READ;
        if (pfds[i].revents & POLLOUT) out[out_count].events |= AETHER_POLL_WRITE;
        if (pfds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
            out[out_count].events |= AETHER_POLL_ERROR;

        out_count++;
    }

    return out_count;
}
