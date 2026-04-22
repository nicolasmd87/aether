// run_conformance.c — JSONTestSuite conformance runner.
//
// Walks every *.json file in the cases directory, parses with
// std.json's json_parse_raw_n, and checks the outcome against the
// filename prefix:
//   y_  MUST be accepted. If rejected → test fails.
//   n_  MUST be rejected. If accepted → test fails.
//   i_  Implementation-defined; outcome is recorded, not enforced.
//
// Exits non-zero if any y_ or n_ case fails.

#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#include "../../../std/json/aether_json.h"

typedef struct {
    int y_pass, y_fail;
    int n_pass, n_fail;
    int i_accepted, i_rejected;
} Stats;

static int load_file(const char* path, char** out_buf, size_t* out_len) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return 0; }
    rewind(f);
    char* buf = (char*)malloc((size_t)sz);
    if (!buf) { fclose(f); return 0; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf); fclose(f); return 0;
    }
    fclose(f);
    *out_buf = buf;
    *out_len = (size_t)sz;
    return 1;
}

static int is_json_file(const char* name) {
    size_t n = strlen(name);
    return n > 5 && strcmp(name + n - 5, ".json") == 0;
}

// Returns the test's expected-outcome prefix: 'y', 'n', 'i', or 0.
static char outcome_prefix(const char* name) {
    if (name[0] == 'y' && name[1] == '_') return 'y';
    if (name[0] == 'n' && name[1] == '_') return 'n';
    if (name[0] == 'i' && name[1] == '_') return 'i';
    return 0;
}

int main(int argc, char** argv) {
    const char* cases_dir = (argc > 1) ? argv[1] : "tests/conformance/json/cases";
    int verbose = (argc > 2 && strcmp(argv[2], "-v") == 0);

    DIR* d = opendir(cases_dir);
    if (!d) {
        fprintf(stderr, "cannot open %s\n", cases_dir);
        return 2;
    }

    Stats s = {0};
    struct dirent* e;
    int total = 0;
    int failures = 0;

    while ((e = readdir(d)) != NULL) {
        if (!is_json_file(e->d_name)) continue;
        char prefix = outcome_prefix(e->d_name);
        if (prefix == 0) continue;

        char path[512];
        snprintf(path, sizeof(path), "%s/%s", cases_dir, e->d_name);

        char* buf = NULL;
        size_t len = 0;
        if (!load_file(path, &buf, &len)) {
            fprintf(stderr, "LOAD FAIL %s\n", e->d_name);
            failures++;
            continue;
        }

        JsonValue* v = json_parse_raw_n(buf, len);
        int accepted = (v != NULL);
        if (v) json_free(v);
        const char* err = json_last_error();

        total++;
        switch (prefix) {
            case 'y':
                if (accepted) {
                    s.y_pass++;
                    if (verbose) printf("  PASS y %s\n", e->d_name);
                } else {
                    s.y_fail++;
                    failures++;
                    printf("  FAIL y (should accept, rejected) %s — %s\n", e->d_name, err);
                }
                break;
            case 'n':
                if (!accepted) {
                    s.n_pass++;
                    if (verbose) printf("  PASS n %s\n", e->d_name);
                } else {
                    s.n_fail++;
                    failures++;
                    printf("  FAIL n (should reject, accepted) %s\n", e->d_name);
                }
                break;
            case 'i':
                if (accepted) s.i_accepted++;
                else          s.i_rejected++;
                if (verbose) printf("  i %s %s\n",
                                    accepted ? "accepted" : "rejected", e->d_name);
                break;
        }

        free(buf);
    }
    closedir(d);

    printf("\n");
    printf("==== JSONTestSuite conformance ====\n");
    printf("  y_* (must accept):    %3d / %3d pass\n",
           s.y_pass, s.y_pass + s.y_fail);
    printf("  n_* (must reject):    %3d / %3d pass\n",
           s.n_pass, s.n_pass + s.n_fail);
    printf("  i_* (impl-defined):   %3d accepted, %3d rejected\n",
           s.i_accepted, s.i_rejected);
    printf("  TOTAL runs:           %d\n", total);
    printf("  FAILURES:             %d\n", failures);
    return failures ? 1 : 0;
}
