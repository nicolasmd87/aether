/**
 * Benchmark: epoll-driven actor HTTP server with adaptive keep-alive
 *
 * Two worker modes:
 *   - Low concurrency:  keep-alive in worker loop (max throughput)
 *   - High concurrency: Connection: close (max concurrency)
 *
 * The mode is controlled by a command-line flag: --keepalive or --close
 * Default: keep-alive
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdatomic.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include "../../std/net/aether_http_server.h"
#include "../../runtime/scheduler/multicore_scheduler.h"
#include "../../runtime/actors/actor_state_machine.h"
#include "../../runtime/config/aether_optimization_config.h"

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

static atomic_int total_requests = 0;
static int use_keepalive = 1;  // default: keep-alive

// ---------------------------------------------------------------------------
// Worker pool
// ---------------------------------------------------------------------------
#define WORKERS_PER_CORE 8

static ActorBase** workers = NULL;
static int worker_count = 0;

static __thread int tls_accept_core = -1;
static __thread int tls_worker_rr = 0;
static atomic_int accept_core_counter = 0;

static void* pick_worker(int preferred_core, void (*step)(void*), size_t size) {
    (void)preferred_core; (void)step; (void)size;
    if (tls_accept_core < 0) {
        tls_accept_core = atomic_fetch_add(&accept_core_counter, 1) % num_cores;
    }
    int base = tls_accept_core * WORKERS_PER_CORE;
    int idx = base + (tls_worker_rr++ % WORKERS_PER_CORE);
    return workers[idx];
}

static void noop_release(void* actor) { (void)actor; }

// ---------------------------------------------------------------------------
// Direct mailbox send
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
// Respond to a single request
// ---------------------------------------------------------------------------
static inline int respond(int fd, int keepalive) {
    int count = atomic_fetch_add(&total_requests, 1) + 1;

    char body[128];
    int body_len = snprintf(body, sizeof(body),
        "{\"message\":\"hello\",\"count\":%d}", count);

    char response[512];
    int resp_len = snprintf(response, sizeof(response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Server: Aether/1.0-epoll\r\n"
        "%s"
        "Content-Length: %d\r\n"
        "\r\n"
        "%s",
        keepalive ? "Connection: keep-alive\r\n" : "Connection: close\r\n",
        body_len, body);

    return (int)send(fd, response, resp_len, MSG_NOSIGNAL);
}

// ---------------------------------------------------------------------------
// Worker step — keep-alive mode
// ---------------------------------------------------------------------------
static void worker_step_keepalive(void* self) {
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

        // Set short recv timeout for keep-alive wait
        struct timeval tv = { .tv_sec = 0, .tv_usec = 5000 }; // 5ms
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        for (int ka = 0; ka < 10000; ka++) {
            char buffer[8192];
            int total = 0;
            while (total < (int)sizeof(buffer) - 1) {
                int n = recv(fd, buffer + total, sizeof(buffer) - 1 - total, 0);
                if (n > 0) {
                    total += n;
                    buffer[total] = '\0';
                    if (strstr(buffer, "\r\n\r\n")) break;
                } else {
                    break;
                }
            }

            if (total <= 0) break;

            // Check if client wants close
            if (strstr(buffer, "Connection: close")) {
                respond(fd, 0);
                break;
            }

            respond(fd, 1);

            // If mailbox has pending work, yield this connection
            if (atomic_load_explicit(&actor->mailbox.count, memory_order_relaxed) > 0) {
                break;
            }
        }

        close(fd);
    }
}

// ---------------------------------------------------------------------------
// Worker step — connection close mode
// ---------------------------------------------------------------------------
static void worker_step_close(void* self) {
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

        if (total > 0) {
            respond(fd, 0);
        }

        shutdown(fd, SHUT_WR);
        char drain[512];
        while (recv(fd, drain, sizeof(drain), 0) > 0) {}
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

int main(int argc, char** argv) {
    signal(SIGINT, handle_sigint);

    // Parse args
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--close") == 0) use_keepalive = 0;
        if (strcmp(argv[i], "--keepalive") == 0) use_keepalive = 1;
    }

    atomic_store(&g_aether_config.inline_mode_disabled, true);

    scheduler_init(0);

    worker_count = num_cores * WORKERS_PER_CORE;

    void (*step_fn)(void*) = use_keepalive ? worker_step_keepalive : worker_step_close;

    printf("Epoll-actor HTTP server: %d cores, %d workers, mode=%s\n",
           num_cores, worker_count, use_keepalive ? "keep-alive" : "close");

    scheduler_start();

    workers = malloc(worker_count * sizeof(ActorBase*));
    for (int i = 0; i < worker_count; i++) {
        workers[i] = scheduler_spawn_pooled(i % num_cores, step_fn, 0);
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

    server->max_connections = 32768;

    http_server_set_actor_handler(server,
        step_fn,
        (void (*)(void*, void*, size_t))direct_send,
        (void* (*)(int, void (*)(void*), size_t))pick_worker,
        (void (*)(void*))noop_release);

    printf("Starting on :8080\nPress Ctrl+C to stop\n\n");

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
