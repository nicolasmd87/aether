/**
 * Benchmark: epoll-driven actor HTTP server (Option C)
 *
 * Same endpoint as bench_actor_http.c (GET /api/hello -> JSON response).
 *
 * Architecture:
 *   Accept thread:  accept(fd) → epoll_register(fd) → wait for data
 *   Accept thread:  epoll reports data ready → send(worker, fd)
 *   Worker actor:   recv(non-blocking, guaranteed data) → respond → close
 *
 * The accept loop uses epoll to wait for client data BEFORE dispatching
 * to a worker actor. This ensures the worker's recv() never blocks,
 * so the scheduler thread is never stalled.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdatomic.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include "../../std/net/aether_http_server.h"
#include "../../runtime/scheduler/multicore_scheduler.h"
#include "../../runtime/actors/actor_state_machine.h"
#include "../../runtime/config/aether_optimization_config.h"

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

static atomic_int total_requests = 0;

// ---------------------------------------------------------------------------
// Worker pool
// ---------------------------------------------------------------------------
#define WORKERS_PER_CORE 8

static ActorBase** workers = NULL;
static int worker_count = 0;

// Thread-local accept thread identity — used to partition workers per accept thread.
// Each accept thread only sends to workers on its own core → one producer per mailbox → SPSC safe.
static __thread int tls_accept_core = -1;
static __thread int tls_worker_rr = 0;

// Auto-assign accept core on first call per thread
static atomic_int accept_core_counter = 0;

static void* pick_worker(int preferred_core, void (*step)(void*), size_t size) {
    (void)preferred_core; (void)step; (void)size;
    // Lazy init: assign this accept thread to a core
    if (tls_accept_core < 0) {
        tls_accept_core = atomic_fetch_add(&accept_core_counter, 1) % num_cores;
    }
    int base = tls_accept_core * WORKERS_PER_CORE;
    int idx = base + (tls_worker_rr++ % WORKERS_PER_CORE);
    return workers[idx];
}

static void noop_release(void* actor) { (void)actor; }

// ---------------------------------------------------------------------------
// Direct mailbox send — safe because each worker has exactly one accept thread
// as producer (partitioned by core). Scheduler thread only reads (consumer).
// ---------------------------------------------------------------------------
static void direct_send(void* actor_ptr, void* message_data, size_t message_size) {
    ActorBase* actor = (ActorBase*)actor_ptr;

    void* msg_copy = malloc(message_size);
    if (!msg_copy) return;
    memcpy(msg_copy, message_data, message_size);

    Message msg;
    msg.type = *(int*)message_data;
    msg.sender_id = 0;
    msg.payload_int = 0;
    msg.payload_ptr = msg_copy;
    msg.zerocopy.data = NULL;
    msg.zerocopy.size = 0;
    msg.zerocopy.owned = 0;
    msg._reply_slot = NULL;

    mailbox_send(&actor->mailbox, msg);
    atomic_store_explicit(&actor->active, 1, memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// Worker actor step — fd arrives with data already available
// ---------------------------------------------------------------------------
static void worker_step(void* self) {
    ActorBase* actor = (ActorBase*)self;
    Message msg;

    while (mailbox_receive(&actor->mailbox, &msg)) {
        if (msg.type != MSG_HTTP_CONNECTION) {
            free(msg.payload_ptr);
            continue;
        }

        HttpConnectionMessage* conn = (HttpConnectionMessage*)msg.payload_ptr;
        int fd = conn->client_fd;
        free(msg.payload_ptr);

        // recv — data is guaranteed to be available (accept loop waited via epoll)
        char buffer[8192];
        int total = 0;
        while (total < (int)sizeof(buffer) - 1) {
            int n = recv(fd, buffer + total, sizeof(buffer) - 1 - total, 0);
            if (n > 0) {
                total += n;
                buffer[total] = '\0';
                if (strstr(buffer, "\r\n\r\n")) break;
            } else if (n == 0) {
                break;
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                break;
            }
        }

        if (total <= 0) { close(fd); continue; }
        buffer[total] = '\0';

        int count = atomic_fetch_add(&total_requests, 1) + 1;

        char body[128];
        int body_len = snprintf(body, sizeof(body),
            "{\"message\":\"hello\",\"count\":%d}", count);

        char response[512];
        int resp_len = snprintf(response, sizeof(response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Server: Aether/1.0-epoll\r\n"
            "Content-Length: %d\r\n"
            "\r\n"
            "%s", body_len, body);

        send(fd, response, resp_len, MSG_NOSIGNAL);
        close(fd);
    }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

static HttpServer* server = NULL;

void handle_sigint(int sig) {
    (void)sig;
    if (server) http_server_stop(server);
}

int main() {
    signal(SIGINT, handle_sigint);

    // Disable inline/main-thread mode
    atomic_store(&g_aether_config.inline_mode_disabled, true);

    scheduler_init(0);

    worker_count = num_cores * WORKERS_PER_CORE;
    printf("Epoll-actor HTTP server: %d cores, %d worker actors\n",
           num_cores, worker_count);

    scheduler_start();

    // Pre-spawn worker actors distributed across cores
    workers = malloc(worker_count * sizeof(ActorBase*));
    for (int i = 0; i < worker_count; i++) {
        workers[i] = scheduler_spawn_pooled(i % num_cores, worker_step, 0);
        if (!workers[i]) {
            fprintf(stderr, "Failed to spawn worker %d\n", i);
            return 1;
        }
    }

    server = http_server_create(8080);
    if (!server) {
        fprintf(stderr, "Failed to create server\n");
        return 1;
    }

    http_server_set_actor_handler(server,
        worker_step,
        (void (*)(void*, void*, size_t))direct_send,
        (void* (*)(int, void (*)(void*), size_t))pick_worker,
        (void (*)(void*))noop_release);

    printf("Starting on :8080 (epoll-actor mode)\n");

    if (http_server_start(server) != 0) {
        fprintf(stderr, "Failed to start server\n");
        http_server_free(server);
        return 1;
    }

    printf("\nTotal requests served: %d\n", atomic_load(&total_requests));
    http_server_free(server);
    free(workers);
    return 0;
}
