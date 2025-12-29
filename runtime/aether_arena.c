#include "aether_arena.h"
#include <stdlib.h>
#include <string.h>

#define ARENA_DEFAULT_SIZE (1024 * 1024)
#define ALIGN_UP(n, align) (((n) + (align) - 1) & ~((align) - 1))

Arena* arena_create(size_t size) {
    if (size == 0) {
        size = ARENA_DEFAULT_SIZE;
    }
    
    Arena* arena = (Arena*)malloc(sizeof(Arena));
    if (!arena) return NULL;
    
    arena->memory = (char*)malloc(size);
    if (!arena->memory) {
        free(arena);
        return NULL;
    }
    
    arena->size = size;
    arena->used = 0;
    arena->next = NULL;
    
    return arena;
}

void* arena_alloc(Arena* arena, size_t bytes) {
    return arena_alloc_aligned(arena, bytes, 8);
}

void* arena_alloc_aligned(Arena* arena, size_t bytes, size_t alignment) {
    if (!arena || bytes == 0) return NULL;
    
    size_t aligned_used = ALIGN_UP(arena->used, alignment);
    
    if (aligned_used + bytes > arena->size) {
        if (!arena->next) {
            size_t new_size = arena->size;
            if (bytes > new_size) {
                new_size = ALIGN_UP(bytes, ARENA_DEFAULT_SIZE);
            }
            arena->next = arena_create(new_size);
        }
        return arena_alloc_aligned(arena->next, bytes, alignment);
    }
    
    void* ptr = arena->memory + aligned_used;
    arena->used = aligned_used + bytes;
    
    return ptr;
}

void arena_reset(Arena* arena) {
    if (!arena) return;
    
    arena->used = 0;
    
    if (arena->next) {
        arena_destroy(arena->next);
        arena->next = NULL;
    }
}

void arena_destroy(Arena* arena) {
    while (arena) {
        Arena* next = arena->next;
        free(arena->memory);
        free(arena);
        arena = next;
    }
}

ArenaScope arena_begin(Arena* arena) {
    ArenaScope scope;
    scope.arena = arena;
    scope.restore_point = arena ? arena->used : 0;
    return scope;
}

void arena_end(ArenaScope scope) {
    if (scope.arena) {
        scope.arena->used = scope.restore_point;
    }
}

size_t arena_get_used(Arena* arena) {
    if (!arena) return 0;
    
    size_t total = arena->used;
    Arena* current = arena->next;
    while (current) {
        total += current->used;
        current = current->next;
    }
    
    return total;
}

size_t arena_get_size(Arena* arena) {
    if (!arena) return 0;
    
    size_t total = arena->size;
    Arena* current = arena->next;
    while (current) {
        total += current->size;
        current = current->next;
    }
    
    return total;
}

