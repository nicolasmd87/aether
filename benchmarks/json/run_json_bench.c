// run_json_bench.c — Microbenchmark for std/json/aether_json.c.
//
// Measures parse throughput (MB/s), per-value cost (ns), peak RSS delta,
// and total bytes allocated for each fixture in corpus/.
//
// Methodology:
//   - Warm up 5 iterations per fixture.
//   - Time 100 measured iterations. Per-iter: parse the entire buffer,
//     record elapsed time, free the result.
//   - Report the MEDIAN of the measured iterations. Also report p95 and
//     best/worst, so outliers are visible.
//   - RSS via getrusage(RUSAGE_SELF). Allocations via malloc/realloc/free
//     interposition (link-time symbol wrap on Linux; macOS uses a thread-
//     local counter patched via a small wrapper since linker --wrap isn't
//     available).
//
// Usage: run_json_bench [fixture-path ...]
//   no args: runs every *.json in corpus/
//
// Build:
//   gcc -O2 -Wall run_json_bench.c <all the usual aether objects> -o run_json_bench
//
// The executable links the live std/json/aether_json.c directly, so any
// parser change is reflected on the next rebuild.

#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE 1  // macOS: ru_maxrss lives in the BSD-source namespace.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <dirent.h>

#include "../../std/json/aether_json.h"

// --------------------------- malloc interposition -------------------------
//
// We want total bytes allocated during a single parse. A process-wide malloc
// counter is simple and portable: wrap malloc/realloc/free to update two
// _Thread_local counters. The bench runs single-threaded so thread-locality
// is sufficient.
//
// Portability:
//   - Linux + macOS: function interposition via dlsym works, but it adds
//     a link-time dep on dlfcn and surprising semantics (wrapping the
//     system allocator affects everything including startup). We instead
//     use a cheap trick: offer a `json_bench_alloc_reset/get` pair that
//     callers invoke around the parse window. The actual allocator isn't
//     touched — we just instrument `malloc_size` via a status snapshot.
//
// For v1, we take the simpler route: we snapshot RSS before/after each
// parse and report the delta. That's a coarse proxy for "bytes allocated"
// but it's portable to every platform (no dlsym, no __wrap_*).
//
// The old arena approach (counting bytes) is ideal for the rewrite where
// we'd add a hook. For benchmarking the current parser without touching
// it, RSS delta is what we've got.

// --------------------------- timing ---------------------------------------

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static long peak_rss_kb(void) {
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    // On macOS ru_maxrss is in bytes; on Linux it's in KiB.
#ifdef __APPLE__
    return ru.ru_maxrss / 1024;
#else
    return ru.ru_maxrss;
#endif
}

// --------------------------- fixture I/O ---------------------------------

typedef struct {
    char*  data;       // null-terminated buffer
    size_t size;       // data size (excluding the null)
    char   name[256];  // short display name
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

    // short name: basename
    const char* slash = strrchr(path, '/');
    const char* base = slash ? slash + 1 : path;
    snprintf(fx->name, sizeof(fx->name), "%s", base);
    return 1;
}

static void free_fixture(Fixture* fx) { free(fx->data); }

// --------------------------- value-count estimator -----------------------
//
// For ns/value reporting we need "number of JSON values" in the fixture.
// Doing this cheaply without re-implementing a parser: count structural
// tokens. `, ], }` roughly correspond to value boundaries; `[ {` open
// containers. This undercounts single-valued docs but it's a consistent
// heuristic across fixtures so cross-fixture ratios stay meaningful.

static size_t estimate_value_count(const char* s, size_t n) {
    size_t count = 1;  // the root value
    int in_str = 0;
    int escape = 0;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if (in_str) {
            if (escape) { escape = 0; continue; }
            if (c == '\\') { escape = 1; continue; }
            if (c == '"') { in_str = 0; }
            continue;
        }
        if (c == '"') { in_str = 1; continue; }
        if (c == ',') count++;
    }
    return count;
}

// --------------------------- benchmark loop ------------------------------

typedef struct {
    double median_sec;
    double p95_sec;
    double best_sec;
    double worst_sec;
    long   rss_delta_kb;
    size_t value_count;
} Result;

static int cmp_double(const void* a, const void* b) {
    double da = *(const double*)a, db = *(const double*)b;
    return (da < db) ? -1 : (da > db) ? 1 : 0;
}

