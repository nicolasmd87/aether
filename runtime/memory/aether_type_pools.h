// Type-Specific Memory Pools Header
// Zero-branch allocation for hot message types
// Generated at compile time for each message type

#ifndef AETHER_TYPE_POOLS_H
#define AETHER_TYPE_POOLS_H

#include <stddef.h>
#include <stdint.h>
#include "../utils/aether_compiler.h"

// Pool configuration
#define TYPE_POOL_SIZE 1024  // Must be power of 2
#define TYPE_POOL_MASK (TYPE_POOL_SIZE - 1)

// Generic type pool structure template
// The compiler will generate specific versions for each message type
#define DECLARE_TYPE_POOL(TypeName) \
    typedef struct { \
        TypeName pool[TYPE_POOL_SIZE]; \
        uint16_t free_list[TYPE_POOL_SIZE]; \
        uint16_t head; \
        uint16_t count; \
    } TypeName##Pool; \
    \
    static inline void TypeName##Pool_init(TypeName##Pool* p) { \
        p->head = 0; \
        p->count = TYPE_POOL_SIZE; \
        for (uint16_t i = 0; i < TYPE_POOL_SIZE; i++) { \
            p->free_list[i] = i; \
        } \
    } \
    \
    static inline TypeName* TypeName##Pool_alloc(TypeName##Pool* p) { \
        if (unlikely(p->count == 0)) { \
            return NULL; \
        } \
        uint16_t idx = p->free_list[p->head]; \
        p->head = (p->head + 1) & TYPE_POOL_MASK; \
        p->count--; \
        return &p->pool[idx]; \
    } \
    \
    static inline void TypeName##Pool_free(TypeName##Pool* p, TypeName* obj) { \
        if (unlikely(!obj)) return; \
        uint16_t idx = (uint16_t)(obj - p->pool); \
        if (unlikely(idx >= TYPE_POOL_SIZE)) return; \
        uint16_t tail = (p->head + p->count) & TYPE_POOL_MASK; \
        p->free_list[tail] = idx; \
        p->count++; \
    }

// Thread-local pool declaration
#define DECLARE_TLS_POOL(TypeName) \
    static AETHER_TLS TypeName##Pool TypeName##_tls_pool = {0}; \
    static AETHER_TLS int TypeName##_pool_initialized = 0; \
    \
    static inline TypeName* TypeName##_alloc(void) { \
        if (unlikely(!TypeName##_pool_initialized)) { \
            TypeName##Pool_init(&TypeName##_tls_pool); \
            TypeName##_pool_initialized = 1; \
        } \
        return TypeName##Pool_alloc(&TypeName##_tls_pool); \
    } \
    \
    static inline void TypeName##_free(TypeName* obj) { \
        TypeName##Pool_free(&TypeName##_tls_pool, obj); \
    }

#endif // AETHER_TYPE_POOLS_H
