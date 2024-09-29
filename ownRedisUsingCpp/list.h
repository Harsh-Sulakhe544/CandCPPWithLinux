#pragma once
// timers for idle connections

#include <stddef.h> // size_t 

// using doubly-linked-list 
struct DList {
    DList *prev = NULL;
    DList *next = NULL;
};

// initializing  
inline void dlist_init(DList *node) {
    node->prev = node->next = node;
}

// check the list is empty 
inline bool dlist_empty(DList *node) {
    return node->next == node; // returns true if the list is empty 
}

// delete the node 
inline void dlist_detach(DList *node) {
    DList *prev = node->prev;
    DList *next = node->next;
    prev->next = next;
    next->prev = prev;
}

// insert before recieves a target pos , to insert before , rookie-new-recruit 
inline void dlist_insert_before(DList *target, DList *rookie) {
    DList *prev = target->prev;
    prev->next = rookie;
    rookie->prev = prev;
    rookie->next = target;
    target->prev = rookie;
}
