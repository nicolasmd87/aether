#ifndef AETHER_ARENA_H
#define AETHER_ARENA_H

#include <stddef.h>

typedef struct Arena {
    char* memory;
    size_t size;
    size_t used;
    struct Arena* next;
} Arena;

typedef struct {
    Arena* arena;
    size_t restore_point;
} ArenaScope;

Arena* arena_create(size_t size);
void* arena_alloc(Arena* arena, size_t bytes);
void* arena_alloc_aligned(Arena* arena, size_t bytes, size_t alignment);
void arena_reset(Arena* arena);
void arena_destroy(Arena* arena);

ArenaScope arena_begin(Arena* arena);
void arena_end(ArenaScope scope);

size_t arena_get_used(Arena* arena);
size_t arena_get_size(Arena* arena);

#endif

