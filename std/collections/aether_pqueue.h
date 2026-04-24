#ifndef AETHER_PQUEUE_H
#define AETHER_PQUEUE_H

#include <stddef.h>
#include <stdbool.h>

// Priority Queue (Binary Heap) implementation
// Min-heap or Max-heap based on comparator

typedef struct {
    void** data;
    size_t size;
    size_t capacity;
    int (*compare)(const void*, const void*); // <0 if a<b, 0 if a==b, >0 if a>b
    void (*element_free)(void*);
    void* (*element_clone)(const void*);
} PriorityQueue;

// Creation and destruction
PriorityQueue* aether_pqueue_create(size_t initial_capacity,
                            int (*compare)(const void*, const void*),
                            void (*element_free)(void*),
                            void* (*element_clone)(const void*));
void aether_pqueue_free(PriorityQueue* pq);

// Core operations - O(log n)
bool aether_pqueue_insert(PriorityQueue* pq, void* element);
void* aether_pqueue_extract(PriorityQueue* pq);  // Extract min/max

// Query operations - O(1)
void* aether_pqueue_peek(PriorityQueue* pq);     // Peek at min/max
size_t aether_pqueue_size(PriorityQueue* pq);
bool aether_pqueue_is_empty(PriorityQueue* pq);

// Utility
void aether_pqueue_clear(PriorityQueue* pq);
bool aether_pqueue_contains(PriorityQueue* pq, const void* element, 
                    bool (*equals)(const void*, const void*));

// Heapify from array
PriorityQueue* aether_pqueue_from_array(void** elements, size_t count,
                                int (*compare)(const void*, const void*),
                                void (*element_free)(void*),
                                void* (*element_clone)(const void*));

// Common comparators
int aether_pqueue_compare_int_min(const void* a, const void* b);  // Min-heap for ints
int aether_pqueue_compare_int_max(const void* a, const void* b);  // Max-heap for ints

#endif // AETHER_PQUEUE_H

