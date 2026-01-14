#include "aether_pqueue.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define DEFAULT_CAPACITY 16
#define GROWTH_FACTOR 2

// Helper macros for heap navigation
#define PARENT(i) (((i) - 1) / 2)
#define LEFT_CHILD(i) (2 * (i) + 1)
#define RIGHT_CHILD(i) (2 * (i) + 2)

PriorityQueue* pqueue_create(size_t initial_capacity,
                            int (*compare)(const void*, const void*),
                            void (*element_free)(void*),
                            void* (*element_clone)(const void*)) {

    if (!compare) return NULL;

    PriorityQueue* pq = (PriorityQueue*)malloc(sizeof(PriorityQueue));
    if (!pq) return NULL;

    if (initial_capacity < DEFAULT_CAPACITY) {
        initial_capacity = DEFAULT_CAPACITY;
    }

    pq->data = (void**)malloc(initial_capacity * sizeof(void*));
    if (!pq->data) {
        free(pq);
        return NULL;
    }

    pq->size = 0;
    pq->capacity = initial_capacity;
    pq->compare = compare;
    pq->element_free = element_free;
    pq->element_clone = element_clone;

    return pq;
}

void pqueue_free(PriorityQueue* pq) {
    if (!pq) return;

    if (pq->data) {
        if (pq->element_free) {
            for (size_t i = 0; i < pq->size; i++) {
                pq->element_free(pq->data[i]);
            }
        }
        free(pq->data);
    }

    free(pq);
}

// Ensure capacity
static bool pqueue_ensure_capacity(PriorityQueue* pq, size_t min_capacity) {
    if (pq->capacity >= min_capacity) {
        return true;
    }

    size_t new_capacity = pq->capacity * GROWTH_FACTOR;
    if (new_capacity < min_capacity) {
        new_capacity = min_capacity;
    }

    void** new_data = (void**)realloc(pq->data, new_capacity * sizeof(void*));
    if (!new_data) {
        return false;
    }

    pq->data = new_data;
    pq->capacity = new_capacity;
    return true;
}

// Swap elements
static void pqueue_swap(PriorityQueue* pq, size_t i, size_t j) {
    void* temp = pq->data[i];
    pq->data[i] = pq->data[j];
    pq->data[j] = temp;
}

// Heapify up (bubble up) - restore heap property upwards
static void pqueue_heapify_up(PriorityQueue* pq, size_t index) {
    while (index > 0) {
        size_t parent = PARENT(index);

        // If current element has higher priority than parent, swap
        if (pq->compare(pq->data[index], pq->data[parent]) < 0) {
            pqueue_swap(pq, index, parent);
            index = parent;
        } else {
            break;
        }
    }
}

// Heapify down (bubble down) - restore heap property downwards
static void pqueue_heapify_down(PriorityQueue* pq, size_t index) {
    while (true) {
        size_t smallest = index;
        size_t left = LEFT_CHILD(index);
        size_t right = RIGHT_CHILD(index);

        // Find smallest among node and its children
        if (left < pq->size && pq->compare(pq->data[left], pq->data[smallest]) < 0) {
            smallest = left;
        }

        if (right < pq->size && pq->compare(pq->data[right], pq->data[smallest]) < 0) {
            smallest = right;
        }

        // If heap property is satisfied, stop
        if (smallest == index) {
            break;
        }

        pqueue_swap(pq, index, smallest);
        index = smallest;
    }
}

bool pqueue_insert(PriorityQueue* pq, void* element) {
    if (!pq) return false;

    if (!pqueue_ensure_capacity(pq, pq->size + 1)) {
        return false;
    }

    // Add element at end
    pq->data[pq->size] = element;
    pq->size++;

    // Restore heap property
    pqueue_heapify_up(pq, pq->size - 1);

    return true;
}

void* pqueue_extract(PriorityQueue* pq) {
    if (!pq || pq->size == 0) return NULL;

    void* result = pq->data[0];

    // Move last element to root
    pq->data[0] = pq->data[pq->size - 1];
    pq->size--;

    // Restore heap property
    if (pq->size > 0) {
        pqueue_heapify_down(pq, 0);
    }

    return result;
}

void* pqueue_peek(PriorityQueue* pq) {
    if (!pq || pq->size == 0) return NULL;
    return pq->data[0];
}

size_t pqueue_size(PriorityQueue* pq) {
    return pq ? pq->size : 0;
}

bool pqueue_is_empty(PriorityQueue* pq) {
    return pq ? (pq->size == 0) : true;
}

void pqueue_clear(PriorityQueue* pq) {
    if (!pq) return;

    if (pq->element_free) {
        for (size_t i = 0; i < pq->size; i++) {
            pq->element_free(pq->data[i]);
        }
    }

    pq->size = 0;
}

bool pqueue_contains(PriorityQueue* pq, const void* element,
                    bool (*equals)(const void*, const void*)) {
    if (!pq || !equals) return false;

    for (size_t i = 0; i < pq->size; i++) {
        if (equals(pq->data[i], element)) {
            return true;
        }
    }

    return false;
}

// Build heap from array - O(n) using Floyd's algorithm
PriorityQueue* pqueue_from_array(void** elements, size_t count,
                                int (*compare)(const void*, const void*),
                                void (*element_free)(void*),
                                void* (*element_clone)(const void*)) {

    PriorityQueue* pq = pqueue_create(count, compare, element_free, element_clone);
    if (!pq) return NULL;

    // Copy elements
    for (size_t i = 0; i < count; i++) {
        pq->data[i] = element_clone ? element_clone(elements[i]) : elements[i];
    }
    pq->size = count;

    // Heapify from bottom up (Floyd's algorithm)
    for (int i = (int)count / 2 - 1; i >= 0; i--) {
        pqueue_heapify_down(pq, (size_t)i);
    }

    return pq;
}

// Common comparators
int pqueue_compare_int_min(const void* a, const void* b) {
    long ia = (long)(intptr_t)a;
    long ib = (long)(intptr_t)b;
    return (ia < ib) ? -1 : (ia > ib) ? 1 : 0;
}

int pqueue_compare_int_max(const void* a, const void* b) {
    // Invert comparison for max-heap
    return -pqueue_compare_int_min(a, b);
}

