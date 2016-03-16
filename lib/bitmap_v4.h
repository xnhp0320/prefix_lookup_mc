#ifndef __BITMAP_v4_H__
#define __BITMAP_v4_H__

#include <stdint.h>
#include "mb_node.h"
#include "bitmap.h"
#include "mm.h"

#define LENGTH 32
#define LEVEL ((LENGTH/STRIDE) + 1)
#define UPDATE_LEVEL ((LENGTH/STRIDE) + 1) 


#define BATCH 32 


int bitmap_traverse_branch(struct mb_node *node, 
        uint32_t ip, int cidr, 
        traverse_func func, 
        void *user_data);

int bitmap_insert_prefix(
        struct mb_node *node,
        struct mm *m, 
        uint32_t ip, int cidr, 
        void *nhi);

int bitmap_delete_prefix(struct mb_node *n, struct mm *m,  
        uint32_t ip, int cidr, void (*destroy_nhi)(void *nhi));

void *bitmap_do_search_lazy(struct mb_node *n, uint32_t ip);
void *bitmap_do_search(struct mb_node *n, uint32_t ip);

uint8_t bitmap_detect_overlap(struct mb_node *n, 
        uint32_t ip, 
        uint8_t cidr, void **nhi_over);

uint8_t bitmap_detect_overlap_generic(struct mb_node *n, 
        uint32_t ip, uint8_t cidr, 
        uint32_t bits_limit, void **nhi_over);

void bitmap_print_all_prefix(struct mb_node *n, 
        void (*print_next_hop)(void *nhi));

int bitmap_prefix_exist(struct mb_node *n, 
        uint32_t ip, uint8_t cidr);

void bitmap_destroy_trie(struct mb_node *n, 
        struct mm *m, void (*destroy_nhi)(void *nhi));

void bitmap_do_search_lazy_batch(struct mb_node *n[BATCH], 
        uint32_t ip[BATCH], void *ret[BATCH], int cnt);

void bitmap_mb_node_iter(struct mb_node *node, uint32_t ip, uint32_t left_bits, 
                         uint32_t cur_cidr, void (*trie_traverse_func)(uint32_t ip, uint32_t cidr, void *nhi, void *user),
                         void *userdata);

void bitmap_redund_rule(struct mb_node *node, uint32_t ip, uint32_t left_bits, 
        uint32_t cur_cidr, uint32_t *redund_rule);

#endif
