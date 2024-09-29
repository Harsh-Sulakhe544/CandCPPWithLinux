#pragma once // comple only once and make only the required changes 

#include <stddef.h> // size_t 
#include <stdint.h> // uint64_t 

// hashtable node, should be embedded into the payload
struct HNode {
    HNode *next = NULL;
    uint64_t hcode = 0; // cached hash value
};

struct HTab {
    HNode **tab = NULL; // array of `HNode *`
    size_t mask = 0;    // 2^n - 1
    size_t size = 0;
};

// the real hashtable interface.
// it uses 2 hashtables for progressive resizing.
struct HMap {
    HTab ht1;   // newer
    HTab ht2;   // older
    size_t resizing_pos = 0;
};

// function definitions 
HNode *hm_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *));
void hm_insert(HMap *hmap, HNode *node);
HNode *hm_pop(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *));
size_t hm_size(HMap *hmap);
void hm_destroy(HMap *hmap);