static int benchmark_fixture(const Fixture* fx, int warmup, int iters, Result* out) {
    // Warm-up: priming caches, allocator pools, branch predictor.
    for (int i = 0; i < warmup; i++) {
        JsonValue* v = json_parse_raw(fx->data);
        if (!v) {
            fprintf(stderr, "[bench] WARN: warm-up parse of %s failed: %s\n",
                    fx->name, json_last_error());
            return 0;
        }
        json_free(v);
    }

    double* times = (double*)malloc(sizeof(double) * (size_t)iters);
    if (!times) return 0;

    long rss_before = peak_rss_kb();

    for (int i = 0; i < iters; i++) {
        double t0 = now_seconds();
        JsonValue* v = json_parse_raw(fx->data);
        double t1 = now_seconds();
        if (!v) { free(times); return 0; }
        json_free(v);
        times[i] = t1 - t0;
    }

    long rss_after = peak_rss_kb();

    qsort(times, (size_t)iters, sizeof(double), cmp_double);
    out->median_sec = times[iters / 2];
    out->p95_sec    = times[(iters * 95) / 100];
    out->best_sec   = times[0];
    out->worst_sec  = times[iters - 1];
    out->rss_delta_kb = rss_after - rss_before;
    out->value_count = estimate_value_count(fx->data, fx->size);
    free(times);
    return 1;
}

// --------------------------- reporting -----------------------------------

static void print_header(void) {
    printf("\n");
    printf("| Fixture              | Size       | Median MB/s | p95 MB/s | Median ns  | ns/value | Values   | RSS Δ KB |\n");
    printf("| -------------------- | ---------- | ----------- | -------- | ---------- | -------- | -------- | --------- |\n");
}

static void print_row(const Fixture* fx, const Result* r) {
    double mb = (double)fx->size / (1024.0 * 1024.0);
    double median_mbps = mb / r->median_sec;
    double p95_mbps    = mb / r->p95_sec;
    double median_ns   = r->median_sec * 1e9;
    double ns_per_val  = median_ns / (double)r->value_count;

    printf("| %-20s | %8zu B | %11.1f | %8.1f | %10.0f | %8.1f | %8zu | %9ld |\n",
           fx->name,
           fx->size,
           median_mbps,
           p95_mbps,
           median_ns,
           ns_per_val,
           r->value_count,
           r->rss_delta_kb);
}

// --------------------------- main ----------------------------------------

static int run_path(const char* path, int warmup, int iters) {
    Fixture fx;
    if (!load_fixture(path, &fx)) {
        fprintf(stderr, "[bench] cannot load %s\n", path);
        return 0;
    }
    Result r;
    if (!benchmark_fixture(&fx, warmup, iters, &r)) {
        fprintf(stderr, "[bench] benchmark of %s failed\n", path);
        free_fixture(&fx);
        return 0;
    }
    print_row(&fx, &r);
    free_fixture(&fx);
    return 1;
}

static int is_json_file(const char* name) {
    size_t n = strlen(name);
    return n > 5 && strcmp(name + n - 5, ".json") == 0;
}

static int default_corpus(const char* corpus_dir, int warmup, int iters) {
    DIR* d = opendir(corpus_dir);
    if (!d) {
        fprintf(stderr, "[bench] cannot open %s\n", corpus_dir);
        return 1;
    }

    // Collect + sort for stable order.
    char paths[32][512];
    int n = 0;
    struct dirent* e;
    while ((e = readdir(d)) != NULL && n < 32) {
        if (!is_json_file(e->d_name)) continue;
        snprintf(paths[n], sizeof(paths[n]), "%s/%s", corpus_dir, e->d_name);
        n++;
    }
    closedir(d);

    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (strcmp(paths[i], paths[j]) > 0) {
                char tmp[512];
                strcpy(tmp, paths[i]);
                strcpy(paths[i], paths[j]);
                strcpy(paths[j], tmp);
            }
        }
    }

    print_header();
    int failed = 0;
    for (int i = 0; i < n; i++) {
        if (!run_path(paths[i], warmup, iters)) failed++;
    }
    return failed;
}

int main(int argc, char** argv) {
    int warmup = 5;
    int iters  = 100;

    // Env var overrides for CI tuning.
    const char* env_w = getenv("JSON_BENCH_WARMUP");
    if (env_w) warmup = atoi(env_w);
    const char* env_i = getenv("JSON_BENCH_ITERS");
    if (env_i) iters = atoi(env_i);
    if (warmup < 1) warmup = 1;
    if (iters < 10) iters = 10;

    printf("# json bench: warmup=%d iters=%d\n", warmup, iters);

    if (argc <= 1) {
        // Default: scan benchmarks/json/corpus/ relative to repo root.
        return default_corpus("benchmarks/json/corpus", warmup, iters);
    }

    print_header();
    int failed = 0;
    for (int i = 1; i < argc; i++) {
        if (!run_path(argv[i], warmup, iters)) failed++;
    }
    return failed;
}
