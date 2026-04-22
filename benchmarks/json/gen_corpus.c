// Deterministic JSON corpus generator for the benchmark harness.
//
// Produces fixtures that stress specific parser code paths:
//   api-response.json   — realistic REST payload (committed)
//   strings-heavy.json  — strings with escapes + Unicode (committed)
//   numbers-heavy.json  — integers + floats + exponents (committed)
//   deep.json           — deeply nested arrays (committed)
//   large.json          — 10 MB of mixed records (generated on demand)
//
// Determinism: uses a linear-congruential pseudo-random sequence so every
// run produces byte-identical output. No dependency on rand() quality.
//
// Usage: gen_corpus <name> <output-path>
//   gen_corpus api-response benchmarks/json/corpus/api-response.json
//   gen_corpus strings       benchmarks/json/corpus/strings-heavy.json
//   gen_corpus numbers       benchmarks/json/corpus/numbers-heavy.json
//   gen_corpus deep          benchmarks/json/corpus/deep.json
//   gen_corpus large         benchmarks/json/corpus/large.json  (10 MB)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static uint32_t g_seed = 0x12345678;
static uint32_t rnd_u32(void) {
    // Park-Miller LCG; deterministic, single-threaded.
    g_seed = (uint32_t)((uint64_t)g_seed * 48271u % 2147483647u);
    return g_seed;
}
static int rnd_range(int lo, int hi) {
    return lo + (int)(rnd_u32() % (uint32_t)(hi - lo + 1));
}

static const char* WORDS[] = {
    "alpha", "beta", "gamma", "delta", "epsilon", "zeta", "eta", "theta",
    "iota", "kappa", "lambda", "mu", "nu", "xi", "omicron", "pi", "rho",
    "sigma", "tau", "upsilon", "phi", "chi", "psi", "omega", "actor",
    "message", "scheduler", "arena", "runtime", "parser", "token", "value"
};
static const int WORDS_N = sizeof(WORDS) / sizeof(WORDS[0]);

static const char* FIRST[] = {
    "Alice", "Bob", "Carol", "Dave", "Eve", "Frank", "Grace", "Heidi",
    "Ivan", "Judy", "Kevin", "Laura", "Mallory", "Ned", "Olivia", "Peggy"
};
static const int FIRST_N = sizeof(FIRST) / sizeof(FIRST[0]);

static const char* TAGS[] = {
    "active", "archived", "pending", "review", "urgent", "backlog",
    "production", "experimental", "internal", "public"
};
static const int TAGS_N = sizeof(TAGS) / sizeof(TAGS[0]);

// ---------------------------------------------------------------------------
// api-response.json: realistic REST payload (~100 KB)
// Shape:
//   { "status": "ok", "count": N,
//     "users": [ { id, name, email, active, tags, ... } ] }
// ---------------------------------------------------------------------------

static void gen_api_response(FILE* out) {
    int n = 500;
    fprintf(out, "{\n");
    fprintf(out, "  \"status\": \"ok\",\n");
    fprintf(out, "  \"count\": %d,\n", n);
    fprintf(out, "  \"page\": 1,\n");
    fprintf(out, "  \"page_size\": %d,\n", n);
    fprintf(out, "  \"generated_at\": \"2026-04-21T19:00:00Z\",\n");
    fprintf(out, "  \"users\": [\n");
    for (int i = 0; i < n; i++) {
        const char* first = FIRST[rnd_u32() % (uint32_t)FIRST_N];
        int age = rnd_range(18, 80);
        int active = (rnd_u32() & 1) ? 1 : 0;
        double score = (double)(rnd_u32() % 100000) / 100.0;
        int tag_count = rnd_range(1, 3);

        fprintf(out, "    {\n");
        fprintf(out, "      \"id\": %d,\n", 1000 + i);
        fprintf(out, "      \"name\": \"%s %d\",\n", first, i);
        fprintf(out, "      \"email\": \"%s.%d@example.com\",\n", first, i);
        fprintf(out, "      \"age\": %d,\n", age);
        fprintf(out, "      \"active\": %s,\n", active ? "true" : "false");
        fprintf(out, "      \"score\": %.2f,\n", score);
        fprintf(out, "      \"tags\": [");
        for (int t = 0; t < tag_count; t++) {
            fprintf(out, "%s\"%s\"", t ? ", " : "", TAGS[rnd_u32() % (uint32_t)TAGS_N]);
        }
        fprintf(out, "],\n");
        fprintf(out, "      \"address\": {\n");
        fprintf(out, "        \"street\": \"%d %s St\",\n", rnd_range(1, 9999), WORDS[rnd_u32() % (uint32_t)WORDS_N]);
        fprintf(out, "        \"city\": \"%s\",\n", WORDS[rnd_u32() % (uint32_t)WORDS_N]);
        fprintf(out, "        \"postal\": \"%05d\"\n", rnd_range(10000, 99999));
        fprintf(out, "      },\n");
        fprintf(out, "      \"last_login\": \"2026-%02d-%02dT%02d:%02d:%02dZ\"\n",
                rnd_range(1, 12), rnd_range(1, 28),
                rnd_range(0, 23), rnd_range(0, 59), rnd_range(0, 59));
        fprintf(out, "    }%s\n", i == n - 1 ? "" : ",");
    }
    fprintf(out, "  ]\n}\n");
}

