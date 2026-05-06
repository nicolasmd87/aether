/* derive.h — `@derive(...)` synthesizer pass (#338).
 *
 * Pre-typecheck pass: walks the program AST, finds AST_STRUCT_DEFINITION
 * nodes with a `derive:<list>` annotation, and synthesizes the
 * corresponding helper function definitions (T_eq today; T_format /
 * T_clone / T_hash in follow-up commits).
 *
 * Synthesized functions are inserted as siblings into the program
 * AST. Normal typecheck + codegen runs them through unchanged.
 *
 * Idempotent: running the pass twice on the same program produces
 * the same number of synthesized functions (the annotation is
 * cleared after synthesis so subsequent passes are no-ops).
 */

#ifndef AETHER_DERIVE_H
#define AETHER_DERIVE_H

#include "../ast.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Walk `program`, synthesize derive-helpers for each annotated
 * struct, and append them as program children. Returns 0 on
 * success, non-zero if any annotated struct could not be
 * synthesized (e.g. unsupported field type, unknown derive name).
 * Errors are emitted via type_error so the diagnostic chain
 * surfaces them with file/line context. */
int derive_synthesize_pass(ASTNode* program);

#ifdef __cplusplus
}
#endif

#endif
