#ifndef AETHER_STRINGSEQ_H
#define AETHER_STRINGSEQ_H

#include <stddef.h>

/* std.collections.string_seq — Erlang/Elixir-shaped cons-cell linked
 * list of string values.
 *
 * Empty list is the NULL pointer. Each cell carries a CACHED length
 * field so `string_seq_length(s)` is O(1) — Erlang's BEAM doesn't
 * cache length per cell because of its fixed compact cell layout;
 * Aether controls the layout, so we do.
 *
 * Cells are reference-counted to allow structural sharing without
 * copying:
 *
 *     t  = string_seq_cons("c",  string_seq_cons("d", NULL));
 *     a  = string_seq_cons("a",  t);   // shares t
 *     b  = string_seq_cons("b",  t);   // shares t
 *     string_seq_free(a);              // a frees, t stays alive for b
 *     string_seq_free(b);              // b frees, t now drops to 0 and frees
 *
 * Cells are also IMMUTABLE after creation — `cons` is the only
 * constructor; there's no `set_head` or `set_tail`. That makes
 * cycles impossible, so the iterative free below cannot loop, and
 * we don't need cycle detection on the spine.
 *
 * Operation complexity:
 *   empty / cons / head / tail / is_empty / length / retain : O(1)
 *   free                                                    : O(n) work,
 *                                                              O(1) stack
 *                                                              (iterative)
 *   from_array                                              : O(n)
 *   to_array                                                : O(n)
 *
 * Refcount semantics:
 *   - `cons(h, t)` retains BOTH `h` (via aether_string_retain) and
 *     `t` (via string_seq_retain). Callers that want to transfer
 *     their ref into the new cell call `string_seq_free` on the old
 *     local immediately afterward — see `string_seq_from_array` for
 *     the canonical builder pattern.
 *   - `retain(s)` bumps a cell's refcount; pair with `free(s)`.
 *   - `free(s)` decrements; if it reaches zero we release the head
 *     and walk to the tail. If a tail cell is still shared
 *     (refcount > 0 after decrement) we stop — the other owner
 *     finishes the walk later when its own free runs.
 *
 * Issue: motivated by the svn-aether port hitting a `string.split()`-
 * returning-`ptr` mismatch with `string[]` message fields. Cons cells
 * give a runtime-built sequence shape that fits actor messages
 * cleanly without forcing callers to plumb a separate `count` field.
 */

typedef struct StringSeq {
    int   ref_count;
    int   length;            /* length of THIS list (head + length(tail)). */
    void* head;              /* AetherString* or plain const char* (refcount-aware via str_*). */
    struct StringSeq* tail;  /* NULL = end of list. */
} StringSeq;

/* Empty list. Returns NULL — we treat NULL itself as the empty seq.
 * Callers shouldn't depend on a unique sentinel pointer; treat the
 * return value as opaque. */
StringSeq* string_seq_empty(void);

/* Prepend `head` to `tail`. Retains `head` and `tail`. Returns a new
 * cell with refcount = 1 and `length = (tail ? tail->length : 0) + 1`.
 * NULL on OOM. Callers transferring ownership of `tail` into the new
 * cell should call `string_seq_free(tail)` after this returns to drop
 * the local ref — the new cell holds its own. */
StringSeq* string_seq_cons(const char* head, StringSeq* tail);

/* Borrowed reference to the head element. Returns "" on the empty
 * list (matches the existing `string.length("")` shape — caller can
 * treat the empty case identically). The returned pointer is
 * borrowed: callers that need to outlive the seq must retain or copy.
 *
 * Returns `const char*` (not `const void*`) so the C signature lines
 * up with what Aether emits for `-> string` returns. The byte
 * pointer behind it can be either an AetherString* header (the
 * magic-tagged refcounted shape) or a plain char* literal —
 * downstream code dispatches via the standard `is_aether_string`
 * magic check. Callers that want the unwrapped payload bytes call
 * `aether_string_data` on the result. */
const char* string_seq_head(StringSeq* s);

/* Borrowed pointer to the tail seq. NULL on empty. The returned
 * pointer's lifetime is tied to the parent `s`; callers that need
 * to retain it independently must call `string_seq_retain`. */
StringSeq* string_seq_tail(StringSeq* s);

/* 1 if empty (NULL), 0 otherwise. */
int string_seq_is_empty(StringSeq* s);

/* O(1) cached length. 0 on empty. */
int string_seq_length(StringSeq* s);

/* Bump refcount. NULL-safe. Returns the same pointer for chaining. */
StringSeq* string_seq_retain(StringSeq* s);

/* Decrement refcount; if it reaches 0, release head and recurse into
 * tail (iteratively — no stack growth for deep lists). Stops at the
 * first cell whose refcount stays > 0 after decrement (shared tail
 * still owned elsewhere). NULL-safe and idempotent on a fully-freed
 * spine. */
void string_seq_free(StringSeq* s);

/* Build a seq from an AetherStringArray* (the shape `string.split`
 * returns). Each element of `arr->strings` becomes one cell. The
 * caller supplies `count` so a longer array can be sub-sequenced
 * via the same entry point. Returns NULL on empty input or OOM
 * (any partial spine is freed before return).
 *
 * The signature uses bare `void*` rather than `AetherStringArray*`
 * so callers can write the Aether-side declaration as `arr: ptr`
 * without a cross-module cast — the function knows the shape
 * internally. */
StringSeq* string_seq_from_array(void* arr, int count);

/* Materialise `s` as an AetherStringArray* (the legacy split result
 * shape). Useful for interop with code that already consumes the
 * AetherStringArray surface. Caller frees with `string_array_free`.
 * Returns NULL on empty input or OOM. */
void* string_seq_to_array(StringSeq* s);

#endif
