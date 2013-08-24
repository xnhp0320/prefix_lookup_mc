#ifndef __BITMAP_v6_H__
#define __BITMAP_v6_H__

#include <stdint.h>
#include <stdlib.h>
#include "mm_color.h"
#include "mb_node.h"
#include <arpa/inet.h>



//Tree bitmap
//use the 64-bit bitmap for ipv6 lookup


#define LENGTH_v6 128 
#define INITIAL_BITS_v6 13 
#define LEVEL_v6 24

#define UPDATE_LEVEL_v6 24 


#define INLINE __attribute__((always_inline))

struct ip_v6 {
#if BYTE_ORDER == __LITTLE_ENDIAN
    uint64_t iplo;
    uint64_t iphi;
#else
    uint64_t iphi;
    uint64_t iplo;
#endif
};


struct lookup_trie_v6{
    struct init_node_v6 *init;
    struct mb_node_v6 up_aux;
    struct mc mm;
};



struct next_hop_info * search_v6(struct lookup_trie_v6 *trie, struct ip_v6 *ip);
void insert_prefix_v6(struct lookup_trie_v6 *trie, struct ip_v6 ip, int cidr, struct next_hop_info *nhi);

int init_lookup_trie_v6(struct lookup_trie_v6 *trie);
void delete_prefix_v6(struct lookup_trie_v6 *trie, struct ip_v6 ip, int cidr, 
        void (*destroy_nhi)(struct next_hop_info *));



//return 1 means the prefix exists.
int prefix_exist_v6(struct lookup_trie_v6 *trie, struct ip_v6 ip, uint8_t cidr);
void print_prefix_v6(struct lookup_trie_v6 *trie, void (*print_next_hop)(struct next_hop_info *nhi));
void destroy_trie_v6(struct lookup_trie_v6 *trie, void (*destroy_nhi)(struct next_hop_info* nhi));
void mem_alloc_stat_v6();


struct mem_stats_v6 mem_trie_v6(struct lookup_trie_v6 *trie);

void mem_op_v6();
void hton_ipv6(struct in6_addr *ip);

#define USE_LAZY
//#define UP_STATS
//#define USE_MM
#define DEBUG_MEMORY_ALLOC

#endif
