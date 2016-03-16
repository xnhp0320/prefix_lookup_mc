#ifndef __TBM_v4_H__
#define __TBM_v4_H__

#include <stdint.h>
#include "lib/mb_node.h"
#include "lib/bitmap_v4.h"
#include "lib/mm.h"
#include "tbm.h"

//use the bitmap configruation comes from the Will Eatherton paper: Tree bitmap
//use the 16,6,6,4

//you can not adjust the STRIDE value
//if you ajust the other value, the macro LEVEL should be changed.

#define INITIAL_BITS 13 

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
void tbm_search_batch(struct tbm_trie *trie, uint32_t ip[BATCH], void *ret[BATCH], int cnt);
#ifndef COMPRESS_NHI
void tbm_redund_rule_count(struct tbm_trie *trie);
#endif
#endif
