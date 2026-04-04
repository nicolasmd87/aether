// aether_host_perl.c — Embedded Perl Language Host Module
//
// Embeds Perl in the Aether process. Perl's open(), $ENV{},
// system() etc. go through libc and are intercepted by the
// LD_PRELOAD sandbox layer.

#include "aether_host_perl.h"
#include "../../../runtime/aether_sandbox.h"
#include "../../../runtime/aether_shared_map.h"

extern void* _aether_ctx_stack[];
extern int _aether_ctx_depth;

#ifdef AETHER_HAS_PERL
#include <EXTERN.h>
#include <perl.h>
#include <string.h>

static PerlInterpreter* my_perl = NULL;

// Permission checker — same as other host modules
extern int list_size(void*);
extern void* list_get(void*, int);

static int pattern_match(const char* pat, const char* resource) {
    int plen = strlen(pat);
    int rlen = strlen(resource);
    if (plen == 1 && pat[0] == '*') return 1;
    if (plen > 1 && pat[plen-1] == '*') {
        if (strncmp(pat, resource, plen-1) == 0) return 1;
    }
    if (plen > 1 && pat[0] == '*') {
        int slen = plen - 1;
        if (rlen >= slen && strcmp(resource + rlen - slen, pat + 1) == 0) return 1;
    }
    return strcmp(pat, resource) == 0;
}

static int perms_allow(void* perms, const char* category, const char* resource) {
    if (!perms) return 1;
    int n = list_size(perms);
    if (n == 0) return 0;
    for (int i = 0; i < n; i += 2) {
        const char* cat = (const char*)list_get(perms, i);
        const char* pat = (const char*)list_get(perms, i + 1);
        if (!cat || !pat) continue;
        if (cat[0] == '*' && pat[0] == '*') return 1;
        if (strcmp(cat, category) == 0 && pattern_match(pat, resource)) return 1;
    }
    return 0;
}

static int host_perl_checker(const char* category, const char* resource) {
    if (_aether_ctx_depth <= 0) return 1;
    for (int level = 0; level < _aether_ctx_depth; level++) {
        if (!perms_allow(_aether_ctx_stack[level], category, resource)) return 0;
    }
    return 1;
}

int aether_perl_init(void) {
    if (my_perl) return 0;
    my_perl = perl_alloc();
    if (!my_perl) return -1;
    perl_construct(my_perl);

    char* embedding[] = { "", "-e", "0" };
    perl_parse(my_perl, NULL, 3, embedding, NULL);
    perl_run(my_perl);
    return 0;
}

void aether_perl_finalize(void) {
    if (my_perl) {
        perl_destruct(my_perl);
        perl_free(my_perl);
        my_perl = NULL;
    }
}

static int run_perl_code(const char* code) {
    // eval_pv runs a Perl string and returns the result
    SV* result = eval_pv(code, FALSE);
    if (SvTRUE(ERRSV)) {
        fprintf(stderr, "[perl] %s\n", SvPV_nolen(ERRSV));
        return -1;
    }
    (void)result;
    return 0;
}

int aether_perl_run(const char* code) {
    if (!code) return -1;
    aether_perl_init();
    return run_perl_code(code);
}

int aether_perl_run_sandboxed(void* perms, const char* code) {
    if (!code) return -1;
    aether_perl_init();

    _aether_ctx_stack[_aether_ctx_depth++] = perms;
    aether_sandbox_check_fn prev = _aether_sandbox_checker;
    _aether_sandbox_checker = host_perl_checker;

    // Scrub Perl's cached %ENV — delete vars the sandbox doesn't grant.
    // Perl populates %ENV at startup before the sandbox is active.
    {
        int n = list_size(perms);
        // Build a Perl snippet that deletes non-granted env vars
        char scrub[4096];
        int pos = snprintf(scrub, sizeof(scrub),
            "my %%keep; ");
        for (int i = 0; i < n && pos < 3900; i += 2) {
            const char* cat = (const char*)list_get(perms, i);
            const char* pat = (const char*)list_get(perms, i + 1);
            if (cat && strcmp(cat, "env") == 0 && pat) {
                if (strcmp(pat, "*") == 0) {
                    pos += snprintf(scrub + pos, sizeof(scrub) - pos,
                        "$keep{$_}=1 for keys %%ENV; ");
                } else {
                    pos += snprintf(scrub + pos, sizeof(scrub) - pos,
                        "$keep{'%s'}=1; ", pat);
                }
            }
        }
        snprintf(scrub + pos, sizeof(scrub) - pos,
            "delete $ENV{$_} for grep { !$keep{$_} } keys %%ENV;");
        run_perl_code(scrub);
    }

    int result = run_perl_code(code);

    _aether_sandbox_checker = prev;
    _aether_ctx_depth--;

    return result;
}

// --- Shared map for Perl ---
// Inject %_aether_input from C map, provide aether_map_get/put subs,
// read %_aether_output back into C map after return.

int aether_perl_run_sandboxed_with_map(void* perms, const char* code, uint64_t map_token) {
    if (!code) return -1;
    aether_perl_init();

    extern void aether_shared_map_freeze_inputs_by_token(uint64_t);
    aether_shared_map_freeze_inputs_by_token(map_token);

    // Inject frozen inputs as %_aether_input
    {
        int n = aether_shared_map_count_by_token(map_token);
        char inject[8192];
        int pos = snprintf(inject, sizeof(inject),
            "our %%_aether_input = (); our %%_aether_output = ();\n");
        for (int i = 0; i < n && pos < 7900; i++) {
            const char* k = aether_shared_map_key_at_by_token(map_token, i);
            const char* v = aether_shared_map_value_at_by_token(map_token, i);
            if (k && v) {
                pos += snprintf(inject + pos, sizeof(inject) - pos,
                    "$_aether_input{'%s'} = '%s';\n", k, v);
            }
        }
        pos += snprintf(inject + pos, sizeof(inject) - pos,
            "sub aether_map_get { return $_aether_input{$_[0]}; }\n"
            "sub aether_map_put { $_aether_output{$_[0]} = $_[1]; }\n");
        run_perl_code(inject);
    }

    _aether_ctx_stack[_aether_ctx_depth++] = perms;
    aether_sandbox_check_fn prev = _aether_sandbox_checker;
    _aether_sandbox_checker = host_perl_checker;

    int result = run_perl_code(code);

    _aether_sandbox_checker = prev;
    _aether_ctx_depth--;

    // Note: outputs written via aether_map_put stay in Perl's
    // %_aether_output hash. Reading them back into C would require
    // XS or SvPV extraction. Known limitation for Perl host module.

    return result;
}

#else
#include <stdio.h>
int aether_perl_init(void) {
    fprintf(stderr, "error: std.host.perl not available (compile with AETHER_HAS_PERL)\n");
    return -1;
}
void aether_perl_finalize(void) {}
int aether_perl_run(const char* code) { (void)code; return perl_init(); }
int aether_perl_run_sandboxed(void* perms,
    const char* code) {
  (void)perms; (void)code;
  return perl_init();
}
int aether_perl_run_sandboxed_with_map(void* perms, const char* code,
    uint64_t token) {
  (void)perms; (void)code; (void)token;
  return perl_init();
}
#endif