// ---------------------------------------------------------------------------
// strings-heavy.json: array of strings with heavy escape/Unicode content (~200 KB)
// ---------------------------------------------------------------------------

static void gen_strings(FILE* out) {
    int n = 2000;
    fprintf(out, "[\n");
    for (int i = 0; i < n; i++) {
        fprintf(out, "  \"");
        int len = rnd_range(20, 120);
        for (int j = 0; j < len; j++) {
            int which = (int)(rnd_u32() % 100u);
            if (which < 3) fputs("\\n", out);       // newline escape
            else if (which < 5) fputs("\\t", out);  // tab escape
            else if (which < 7) fputs("\\\"", out); // quoted quote
            else if (which < 9) fputs("\\\\", out); // escaped backslash
            else if (which < 12) fputs("\\u00e9", out); // unicode escape (é)
            else if (which < 14) fputs("café", out);    // raw UTF-8
            else if (which < 16) fputs("☃", out);        // 3-byte UTF-8
            else if (which < 18) fputs("😀", out);       // 4-byte UTF-8
            else {
                char c = 'a' + (char)(rnd_u32() % 26u);
                fputc(c, out);
            }
        }
        fprintf(out, "\"%s\n", i == n - 1 ? "" : ",");
    }
    fprintf(out, "]\n");
}

// ---------------------------------------------------------------------------
// numbers-heavy.json: array mixing integers, floats, negatives, exponents (~200 KB)
// ---------------------------------------------------------------------------

static void gen_numbers(FILE* out) {
    int n = 15000;
    fprintf(out, "[\n");
    for (int i = 0; i < n; i++) {
        int which = (int)(rnd_u32() % 5u);
        const char* sep = (i == n - 1) ? "" : ",";
        switch (which) {
            case 0: {  // small integer
                int v = (int)(rnd_u32() % 1000u) - 500;
                fprintf(out, "  %d%s\n", v, sep);
                break;
            }
            case 1: {  // large integer
                long long v = (long long)rnd_u32() * 1000LL;
                if (rnd_u32() & 1) v = -v;
                fprintf(out, "  %lld%s\n", v, sep);
                break;
            }
            case 2: {  // float
                double v = (double)(rnd_u32() % 1000000u) / 100.0;
                if (rnd_u32() & 1) v = -v;
                fprintf(out, "  %.4f%s\n", v, sep);
                break;
            }
            case 3: {  // exponential
                double v = (double)(rnd_u32() % 10000u) / 10.0;
                int e = (int)(rnd_u32() % 20u) - 10;
                fprintf(out, "  %.3fe%d%s\n", v, e, sep);
                break;
            }
            case 4: {  // very small float
                double v = (double)(rnd_u32() % 1000u) * 0.0001;
                fprintf(out, "  %.6f%s\n", v, sep);
                break;
            }
        }
    }
    fprintf(out, "]\n");
}

// ---------------------------------------------------------------------------
// deep.json: deeply nested array, 200 levels, value at bottom (~5 KB)
// ---------------------------------------------------------------------------

static void gen_deep(FILE* out) {
    int depth = 200;
    for (int i = 0; i < depth; i++) fputc('[', out);
    fputs("42", out);
    for (int i = 0; i < depth; i++) fputc(']', out);
    fputc('\n', out);
}

// ---------------------------------------------------------------------------
// large.json: 10 MB of mixed records (generated, not committed)
// ---------------------------------------------------------------------------

static void gen_large(FILE* out, size_t target_bytes) {
    fprintf(out, "[\n");
    size_t written = 2;
    int i = 0;
    while (written < target_bytes) {
        const char* first = FIRST[rnd_u32() % (uint32_t)FIRST_N];
        int age = rnd_range(18, 80);
        double score = (double)(rnd_u32() % 100000) / 100.0;
        char rec[512];
        int n = snprintf(rec, sizeof(rec),
            "%s  {\"id\":%d,\"name\":\"%s %d\",\"age\":%d,\"score\":%.2f,\"tag\":\"%s\"}",
            i == 0 ? "" : ",\n",
            1000 + i, first, i, age, score,
            TAGS[rnd_u32() % (uint32_t)TAGS_N]);
        fwrite(rec, 1, (size_t)n, out);
        written += (size_t)n;
        i++;
    }
    fprintf(out, "\n]\n");
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <name> <output-path>\n", argv[0]);
        fprintf(stderr, "  names: api-response, strings, numbers, deep, large\n");
        return 1;
    }
    const char* name = argv[1];
    const char* path = argv[2];
    FILE* out = fopen(path, "wb");
    if (!out) {
        fprintf(stderr, "cannot open %s for writing\n", path);
        return 1;
    }

    // Reset seed per-fixture so each one is independently reproducible.
    g_seed = 0x12345678;

    if      (strcmp(name, "api-response") == 0) gen_api_response(out);
    else if (strcmp(name, "strings")      == 0) gen_strings(out);
    else if (strcmp(name, "numbers")      == 0) gen_numbers(out);
    else if (strcmp(name, "deep")         == 0) gen_deep(out);
    else if (strcmp(name, "large")        == 0) gen_large(out, 10 * 1024 * 1024);
    else {
        fprintf(stderr, "unknown fixture: %s\n", name);
        fclose(out);
        return 1;
    }

    fclose(out);
    return 0;
}
