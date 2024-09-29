#include <stddef.h> // size_t 
#include <stdint.h>
#include "heap.h"

// parent ele formula 
static size_t heap_parent(size_t i) {
    return (i + 1) / 2 - 1;
}

// left ele formula
static size_t heap_left(size_t i) {
    return i * 2 + 1;
}

// right ele formula
static size_t heap_right(size_t i) {
    return i * 2 + 2;
}

// to ensure properties of heap , after new insertion : The heap_up function is designed to maintain the properties of a min-heap by moving an element up the heap until the heap property is restored
static void heap_up(HeapItem *a, size_t pos) {
    HeapItem t = a[pos]; // new-element to insert 
    
    // check untill parent is greater 
    while (pos > 0 && a[heap_parent(pos)].val > t.val) {
        // swap with the parent
        a[pos] = a[heap_parent(pos)];
        *a[pos].ref = pos; // Update the reference of the parent
        pos = heap_parent(pos); // Move up to the parent's position
    }
    
    a[pos] = t; // Place the original item in its new position
    *a[pos].ref = pos; // Update the reference of the new position
}

static void heap_down(HeapItem *a, size_t pos, size_t len) {
    HeapItem t = a[pos];
    while (true) {
        // find the smallest one among the parent and their kids
        size_t l = heap_left(pos);
        size_t r = heap_right(pos);
        size_t min_pos = -1;
        size_t min_val = t.val;
        
        // check left child 
        if (l < len && a[l].val < min_val) {
            min_pos = l;
            min_val = a[l].val;
        }
        
        // check right child 
        if (r < len && a[r].val < min_val) {
            min_pos = r;
        }
        
        // if no updation was required 
        if (min_pos == (size_t)-1) {
            break;
        }
        
        // swap with the kid
        a[pos] = a[min_pos];
        *a[pos].ref = pos; // update the reference of parent 
        pos = min_pos; // move down to the parents position 
    }
    
    a[pos] = t;
    *a[pos].ref = pos;
}

// The heap_update is the heap function for updating a position. It is used for updating, inserting, and deleting.
void heap_update(HeapItem *a, size_t pos, size_t len) {
    if (pos > 0 && a[heap_parent(pos)].val > a[pos].val) {
        heap_up(a, pos);
    } else {
        heap_down(a, pos, len);
    }
}
