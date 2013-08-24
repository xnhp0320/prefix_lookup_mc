#ifndef __BITMAP_v4_H__
#define __BITMAP_v4_H__

#include <stdint.h>
#include "mm_color.h"
#include "mb_node.h"



//use the bitmap configruation comes from the Will Eatherton paper: Tree bitmap
//use the 13,4,4,4,3 trie

//you can not adjust the STRIDE value
//if you ajust the other value, the macro LEVEL should be changed.

#define LENGTH 32
#define INITIAL_BITS 13 
#define LEVEL 5

#define UPDATE_LEVEL 16 


#define INLINE __attribute__((always_inline))


struct lookup_trie{
    struct init_node_v6 *init;
    struct mb_node_v6 up_aux;
    struct mc mm;
};




struct next_hop_info * search(struct lookup_trie *trie, uint32_t ip);
void insert_prefix(struct lookup_trie *trie, uint32_t ip, int cidr, struct next_hop_info *nhi);
//void initial_table_init();
//void init_bits_lookup();

int init_lookup_trie(struct lookup_trie *trie);
void delete_prefix(struct lookup_trie *trie, uint32_t ip, int cidr, 
        void (*destroy_nhi)(struct next_hop_info *nhi));



//return 1 means the prefix exists.
int prefix_exist(struct lookup_trie *trie, uint32_t ip, uint8_t cidr);
void print_prefix(struct lookup_trie *trie, void (*print_next_hop)(struct next_hop_info *nhi));
void destroy_trie(struct lookup_trie *trie, void (*destroy_nhi)(struct next_hop_info* nhi));
void mem_alloc_stat();

void mem_op();

void level_memory(struct lookup_trie *trie);
#define USE_LAZY
//#define UP_STATS
//#define USE_MM

#endif
