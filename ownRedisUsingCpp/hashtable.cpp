#include <assert.h> // for assert() 
#include <stdlib.h> // size_t 
#include "hashtable.h"

// HASHTABLE - INIT -- to create a hash-table 
static void h_init(HTab *htab, size_t n) {
// n must be a power of 2
    assert(n > 0 && ((n - 1) & n) == 0);
    htab->tab = (HNode **)calloc(sizeof(HNode *), n); // ALLOCATE WITH 0 FOR ALL N CHAINED-NODES - FOR EACH NODE WRT KEY 
    htab->mask = n - 1; // cuz 0 based indexing 
    htab->size = 0;
}

// hashtable insertion
static void h_insert(HTab *htab, HNode *node) {
    size_t pos = node->hcode & htab->mask;  // slot index (means, the exact slot to insert the current node value  ) , get the position 
    HNode *next = htab->tab[pos];           // prepend the list
    node->next = next; // point to the next node in the same slot to insert 
    htab->tab[pos] = node; // inserting the value 
    htab->size++; // increase of size 
}

// look-up , "eq" is callback function to check equality   
// Pay attention to the return value. It returns the address of
// the parent pointer that owns the target node,
// which can be used to delete the target node.

static HNode **h_lookup(HTab *htab, HNode *key, bool (*eq)(HNode * , HNode *  ) ) {
	// if htab is null , not present 
	if(!htab->tab){
		return NULL; 
	}
	
	// PERFORM A STANDARD LINKED-LIST SEARCH 
	
	// get the hashed-code for the key 
	size_t pos = key->hcode & htab->mask; 
	HNode **from = &htab->tab[pos];   // incoming pointer to the result
	
	for(HNode *curr; (curr = *from)!= NULL; from = &curr->next) {
		if(curr->hcode == key->hcode && eq(curr,key)) {
			return from; 
		}
	}
	return NULL;
}

// remove a node from the chain
static HNode *h_detach(HTab *htab, HNode **from) {
    HNode *node = *from; // like node is current-node 
    *from = node->next; // move from to next 
    htab->size--; // cuz 1 node got reduced 
    return node;
}

const size_t k_resizing_work = 128; // constant work- maximum number of nodes that can be processed in a single resizing operation.

static void hm_help_resizing(HMap *hmap) {
    size_t nwork = 0; // used to track the number of nodes that have been processed during the current resizing operation.
    while (nwork < k_resizing_work && hmap->ht2.size > 0) {
        // scan for nodes from ht2 and move them to ht1
        HNode **from = &hmap->ht2.tab[hmap->resizing_pos];
        
        // IF NULL , JUST INCREMENT TO THE NEXT-LOCATION 
        if (!*from) {
            hmap->resizing_pos++; // NEXT LOCATION 
            continue;
        }

        h_insert(&hmap->ht1, h_detach(&hmap->ht2, from));
        nwork++;
    }
	
	// if size is 0 , free 
    if (hmap->ht2.size == 0 && hmap->ht2.tab) {
        // done
        free(hmap->ht2.tab);
        hmap->ht2 = HTab{};
    }
}

const size_t k_max_load_factor = 8; // max-8 buckets in for each slot 

static void hm_start_resizing(HMap *hmap) {
    assert(hmap->ht2.tab == NULL); // error if nothing is there in hashtable - 2
    
    // create a bigger hashtable and swap them
    hmap->ht2 = hmap->ht1; // h1 becomes h2 , so that we can add newer elements in new-(2X * h1)
    h_init(&hmap->ht1, (hmap->ht1.mask + 1) * 2); // *2 , cuz , double the size of h1 hashtable and then allocate 
    hmap->resizing_pos = 0;
}

// The Lookup checks both tables :
HNode *hm_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *)) {
    hm_help_resizing(hmap); // check resizing 
    HNode **from = h_lookup(&hmap->ht1, key, eq); // search a node in ht1 
    from = from ? from : h_lookup(&hmap->ht2, key, eq); // search a node in ht2 
    return from ? *from : NULL;
}

// insert into hash-map 
void hm_insert(HMap *hmap, HNode *node) {
    if (!hmap->ht1.tab) {
        h_init(&hmap->ht1, 4);  // 1. Initialize the table if it is empty.
    }
    
    h_insert(&hmap->ht1, node); // 2. Insert the key into the newer table.

    if (!hmap->ht2.tab) {       // 3. Check the load factor
        size_t load_factor = hmap->ht1.size / (hmap->ht1.mask + 1);
        // CHECK MORE THAN 8 ELEMENTS FOR A BUCKET 
        if (load_factor >= k_max_load_factor) {
            hm_start_resizing(hmap);    // create a larger table
        }
    }
    
    hm_help_resizing(hmap);     // 4. Move some keys into the newer table.
}

// Deletion of element in a bucket is omitted because it’s trivial.

// delete ele from map 
HNode *hm_pop(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *)) {
    hm_help_resizing(hmap);
    if (HNode **from = h_lookup(&hmap->ht1, key, eq)) {
        return h_detach(&hmap->ht1, from);
    }
    if (HNode **from = h_lookup(&hmap->ht2, key, eq)) {
        return h_detach(&hmap->ht2, from);
    }
    return NULL;
}

// change the size of hashmap 
size_t hm_size(HMap *hmap) {
    return hmap->ht1.size + hmap->ht2.size;
}

// free the memory for both the maps  : h1 and h2 
void hm_destroy(HMap *hmap) {
    free(hmap->ht1.tab);
    free(hmap->ht2.tab);
    *hmap = HMap{};
}
