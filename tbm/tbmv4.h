#ifndef __TBM_H__
#define __TBM_H__

#include <stdint.h>
#include "lib/mb_node.h"
#include "lib/mm.h"

//use the bitmap configruation comes from the Will Eatherton paper: Tree bitmap
//use the 16,6,6,4

//you can not adjust the STRIDE value
//if you ajust the other value, the macro LEVEL should be changed.

#define INITIAL_BITS 16 

#define INIT_HAS_A_CHILD 0x00000001
#define PREFIX_HI 16

union init_entry{
    struct mb_node node;
    void *ptr;
};

struct init_node {
    uint32_t flags;
    union init_entry e;
};

struct tbm_trie{
    struct init_node *init;
    struct mm m;
    struct mb_node up_aux;
    struct mm up_m;
};


int tbm_init_trie(struct tbm_trie *trie);
void* tbm_search(struct tbm_trie *trie, uint32_t ip);
int tbm_insert_prefix(struct tbm_trie *trie, uint32_t ip, int cidr, void *nhi);

int tbm_delete_prefix(struct tbm_trie *trie, uint32_t ip, int cidr, 
        void (*destroy_nhi)(void *nhi));

void tbm_destroy_trie(struct tbm_trie *trie, void (*destroy_nhi)(void* nhi));
void tbm_print_all_prefix(struct tbm_trie *trie, void (*print_next_hop)(void *nhi));
int tbm_prefix_exist(struct tbm_trie *trie, uint32_t ip, int cidr);

#endif
