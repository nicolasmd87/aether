// epoll backend for Linux
// Included by aether_poller.c on __linux__

#include <sys/epoll.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

struct AetherPoller {
    int epfd;
};

AetherPoller* aether_poller_create(void) {
    int epfd = epoll_create1(0);
    if (epfd < 0) return NULL;

    AetherPoller* p = calloc(1, sizeof(AetherPoller));
    if (!p) { close(epfd); return NULL; }
    p->epfd = epfd;
    return p;
}

void aether_poller_destroy(AetherPoller* p) {
    if (!p) return;
    close(p->epfd);
    free(p);
}

int aether_poller_add(AetherPoller* p, int fd, int events, void* user_data) {
    struct epoll_event ev = {0};
    if (events & AETHER_POLL_READ)  ev.events |= EPOLLIN;
    if (events & AETHER_POLL_WRITE) ev.events |= EPOLLOUT;
    ev.data.ptr = user_data;

    return epoll_ctl(p->epfd, EPOLL_CTL_ADD, fd, &ev) < 0 ? -1 : 0;
}

int aether_poller_remove(AetherPoller* p, int fd) {
    return epoll_ctl(p->epfd, EPOLL_CTL_DEL, fd, NULL) < 0 ? -1 : 0;
}

int aether_poller_modify(AetherPoller* p, int fd, int events, void* user_data) {
    struct epoll_event ev = {0};
    if (events & AETHER_POLL_READ)  ev.events |= EPOLLIN;
    if (events & AETHER_POLL_WRITE) ev.events |= EPOLLOUT;
    ev.data.ptr = user_data;

    return epoll_ctl(p->epfd, EPOLL_CTL_MOD, fd, &ev) < 0 ? -1 : 0;
}

int aether_poller_wait(AetherPoller* p, AetherPollEvent* out, int max_events, int timeout_ms) {
    struct epoll_event* epevents = (struct epoll_event*)alloca(max_events * sizeof(struct epoll_event));
    int n = epoll_wait(p->epfd, epevents, max_events, timeout_ms);
    if (n < 0) return (errno == EINTR) ? 0 : -1;

    for (int i = 0; i < n; i++) {
        out[i].fd = -1;  // epoll doesn't store fd in events; caller uses user_data
        out[i].events = 0;
        out[i].user_data = epevents[i].data.ptr;

        if (epevents[i].events & EPOLLIN)  out[i].events |= AETHER_POLL_READ;
        if (epevents[i].events & EPOLLOUT) out[i].events |= AETHER_POLL_WRITE;
        if (epevents[i].events & (EPOLLERR | EPOLLHUP)) out[i].events |= AETHER_POLL_ERROR;
    }

    return n;
}
