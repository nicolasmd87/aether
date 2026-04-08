// kqueue backend for macOS / BSD
// Included by aether_poller.c on __APPLE__ and __FreeBSD__

#include <sys/event.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

struct AetherPoller {
    int kq;
};

AetherPoller* aether_poller_create(void) {
    int kq = kqueue();
    if (kq < 0) return NULL;

    AetherPoller* p = calloc(1, sizeof(AetherPoller));
    if (!p) { close(kq); return NULL; }
    p->kq = kq;
    return p;
}

void aether_poller_destroy(AetherPoller* p) {
    if (!p) return;
    close(p->kq);
    free(p);
}

int aether_poller_add(AetherPoller* p, int fd, int events, void* user_data) {
    struct kevent changes[2];
    int n = 0;

    if (events & AETHER_POLL_READ) {
        EV_SET(&changes[n++], fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, user_data);
    }
    if (events & AETHER_POLL_WRITE) {
        EV_SET(&changes[n++], fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, user_data);
    }

    return kevent(p->kq, changes, n, NULL, 0, NULL) < 0 ? -1 : 0;
}

int aether_poller_remove(AetherPoller* p, int fd) {
    struct kevent changes[2];
    EV_SET(&changes[0], fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    EV_SET(&changes[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);

    // Ignore ENOENT — filter may not have been registered
    kevent(p->kq, changes, 2, NULL, 0, NULL);
    return 0;
}

int aether_poller_modify(AetherPoller* p, int fd, int events, void* user_data) {
    aether_poller_remove(p, fd);
    return aether_poller_add(p, fd, events, user_data);
}

int aether_poller_wait(AetherPoller* p, AetherPollEvent* out, int max_events, int timeout_ms) {
    struct timespec ts;
    struct timespec* tsp = NULL;

    if (timeout_ms >= 0) {
        ts.tv_sec = timeout_ms / 1000;
        ts.tv_nsec = (timeout_ms % 1000) * 1000000L;
        tsp = &ts;
    }

    struct kevent* kevents = (struct kevent*)alloca(max_events * sizeof(struct kevent));
    int n = kevent(p->kq, NULL, 0, kevents, max_events, tsp);
    if (n < 0) return (errno == EINTR) ? 0 : -1;

    for (int i = 0; i < n; i++) {
        out[i].fd = (int)kevents[i].ident;
        out[i].events = 0;
        out[i].user_data = kevents[i].udata;

        if (kevents[i].filter == EVFILT_READ)  out[i].events |= AETHER_POLL_READ;
        if (kevents[i].filter == EVFILT_WRITE) out[i].events |= AETHER_POLL_WRITE;
        if (kevents[i].flags & EV_ERROR)       out[i].events |= AETHER_POLL_ERROR;
        if (kevents[i].flags & EV_EOF)         out[i].events |= AETHER_POLL_READ;
    }

    return n;
}
