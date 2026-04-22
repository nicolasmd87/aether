// run_yyjson_bench.c — Reference comparison harness for yyjson.
//
// Same corpus + methodology as run_json_bench.c, but parses with yyjson
// instead of std/json. yyjson is fetched to vendor/ (gitignored) at
// benchmark-time; this file is NOT compiled in any normal build.
//
// Purpose: give the user an apples-to-apples upper bound ("how fast is
// the state-of-the-art scalar parser on this hardware?"). The decision
// to rewrite our parser is gated on the gap between this number and
// ours.

#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <dirent.h>

#include "vendor/yyjson.h"

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static long peak_rss_kb(void) {
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
#ifdef __APPLE__
    return ru.ru_maxrss / 1024;
#else
    return ru.ru_maxrss;
#endif
}

typedef struct {
    char*  data;
    size_t size;
    char   name[256];
} Fixture;

static int load_fixture(const char* path, Fixture* fx) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return 0; }
    rewind(f);
    fx->data = (char*)malloc((size_t)sz + 1);
    if (!fx->data) { fclose(f); return 0; }
    if (fread(fx->data, 1, (size_t)sz, f) != (size_t)sz) {
        free(fx->data); fclose(f); return 0;
    }
    fx->data[sz] = '\0';
    fx->size = (size_t)sz;
    fclose(f);
    const char* slash = strrchr(path, '/');
    const char* base = slash ? slash + 1 : path;
    snprintf(fx->name, sizeof(fx->name), "%s", base);
    return 1;
}

static void free_fixture(Fixture* fx) { free(fx->data); }

static size_t estimate_value_count(const char* s, size_t n) {
    size_t count = 1;
    int in_str = 0, escape = 0;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if (in_str) {
            if (escape) { escape = 0; continue; }
            if (c == '\\') { escape = 1; continue; }
            if (c == '"') in_str = 0;
            continue;
        }
        if (c == '"') { in_str = 1; continue; }
        if (c == ',') count++;
    }
    return count;
}

typedef struct {
    double median_sec;
    double p95_sec;
    long   rss_delta_kb;
    size_t value_count;
} Result;

static int cmp_double(const void* a, const void* b) {
    double da = *(const double*)a, db = *(const double*)b;
    return (da < db) ? -1 : (da > db) ? 1 : 0;
}

static int benchmark_fixture(const Fixture* fx, int warmup, int iters, Result* out) {
    for (int i = 0; i < warmup; i++) {
        yyjson_doc* d = yyjson_read(fx->data, fx->size, 0);
        if (!d) {
            fprintf(stderr, "[yyjson] warmup parse %s failed\n", fx->name);
            return 0;
        }
        yyjson_doc_free(d);
    }

    double* times = (double*)malloc(sizeof(double) * (size_t)iters);
    if (!times) return 0;

    long rss_before = peak_rss_kb();
    for (int i = 0; i < iters; i++) {
        double t0 = now_seconds();
        yyjson_doc* d = yyjson_read(fx->data, fx->size, 0);
        double t1 = now_seconds();
        if (!d) { free(times); return 0; }
        yyjson_doc_free(d);
        times[i] = t1 - t0;
    }
    long rss_after = peak_rss_kb();

    qsort(times, (size_t)iters, sizeof(double), cmp_double);
    out->median_sec = times[iters / 2];
    out->p95_sec    = times[(iters * 95) / 100];
    out->rss_delta_kb = rss_after - rss_before;
    out->value_count = estimate_value_count(fx->data, fx->size);
    free(times);
    return 1;
}

static void print_header(void) {
    printf("\n");
    printf("| Fixture              | Size       | yyjson MB/s | p95 MB/s | Median ns  | ns/value | Values   | RSS Δ KB |\n");
    printf("| -------------------- | ---------- | ----------- | -------- | ---------- | -------- | -------- | --------- |\n");
}

static void print_row(const Fixture* fx, const Result* r) {
    double mb = (double)fx->size / (1024.0 * 1024.0);
    printf("| %-20s | %8zu B | %11.1f | %8.1f | %10.0f | %8.1f | %8zu | %9ld |\n",
           fx->name, fx->size,
           mb / r->median_sec,
           mb / r->p95_sec,
           r->median_sec * 1e9,
           (r->median_sec * 1e9) / (double)r->value_count,
           r->value_count,
           r->rss_delta_kb);
}

static int is_json_file(const char* name) {
    size_t n = strlen(name);
    return n > 5 && strcmp(name + n - 5, ".json") == 0;
}

int main(int argc, char** argv) {
    int warmup = 5, iters = 100;
    const char* env_w = getenv("JSON_BENCH_WARMUP");
    const char* env_i = getenv("JSON_BENCH_ITERS");
    if (env_w) warmup = atoi(env_w);
    if (env_i) iters  = atoi(env_i);
    if (warmup < 1) warmup = 1;
    if (iters < 10) iters = 10;

    printf("# yyjson bench: warmup=%d iters=%d\n", warmup, iters);

    const char* corpus_dir = (argc > 1) ? argv[1] : "benchmarks/json/corpus";
    DIR* d = opendir(corpus_dir);
    if (!d) { fprintf(stderr, "cannot open %s\n", corpus_dir); return 1; }

    char paths[32][512];
    int n = 0;
    struct dirent* e;
    while ((e = readdir(d)) && n < 32) {
        if (!is_json_file(e->d_name)) continue;
        snprintf(paths[n++], sizeof(paths[0]), "%s/%s", corpus_dir, e->d_name);
    }
    closedir(d);
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (strcmp(paths[i], paths[j]) > 0) {
                char tmp[512]; strcpy(tmp, paths[i]); strcpy(paths[i], paths[j]); strcpy(paths[j], tmp);
            }

    print_header();
    int failed = 0;
    for (int i = 0; i < n; i++) {
        Fixture fx;
        if (!load_fixture(paths[i], &fx)) { failed++; continue; }
        Result r;
        if (!benchmark_fixture(&fx, warmup, iters, &r)) failed++;
        else print_row(&fx, &r);
        free_fixture(&fx);
    }
    return failed;
}
