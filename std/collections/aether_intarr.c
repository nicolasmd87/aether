/* IntArray — a fixed-size packed-int buffer.
 *
 * Intended for hot-path int-keyed lookup (DP tables, flat index
 * buffers) where going through std.list's void*-boxed items would
 * cost an allocation per entry and chase an extra pointer.
 *
 * Shape is as minimal as it gets: heap-allocate size*int, store a
 * size next to it, expose direct index ops plus a bulk-fill. No
 * amortised growth, no capacity-beyond-size. Callers that need
 * resize on write use std.list. */

#include "aether_collections.h"
#include <stdlib.h>
#include <string.h>

struct IntArray {
    int* data;
    int  size;
};

IntArray* intarr_new_raw(int size) {
    if (size < 0) return NULL;
    IntArray* arr = (IntArray*)malloc(sizeof(*arr));
    if (!arr) return NULL;
    arr->size = size;
    if (size == 0) {
        arr->data = NULL;   // legal empty array; intarr_free still OK
        return arr;
    }
    arr->data = (int*)calloc((size_t)size, sizeof(int));
    if (!arr->data) { free(arr); return NULL; }
    return arr;
}

IntArray* intarr_new_filled_raw(int size, int init) {
    IntArray* arr = intarr_new_raw(size);
    if (!arr) return NULL;
    for (int i = 0; i < size; i++) arr->data[i] = init;
    return arr;
}

int intarr_size(IntArray* arr) {
    return arr ? arr->size : -1;
}

int intarr_get_raw(IntArray* arr, int i) {
    if (!arr || i < 0 || i >= arr->size) return 0;
    return arr->data[i];
}

void intarr_set_raw(IntArray* arr, int i, int value) {
    if (!arr || i < 0 || i >= arr->size) return;
    arr->data[i] = value;
}

int intarr_get_unchecked(IntArray* arr, int i) {
    return arr->data[i];
}

void intarr_set_unchecked(IntArray* arr, int i, int value) {
    arr->data[i] = value;
}

void intarr_fill(IntArray* arr, int value) {
    if (!arr || arr->size == 0) return;
    for (int i = 0; i < arr->size; i++) arr->data[i] = value;
}

void intarr_free(IntArray* arr) {
    if (!arr) return;
    free(arr->data);
    free(arr);
}
