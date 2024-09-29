// for caching servers with TTL , instead of doubly-list (dll)
#pragma once

#include <stddef.h> // size_t 
#include <stdint.h> // uint64_t 

struct HeapItem {
    uint64_t val = 0;
    size_t *ref = NULL; // to hold the index of the item in the heap. , for updating index of each element dynamically 
};

void heap_update(HeapItem *a, size_t pos, size_t len);
