#ifndef __TBM_v6_H__
#define __TBM_v6_H__

#include <stdint.h>
#include <stdlib.h>
#include "lib/bitmap_v6.h"
#include "lib/mb_node.h"
#include "tbm.h"
#include <arpa/inet.h>



//Tree bitmap
//use the 64-bit bitmap for ipv6 lookup

#define INITIAL_BITS_v6 13 

struct tbmv6_trie{
    struct init_node *init;
    struct mb_node up_aux;
    struct mm m;
    struct mm up_m;
};


void * tbmv6_search(struct tbmv6_trie *trie, struct ip_v6 ip);
void tbmv6_insert_prefix(struct tbmv6_trie *trie, struct ip_v6 ip, int cidr, void *nhi);

int tbmv6_init_trie(struct tbmv6_trie *trie);
void tbmv6_delete_prefix(struct tbmv6_trie *trie, struct ip_v6 ip, int cidr, 
        void (*destroy_nhi)(void *));



//return 1 means the prefix exists.
int tbmv6_prefix_exist(struct tbmv6_trie *trie, struct ip_v6 ip, uint8_t cidr);
void tbmv6_print_all_prefix(struct tbmv6_trie *trie, void (*print_next_hop)(void *nhi));
void tbmv6_destroy_trie(struct tbmv6_trie *trie, void (*destroy_nhi)(void* nhi));


#endif
