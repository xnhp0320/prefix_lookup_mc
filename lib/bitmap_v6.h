#ifndef __BITMAP_v6_H__
#define __BITMAP_v6_H__

#include <stdint.h>
#include <stdlib.h>
#include "bitmap.h"
#include "mb_node.h"
#include "mm.h"
#include <arpa/inet.h>



//Tree bitmap
//use the 64-bit bitmap for ipv6 lookup

#define LENGTH_v6 128 
#define LEVEL_v6  ((LENGTH_v6/STRIDE)+1) 
#define UPDATE_LEVEL_v6 ((LENGTH_v6/STRIDE)+1) 

struct ip_v6 {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint64_t iplo;
    uint64_t iphi;
#else
    uint64_t iphi;
    uint64_t iplo;
#endif
};


void * bitmapv6_do_search_lazy(struct mb_node *n, struct ip_v6 ip);
void * bitmapv6_do_search(struct mb_node *n, struct ip_v6 ip);
int bitmapv6_insert_prefix(struct mb_node *n, struct mm *m,  
        struct ip_v6 ip, int cidr, void *nhi);
int bitmapv6_delete_prefix(struct mb_node *n, struct mm *m, 
        struct ip_v6 ip, int cidr, 
        void (*destroy_nhi)(void *));

uint8_t bitmapv6_detect_overlap(struct mb_node *n, 
        struct ip_v6 ip, 
        uint8_t cidr, void **nhi_over);

uint8_t bitmapv6_detect_overlap_generic(struct mb_node *n, 
        struct ip_v6 ip, uint8_t cidr, 
        uint32_t bits_limit, void **nhi_over);


//return 1 means the prefix exists.
int bitmapv6_prefix_exist(struct mb_node *node, struct ip_v6 ip, uint8_t cidr);
void bitmapv6_print_all_prefix(struct mb_node *node, 
        void (*print_next_hop)(void *nhi));
void bitmapv6_destroy_trie(struct mb_node *node, struct mm *m, 
        void (*destroy_nhi)(void* nhi));

int bitmapv6_traverse_trie(struct mb_node *node, struct ip_v6 ip, int cidr,
        traverse_func func, void *user_data);

void hton_ipv6(struct in6_addr *ip);

void lshift_ipv6(struct ip_v6 *ip, uint8_t bits);
void rshift_ipv6(struct ip_v6 *ip, uint8_t bits);

#endif
