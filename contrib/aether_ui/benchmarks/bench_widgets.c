// Aether UI backend benchmarks.
//
// Measures per-operation latency for the hot paths — widget creation,
// reactive-state propagation, layout, canvas replay, and reverse
// handle lookup. Results are emitted as `name,count,total_ms,mean_us`
// CSV on stdout so the CI can snapshot them into
// docs/aether-ui-benchmarks.md.
//
// All benchmarks run headless (no message pump) so they measure pure
// backend overhead without display compositor interaction.

#include "../aether_ui_backend.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
static double now_ms(void) {
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart * 1000.0 / (double)freq.QuadPart;
}
#else
static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1.0e6;
}
#endif

static void emit(const char* name, int count, double total_ms) {
    double mean_us = (total_ms * 1000.0) / (double)count;
    printf("%s,%d,%.3f,%.3f\n", name, count, total_ms, mean_us);
}

static void bench_widget_create(void) {
    const int N = 5000;
    double t0 = now_ms();
    for (int i = 0; i < N; i++) (void)aether_ui_text_create("bench");
    emit("text_create", N, now_ms() - t0);

    t0 = now_ms();
    for (int i = 0; i < N; i++) (void)aether_ui_button_create_plain("x");
    emit("button_create_plain", N, now_ms() - t0);

    t0 = now_ms();
    for (int i = 0; i < N; i++) (void)aether_ui_vstack_create(4);
    emit("vstack_create", N, now_ms() - t0);

    t0 = now_ms();
    for (int i = 0; i < N; i++) (void)aether_ui_divider_create();
    emit("divider_create", N, now_ms() - t0);
}

static void bench_state_propagation(void) {
    int state = aether_ui_state_create(0.0);
    for (int i = 0; i < 200; i++) {
        int t = aether_ui_text_create("x");
        aether_ui_state_bind_text(state, t, "v=", "");
    }
    const int N = 500;
    double t0 = now_ms();
    for (int i = 0; i < N; i++) aether_ui_state_set(state, (double)i);
    emit("state_set_200bindings", N, now_ms() - t0);
}

static void bench_deep_layout(void) {
    int root = aether_ui_vstack_create(2);
    const int N = 400;
    double t0 = now_ms();
    for (int i = 0; i < N; i++) {
        int t = aether_ui_text_create("row");
        aether_ui_widget_add_child_ctx((void*)(intptr_t)root, t);
    }
    emit("add_child_400_flat", N, now_ms() - t0);
}

static void bench_canvas(void) {
    int c = aether_ui_canvas_create_impl(800, 600);
    const int N = 5000;
    double t0 = now_ms();
    for (int i = 0; i < N; i++) {
        aether_ui_canvas_move_to_impl(c, (float)(i % 800), (float)(i % 600));
        aether_ui_canvas_line_to_impl(c,
            (float)((i + 20) % 800), (float)((i + 20) % 600));
    }
    emit("canvas_line_cmd", N, now_ms() - t0);

    t0 = now_ms();
    for (int i = 0; i < N; i++) {
        aether_ui_canvas_fill_rect_impl(c,
            (float)(i % 800), (float)(i % 600),
            10.0f, 10.0f, 0.5f, 0.2f, 0.8f, 1.0f);
    }
    emit("canvas_fill_rect_cmd", N, now_ms() - t0);
}

static void bench_utf8_roundtrip(void) {
    int tf = aether_ui_textfield_create("", (void*)0);
    const int N = 10000;
    double t0 = now_ms();
    for (int i = 0; i < N; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "sample-%d", i);
        aether_ui_textfield_set_text(tf, buf);
        (void)aether_ui_textfield_get_text(tf);
    }
    emit("textfield_utf8_roundtrip", N, now_ms() - t0);
}

// Reverse-lookup benchmark: exercises aether_ui_handle_for_widget(),
// the hot path fired from every WM_COMMAND dispatch, every stack/grid
// layout pass, and every /widgets JSON emit in the test driver.
//
// The benchmark runs at multiple pool sizes. With the O(1) hash backing
// this function, mean_us should stay ~flat as the pool grows. The
// pre-hash linear scan would have shown roughly linear growth (e.g.
// 2500ns at pool=5000 vs 50ns at pool=100).
static void bench_reverse_lookup_at_size(int widget_count) {
    void** handles = (void**)malloc(sizeof(void*) * (size_t)widget_count);
    for (int i = 0; i < widget_count; i++) {
        int h = aether_ui_text_create("row");
        handles[i] = aether_ui_get_widget(h);
    }
    const int ITER = 500000;
    volatile int sink = 0;
    char buf[64];
    double t0 = now_ms();
    for (int i = 0; i < ITER; i++) {
        sink += aether_ui_handle_for_widget(
            handles[(i * 2654435761u) % widget_count]);
    }
    double elapsed = now_ms() - t0;
    snprintf(buf, sizeof(buf), "handle_for_widget_pool_%d", widget_count);
    emit(buf, ITER, elapsed);
    (void)sink;
    free(handles);
}

static void bench_reverse_lookup(void) {
    bench_reverse_lookup_at_size(100);
    bench_reverse_lookup_at_size(1000);
    bench_reverse_lookup_at_size(5000);
    bench_reverse_lookup_at_size(20000);
}

int main(void) {
    printf("name,count,total_ms,mean_us\n");
    bench_widget_create();
    bench_state_propagation();
    bench_deep_layout();
    bench_canvas();
    bench_utf8_roundtrip();
    bench_reverse_lookup();
    return 0;
}
