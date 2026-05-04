/*
 * Issue #344 — caller-info attribute path. The Aether-side
 * caller_info_inproc.ae covers identity + deadline; this C harness
 * covers the per-attribute key/value pair flow because it needs
 * `const char**` argv that's awkward to produce from Aether code.
 *
 * Links against libaether.a (built by `make stdlib`) and exercises
 * aether_set_caller / aether_caller_attribute / aether_clear_caller
 * directly. self-reports PASS or a FAIL line on stderr.
 */

#include "../../../runtime/aether_host.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define FAIL(msg) do { fprintf(stderr, "FAIL: %s\n", msg); return 1; } while (0)

int main(void) {
    /* Default state: empty. */
    if (aether_caller_attribute("role")[0] != '\0') FAIL("attr non-empty before set");

    /* Three attributes plus identity + deadline. */
    const char* keys[3] = { "role",  "tier", "tenant" };
    const char* vals[3] = { "admin", "gold", "acme"   };

    int rc = aether_set_caller("alice", keys, vals, 3, 7777);
    if (rc != 0) FAIL("set_caller returned non-zero");

    if (strcmp(aether_caller_identity(),     "alice") != 0) FAIL("identity wrong");
    if (strcmp(aether_caller_attribute("role"),    "admin") != 0) FAIL("role wrong");
    if (strcmp(aether_caller_attribute("tier"),    "gold")  != 0) FAIL("tier wrong");
    if (strcmp(aether_caller_attribute("tenant"),  "acme")  != 0) FAIL("tenant wrong");
    if (aether_caller_attribute("missing")[0] != '\0')      FAIL("missing key returned non-empty");
    if (aether_caller_deadline_ms() != 7777) FAIL("deadline wrong");

    /* Replace on a second set — no leak of old keys. */
    const char* keys2[1] = { "role" };
    const char* vals2[1] = { "guest" };
    if (aether_set_caller("bob", keys2, vals2, 1, 0) != 0) FAIL("second set non-zero");
    if (strcmp(aether_caller_identity(),     "bob")   != 0) FAIL("identity didn't replace");
    if (strcmp(aether_caller_attribute("role"),    "guest") != 0) FAIL("role didn't replace");
    if (aether_caller_attribute("tier")[0]   != '\0') FAIL("tier leaked from previous set");
    if (aether_caller_attribute("tenant")[0] != '\0') FAIL("tenant leaked from previous set");
    if (aether_caller_deadline_ms() != 0) FAIL("deadline didn't replace");

    /* Clear wipes everything. */
    aether_clear_caller();
    if (aether_caller_identity()[0]      != '\0') FAIL("identity not cleared");
    if (aether_caller_attribute("role")[0] != '\0') FAIL("attr not cleared");
    if (aether_caller_deadline_ms()      != 0)    FAIL("deadline not cleared");

    /* Overflow — feeding more attributes than the cap returns -1
     * and leaves prior state intact. */
    if (aether_set_caller("alice", keys, vals, 3, 100) != 0) FAIL("setup before overflow");
    /* Deliberately oversized — cap is AETHER_CALLER_INFO_MAX_ATTRS (32). */
    enum { TOO_MANY = AETHER_CALLER_INFO_MAX_ATTRS + 5 };
    const char* big_keys[TOO_MANY];
    const char* big_vals[TOO_MANY];
    for (int i = 0; i < TOO_MANY; i++) {
        big_keys[i] = "k";
        big_vals[i] = "v";
    }
    if (aether_set_caller("eve", big_keys, big_vals, TOO_MANY, 0) != -1)
        FAIL("oversized set_caller didn't return -1");
    /* Prior state must be intact. */
    if (strcmp(aether_caller_identity(), "alice") != 0)
        FAIL("oversized set_caller leaked partial state");
    if (strcmp(aether_caller_attribute("role"), "admin") != 0)
        FAIL("oversized set_caller corrupted attrs");

    /* Byte-budget overflow: one giant value larger than the arena. */
    aether_clear_caller();
    char* huge = malloc(AETHER_CALLER_INFO_MAX_BYTES + 100);
    if (!huge) FAIL("malloc");
    memset(huge, 'x', AETHER_CALLER_INFO_MAX_BYTES + 99);
    huge[AETHER_CALLER_INFO_MAX_BYTES + 99] = '\0';
    const char* k1[1] = { "k" };
    const char* v1[1] = { huge };
    if (aether_set_caller("a", k1, v1, 1, 0) != -1)
        FAIL("byte-budget overflow didn't return -1");
    free(huge);

    fprintf(stderr, "PASS\n");
    return 0;
}
