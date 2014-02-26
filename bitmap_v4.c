#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "bitmap_v4.h"
#include "mb_node.h"
#include "hash.h"
#include "hmap.h"
#include "fast_lookup.h"
#include <arpa/inet.h>


int init_lookup_trie(struct lookup_trie *trie)
{
    if (!trie) {
        printf("Wrong argument\n");
        return -1;
    }

    fast_table_init();

    trie->init = (struct init_node_v6*)calloc(1,(1 << INITIAL_BITS) * sizeof(struct init_node_v6));

    trie->up_aux.external= 0;
    trie->up_aux.internal= 0;
    trie->up_aux.child_ptr=NULL;

    if (!trie->init) {
        perror("no memory:");
        exit(-1);
    }

#ifdef USE_MM
    int ret;
    //8 colors
    trie->mm.cs[0].size = 32 * 1024;
    //22 colors
    trie->mm.cs[1].size = 22 * 4 * 1024;
    //2 colors
    trie->mm.cs[2].size = 2 * 4 * 1024;
    ret = mc_init(&trie->mm);
    if (ret == -1) 
        exit(-1);
#else
    memset(&trie->mm, 0, sizeof(struct mc));
#endif

    return 0;
}


//notes: cidr > 0 

void insert_entry(
        struct lookup_trie *trie,
        struct mb_node_v6 *node,
        uint32_t ip, int cidr, 
        struct next_hop_info *nhi,
        int use_mm
        )
{
    uint8_t pos;
    uint8_t stride;
    uint8_t level =0;
    struct next_hop_info **i;

    for (;;) {

        if (cidr < STRIDE) {
            // if the node has the prefix already
            //need to be atomic
            //
            stride = ip >> (LENGTH - cidr);
            pos = count_inl_bitmap(stride, cidr);
            if (test_bitmap(node->internal, pos)) {
                //already has the rule, need to update the rule
                i = (struct next_hop_info**)node->child_ptr - count_ones(node->internal, pos) -1;
                *i = nhi;
                return;
            }
            else {
                update_inl_bitmap(node, pos);
                //rules pos starting at 1, so add 1 to offset
                pos = count_ones(node->internal, pos) + 1;
                extend_rule(&trie->mm, node, pos, level, nhi, use_mm); 
                break;
            }
        }
        //push the "cidr == stride" node into the next child
        else {
            stride = ip >> (LENGTH - STRIDE);
            pos = count_enl_bitmap(stride);

            if (test_bitmap(node->external, pos)) {
                node = (struct mb_node_v6*) node->child_ptr + count_ones(node->external, pos); 
            } 
            else {

                update_enl_bitmap(node, pos);
                //iteration
                //child pos starting at 0, so add 0 to offset
                pos = count_ones(node->external, pos);
                node = extend_child(&trie->mm, node, level, pos, use_mm); 
            }
            cidr -= STRIDE;
            ip <<= STRIDE;
            level ++;
        }
    }
}


//assume the init table is completed
//ip = 192.168.1.0
//cidr = 24
//the unmask part of ip need to be zero
//for exmaple input ip = 192.168.1.1 cidr = 24 is illegal, 
//will cause a segfault
//

void insert_prefix(struct lookup_trie *trie, uint32_t ip, int cidr, struct next_hop_info *nhi)
{
    uint32_t index = ip >> (LENGTH - INITIAL_BITS);
    //uint16_t prefix;
    uint32_t i;

    if (cidr == 0) {
        printf("cidr == 0 is a default rules\n");
        return ;
    }


    if (cidr <= INITIAL_BITS) {
        for (i=0; i<(1<<(INITIAL_BITS - cidr)); i++){
            //it could be a child node and a ptr node or an empty node
            //
            //if it's a child node, we compare the prefix_hi 
            //      if the prefix_hi > cidr, it means the nodes already has a prefix longger than the rules we insert
            //      so we ignore the rules
            //      else 
            //          we insert the rules, to overlapped the rule. and we update the prefix_hi
            //else
            //      

            if ( (trie->init)[index + i].flags & INIT_HAS_A_CHILD) {
                if ( ((trie->init)[index + i].flags >> PREFIX_HI) > cidr){
                    continue;
                }
                else {
                    (trie->init)[index + i].flags = (cidr << PREFIX_HI) | INIT_HAS_A_CHILD;
                    insert_entry(trie, &((trie->init)[index + i].e.node), 0, 0, nhi, 1);
#ifdef UP_STATS
                    node_set++;
#endif
                }
            }
            else{
                if (( (trie->init)[index + i].flags >> PREFIX_HI ) > cidr) {
                    continue;
                }
                else {
                    (trie->init)[index + i].flags = (cidr << PREFIX_HI) | INIT_HAS_A_NEXT;
                    (trie->init)[index + i].e.ptr = (struct next_hop_info*)nhi;
#ifdef UP_STATS
                    node_set++;
#endif
                }
            }
            

            //if ( flags_prefix > cidr ) {
            //    //if one init entry has a child, its prefix == INITIAL_BITS + 1
            //    //so cidr <= INITAIL_BITS will not taken the entry
            //    if (flags_prefix == INITIAL_BITS + 1 ) {
            //        insert_entry(&(init[index + i].e.node), 0, 0, nhi);
            //    }
            //    continue;
            //}
            ////need to atomic?
            //init[index + i].flags = (cidr << PREFIX_HI) | INIT_HAS_A_NEXT;
            //init[index + i].e.ptr = (struct next_hop_info*)nhi;
        }
    }
    else {
        //cidr > INITIAL_BITS
        //which means the inserted prefix will follow the init entry to add a branch
        //if the entry has a child, it will "update" the entry, to change the ptr node into the trie node


        //if the entry is a empty one, the code will "insert" a new path.
        if ( (trie->init)[index].flags & INIT_HAS_A_CHILD) {
            // if the node is a ptr node, then we have to change the node to a trie node;
            // and add the entry
            //if(index == 6521) {
            //    printf("here\n");
            //}
            insert_entry(trie, &((trie->init)[index].e.node), ip << INITIAL_BITS, 
                    cidr - INITIAL_BITS, nhi, 1);

#ifdef UP_STATS
            //node_set++;
#endif
        }
        else {

            //it may be a empty node
            //or a ptr node
            //
            //if it's an empty node, it prefix_hi == 0
            //we insert_entry and set the child bit
            //
            //if it's a ptr node, its prefix_hi > 0 and <= INITIAL_BITS
            //then we first insert the ptr and then insert the nexthop
            //set the child bit

            if ( ((trie->init)[index].flags >> PREFIX_HI) > 0 ) {
                // if it's a ptr node

                struct next_hop_info *info  = (trie->init)[index].e.ptr;
                (trie->init)[index].e.ptr = NULL;
                (trie->init)[index].flags &= (~INIT_HAS_A_NEXT);
                insert_entry(trie, &((trie->init)[index].e.node), 0, 0, info, 1);
            }

            (trie->init)[index].flags |= INIT_HAS_A_CHILD;
            insert_entry(trie, &((trie->init)[index].e.node), (uint32_t)(ip << INITIAL_BITS), cidr - INITIAL_BITS,
                    nhi, 1);

#ifdef UP_STATS
            node_set++;
#endif
        }

    }
    // build an extra trie to help the update 
    
#ifdef UP_STATS
    disable_stat();
#endif
    insert_entry(trie, &trie->up_aux, ip, cidr, nhi, 0);

#ifdef UP_STATS
    enable_stat();
#endif
}

struct trace{
    struct mb_node_v6 *node;
    uint32_t pos;
};



int update_nodes(struct mc *mm, struct trace *t, int total, int use_mm)
{
    int i;
    int node_need_to_del = 0;
    for(i=total;i >= 0;i--){
        if(i==total){
            reduce_rule(mm, t[i].node, count_ones((t[i].node)->internal, t[i].pos) + 1, i, use_mm);
            clear_bitmap(&(t[i].node)->internal, (t[i].pos));
            if((t[i].node)->internal == 0 && (t[i].node)->external == 0)
            {
                node_need_to_del = 1;
            }
        }
        else{
            if(node_need_to_del){
                reduce_child(mm, t[i].node, count_ones((t[i].node)->external, t[i].pos), i, use_mm);
                clear_bitmap(&(t[i].node)->external, (t[i].pos));
            }
            if((t[i].node)->internal == 0 && (t[i].node)->external == 0){
                node_need_to_del = 1;
            }
            else{
                node_need_to_del = 0;
            }
        }
    }
    return node_need_to_del;


}

typedef void (*traverse_func) (struct mb_node_v6 *node, uint32_t cur_ip, int cidr, void *data);

static void traverse_trie(struct lookup_trie *trie, struct mb_node_v6 *node, uint32_t ip, int cidr,
        traverse_func func, void *user_data)
{

    uint8_t pos;
    uint8_t stride;

    for (;;) {

        if (cidr < STRIDE) {
            stride = ip >> (LENGTH - cidr);
            pos = count_inl_bitmap(stride, cidr);
            func(node, ip, cidr, user_data);

            break;
        }
        else {
            stride = ip >> (LENGTH - STRIDE);
            pos = count_enl_bitmap(stride);

            func(node, ip, cidr, user_data);
            node = (struct mb_node_v6*) node->child_ptr + count_ones(node->external, pos); 

            cidr -= STRIDE;
            ip <<= STRIDE;
        }
    }
}


typedef void (*destroy_func)(struct next_hop_info*);

struct overlap_nhi_data{
    destroy_func func;
    struct next_hop_info *nhi_near;
};

static void overlap_nhi(struct mb_node_v6 *node, uint32_t ip, int cidr, void *user_data)
{
    if (cidr < STRIDE) {
        uint8_t stride;
        uint8_t pos;
        struct next_hop_info ** nhi;
        struct overlap_nhi_data *ond  = (struct overlap_nhi_data *)(user_data);

        stride = ip >> (LENGTH - cidr);
        pos = count_inl_bitmap(stride, cidr);


        nhi = (struct next_hop_info **)node->child_ptr - 
            count_ones(node->internal, pos) - 1; 

        if (ond->func)
            ond->func(*nhi);
        *nhi = ond->nhi_near;
    }
}

int delete_entry(struct lookup_trie *trie, struct mb_node_v6 *node, uint32_t ip, int cidr, 
        void (*destroy_nhi)(struct next_hop_info *nhi), int use_mm)
{
    uint8_t pos;
    uint8_t stride;
    struct trace trace_node[UPDATE_LEVEL];
    int i = 0;

    for (;;) {

        if (cidr < STRIDE) {
            // if the node has the prefix already
            //need to be atomic
            //
            stride = ip >> (LENGTH - cidr);
            pos = count_inl_bitmap(stride, cidr);
            if (destroy_nhi) {
                struct next_hop_info ** nhi;
                nhi = (struct next_hop_info **)node->child_ptr - 
                    count_ones(node->internal, pos) - 1; 
                destroy_nhi(*nhi);
            }

            trace_node[i].node = node;
            trace_node[i].pos =  pos; 

            break;
        }
        //push the "cidr == stride" node into the next child
        else {
            stride = ip >> (LENGTH - STRIDE);
            pos = count_enl_bitmap(stride);

            
            trace_node[i].node = node;
            trace_node[i].pos  = pos; 

            node = (struct mb_node_v6*) node->child_ptr + count_ones(node->external, pos); 

            cidr -= STRIDE;
            ip <<= STRIDE;
        }
        i++;
    }
    return update_nodes(&trie->mm, trace_node, i, use_mm);
}


uint8_t detect_overlap(struct lookup_trie *trie, uint32_t ip, uint8_t cidr, uint32_t leaf_pushing_bits, struct next_hop_info **nhi_over);

//return 1 means the prefix exists
int prefix_exist(struct lookup_trie *trie, uint32_t ip, uint8_t cidr)
{
    uint8_t pos;
    uint8_t stride;
    struct mb_node_v6 *node = &trie->up_aux;

    for (;;) {

        if (cidr < STRIDE) {
            // if the node has the prefix already
            //need to be atomic
            //
            stride = ip >> (LENGTH - cidr);
            pos = count_inl_bitmap(stride, cidr);
            if (!(test_bitmap(node->internal , pos))) {
                //printf("ERROR: try to delete a non-exist prefix\n");
                return 0;
            }

            break;
        }
        //push the "cidr == stride" node into the next child
        else {
            stride = ip >> (LENGTH - STRIDE);
            pos = count_enl_bitmap(stride);

            if(!test_bitmap(node->external,pos)) {
                //printf("ERROR: try to delete a non-exist prefix\n");
                return 0;
            }

            node = (struct mb_node_v6*) node->child_ptr + count_ones(node->external, pos); 

            cidr -= STRIDE;
            ip <<= STRIDE;
        }
    }

    return 1;
}

void delete_prefix(struct lookup_trie *trie, uint32_t ip, int cidr, void (*destroy_nhi)(struct next_hop_info *nhi))
{
    uint32_t index = ip >> (LENGTH - INITIAL_BITS);
    uint32_t i;
    int ret;
    uint8_t prefix_near; 
    uint8_t prefix;
    struct next_hop_info *nhi_near = NULL;

    if (cidr == 0){
        printf("can't delete: cidr == 0\n");
        return;
    }

    if (cidr <= INITIAL_BITS) {

//13-bits is like leaf pushing a trie into the 13-bits
//if a prefix with length of 8 is deleted
//we should also see if there is /7,/6,...share the same prefix with this
// /8 prefix. if there is, we need push this prefix to the /13 bits

        prefix_near = detect_overlap(trie, ip, cidr,INITIAL_BITS, &nhi_near);
        struct overlap_nhi_data ond;
        ond.nhi_near = nhi_near;

        for (i=0; i<(1<<(INITIAL_BITS - cidr)); i++){
            //if it has a child
            prefix = (trie->init)[index + i].flags >> PREFIX_HI;
            if ( (trie->init)[index + i].flags & INIT_HAS_A_CHILD) {
                if ( prefix > cidr){
                    continue;
                }
                else {
                    if (prefix_near == 0){
                        ret = delete_entry(trie, &((trie->init)[index + i].e.node), 0, 0, destroy_nhi, 1);
                        // we only need to delete once!!!
                        if (!ret) {
                            (trie->init)[index + i].flags = 0 | INIT_HAS_A_CHILD;
                        }
                        else {
                            (trie->init)[index + i].flags = 0;
                            (trie->init)[index + i].e.ptr = NULL;
                        }
#ifdef UP_STATS
                        node_set++;
#endif
                    }
                    else {
                        //printf("insert prefix_near %d\n", prefix_near);

                        (trie->init)[index + i].flags = (prefix_near << PREFIX_HI) | INIT_HAS_A_CHILD;
                        //insert_entry(trie, &((trie->init)[index + i].e.node), 0, 0, nhi_near); 

                        // we only delete the nhi once !!!!!
                        ond.func = destroy_nhi;
                        traverse_trie(trie, &((trie->init)[index + i].e.node), 0, 0, overlap_nhi, &ond); 
#ifdef UP_STATS
                        node_set++;
#endif
                    }
                }
            }
            //if it doesn't has a child
            else{
                if (( prefix ) > cidr) {
                    continue;
                }
                else {
                    if (destroy_nhi) {
                        destroy_nhi((trie->init)[index + i].e.ptr);
                    }

                    if (prefix_near == 0) {
                        (trie->init)[index + i].flags = 0;
                        (trie->init)[index + i].e.ptr = NULL;
                    }
                    else {
                        (trie->init)[index + i].flags = (prefix_near << PREFIX_HI) | INIT_HAS_A_NEXT; 
                        (trie->init)[index + i].e.ptr = nhi_near;
                    }
#ifdef UP_STATS
                    node_set++;
#endif
                }
            }

            if (destroy_nhi) {
                destroy_nhi = NULL;
            }

        }
    }
    else {
        if ( (trie->init)[index].flags & INIT_HAS_A_CHILD) {
            // if the node is a ptr node, then we have to change the node to a trie node;
            // and add the entry
            ret = delete_entry(trie, &((trie->init)[index].e.node), ip << INITIAL_BITS, 
                    cidr - INITIAL_BITS, destroy_nhi, 1);
            if (ret){
                prefix_near = detect_overlap(trie, ip, cidr,INITIAL_BITS, &nhi_near);
                if( !prefix_near) {
                    trie->init[index].flags = 0;
                    trie->init[index].e.ptr = NULL;
                }
                else {
                    trie->init[index].flags = (prefix_near << PREFIX_HI) | INIT_HAS_A_NEXT; 
                    trie->init[index].e.ptr = nhi_near;
                }

#ifdef UP_STATS
                node_set++;
#endif
            }
        }
    }
    //printf("ip %x\n, cidr %d\n", ip, cidr);
#ifdef UP_STATS
    disable_stat();
#endif
    delete_entry(trie, &trie->up_aux, ip, cidr, NULL, 0);

#ifdef UP_STATS
    enable_stat();
#endif
}



   
static struct next_hop_info * do_search(struct mb_node_v6 *n, uint32_t ip)
{
    uint8_t stride;
    int pos;
    struct next_hop_info **longest = NULL;
    //int depth = 1;

    for (;;){
        stride = ip >> (LENGTH - STRIDE);
        pos = tree_function(n->internal, stride);

        if (pos != -1){
            longest = (struct next_hop_info**)n->child_ptr - count_ones(n->internal, pos) - 1;
        }
        if (test_bitmap(n->external, count_enl_bitmap(stride))) {
            //printf("%d %p\n", depth, n);
            n = (struct mb_node_v6*)n->child_ptr + count_ones(n->external, count_enl_bitmap(stride));
            ip = (uint32_t)(ip << STRIDE);
            //depth ++;
        }
        else {
            break;
        }
    }
//    printf("depth %d\n",depth);
    return (longest == NULL)?NULL:*longest;
}



uint8_t detect_overlap(struct lookup_trie *trie, uint32_t ip, uint8_t cidr, uint32_t leaf_pushing_bits, struct next_hop_info **nhi_over)
{
    //uint8_t ret;
    uint8_t stride;
    uint8_t mask = 0;
    uint8_t final_mask = 0;
    uint8_t curr_mask = 0;

    uint8_t step = 0;
    int pos;
    struct next_hop_info **longest = NULL;

    struct mb_node_v6 *n = &trie->up_aux;

    //limit the bits to detect
    int limit;
    int limit_inside;
    int org_limit;
    
    limit = (cidr > leaf_pushing_bits) ? leaf_pushing_bits : cidr;
    org_limit = limit;
    while(limit>0) {

        stride = ip >> (LENGTH - STRIDE);
        limit_inside = (limit > STRIDE) ? STRIDE: limit;
        pos = find_overlap_in_node(n->internal, stride, &mask, limit_inside);

        if (pos != -1){
            curr_mask = step * STRIDE + mask;
            if (curr_mask < org_limit) {  
                final_mask = curr_mask;
                longest = (struct next_hop_info**)n->child_ptr - count_ones(n->internal, pos) - 1;
            }
        }

        limit -= STRIDE;
        step ++;

        if (test_bitmap(n->external, count_enl_bitmap(stride))) {
            //printf("%d %p\n", depth, n);
            n = (struct mb_node_v6*)n->child_ptr + count_ones(n->external, count_enl_bitmap(stride));
            ip = (uint32_t)(ip << STRIDE);
        }
        else {
            break;
        }
    }
    //printf("limit %d, total_mask %d\n", limit, final_mask);

    if(final_mask != 0) {
        *nhi_over = *longest;
    }
    
    //printf("detect_prefix error\n");
    return final_mask;
}


//skip node as far as possible
//don't check the internal bitmap as you have to check

struct lazy_travel {
    struct mb_node_v6 *lazy_p;
    uint32_t stride;
};

static __thread struct lazy_travel lazy_mark[LEVEL]; 

static struct next_hop_info * do_search_lazy(struct mb_node_v6 *n, uint32_t ip)
{
    uint8_t stride;
    int pos;
    struct next_hop_info **longest = NULL;
    int travel_depth = -1;
//    int depth = 1;

    for (;;){
        stride = ip >> (LENGTH - STRIDE);

        if (test_bitmap(n->external, count_enl_bitmap(stride))) {
            travel_depth++;
            lazy_mark[travel_depth].lazy_p = n;
            lazy_mark[travel_depth].stride = stride;
            n = (struct mb_node_v6*)n->child_ptr + count_ones(n->external, count_enl_bitmap(stride));
            ip = (uint32_t)(ip << STRIDE);
//           depth ++;
        }
        else {

            //printf("1 check node %d\n", travel_depth);
            pos = tree_function(n->internal, stride);
            if (pos != -1) {
                longest = (struct next_hop_info**)n->child_ptr - count_ones(n->internal, pos) - 1;
                //already the longest match 
                goto out;
            }

            for(;travel_depth>=0;travel_depth --) {
                n = lazy_mark[travel_depth].lazy_p;
                stride = lazy_mark[travel_depth].stride;
                pos = tree_function(n->internal, stride);
                if (pos != -1) {
                    longest = (struct next_hop_info**)n->child_ptr - count_ones(n->internal, pos) - 1;
                    //printf("2 check node %d\n", travel_depth);
                    //already the longest match 
                    goto out;
                }
            }
            //anyway we have to go out
            break;
        }
    }
//    printf("depth %d\n",depth);
out:
    return (longest == NULL)?NULL:*longest;

}




struct next_hop_info * search(struct lookup_trie *trie, uint32_t ip)
{
    struct init_node_v6 *n = &((trie->init)[ip>>(LENGTH - INITIAL_BITS)]);
    //printf("1 %p\n",n);

    if ( n->flags & INIT_HAS_A_CHILD ) {
#ifndef USE_LAZY
        return do_search(&(n->e.node),(uint32_t)(ip<<INITIAL_BITS));
#else
        return do_search_lazy(&(n->e.node),(uint32_t)(ip<<INITIAL_BITS));
#endif

    }
    else if ( n->flags & INIT_HAS_A_NEXT ) {
//        printf("depth 1\n");
        return (n->e).ptr;
    }
    else {
//       printf("depth 1\n");
        return NULL;
    }
}

//this function will construct a temp hash table to compress the infomation to print
//
//
struct print_key {
    uint32_t ip;
    uint32_t cidr;
};

struct print_hash_node{
    struct print_key key;
    struct hmap_node node; 
};

int print_hash_check(struct hmap *h, struct print_key *key, uint32_t key_hash)
{
    struct print_hash_node *n;
    HMAP_FOR_EACH_WITH_HASH(n, node, key_hash, h) {
        if (n->key.ip == key->ip && n->key.cidr == key->cidr){
            return 0;
        }
    }
    return 1;

}

void print_ptr(struct print_key *key, void (*print_next_hop)(struct next_hop_info *nhi), struct next_hop_info *nhi)
{
    struct in_addr addr;
    addr.s_addr = htonl(key->ip);
    printf("%s/%d ", inet_ntoa(addr), key->cidr);

    if (print_next_hop)
        print_next_hop(nhi);

    printf("\n");
}

void print_mb_node_iter(struct mb_node_v6 *node, uint32_t ip, uint32_t left_bits, 
        uint32_t cur_cidr, void (*print_next_hop)(struct next_hop_info *nhi)
        )
{
    int bit=0;
    int cidr=0;
    int stride = 0;
    uint32_t iptmp;
    int pos;
    struct next_hop_info **nhi;
    struct mb_node_v6 *next;
    struct print_key key;

    //internal prefix first
    for (cidr=0;cidr<= STRIDE -1;cidr ++ ){
        for (bit=0;bit< (1<<cidr);bit++) {
            pos = count_inl_bitmap(bit,cidr);
            if (test_bitmap(node->internal, pos)) {
                nhi = (struct next_hop_info**)node->child_ptr - count_ones(node->internal, pos) - 1;
                iptmp = ip;
                iptmp |= bit << (left_bits - cidr);
                key.ip = iptmp;
                key.cidr = cur_cidr + cidr; 
                print_ptr(&key, print_next_hop, *nhi); 
            }
        }
    }

    for (stride = 0; stride < (1<<STRIDE); stride ++ ){
        pos = count_enl_bitmap(stride);
        if (test_bitmap(node->external, pos)) {
            //ip |= stride << (left_bits - STRIDE);
            next = (struct mb_node_v6 *)node->child_ptr + count_ones(node->external, pos);
            print_mb_node_iter(next, ip | (stride << (left_bits - STRIDE)), left_bits - STRIDE, cur_cidr + STRIDE, print_next_hop); 
        }
    }
}


void print_mb_node(struct mb_node_v6 *node, uint32_t ip, uint32_t cidr, 
        void (*print_next_hop)(struct next_hop_info *nhi),
        struct hmap *h)
{
    //print internal prefix
    //the first bit of the internal needs to be specially handled
    if (test_bitmap(node->internal, (count_inl_bitmap(0,0)))) {
        struct print_key key;
        uint32_t hash_key;
        key.ip = ip;
        key.cidr = cidr;
        struct next_hop_info **nhi;

        hash_key = hash_words((const uint32_t *)&key, sizeof(key)/sizeof(uint32_t), 0);

        if(print_hash_check(h, &key, hash_key)) {
            nhi = (struct next_hop_info **)node->child_ptr - 
                count_ones(node->internal, count_inl_bitmap(0,0)) - 1;
            print_ptr(&key, print_next_hop, *nhi);

            struct print_hash_node *n = xzalloc(sizeof *n);
            n->key = key;

            hmap_insert(h, &n->node, hash_key); 
        }
    }
    struct mb_node_v6 tmp;
    tmp = *node;

    clear_bitmap(&tmp.internal, count_inl_bitmap(0,0));
    //tmp.internal &= (BITMAP_TYPE)(~(1ULL << (count_inl_bitmap(0,0))));

    print_mb_node_iter(&tmp, ip, LENGTH-INITIAL_BITS, INITIAL_BITS, print_next_hop);
}

void print_all_prefix(struct lookup_trie *trie, void (*print_next_hop)(struct next_hop_info *nhi)) 
{
    print_mb_node_iter(&trie->up_aux, 0, LENGTH, 0, print_next_hop); 
}

void print_valid_prefix(struct lookup_trie *trie, void (*print_next_hop)(struct next_hop_info *nhi))
{
    int i;
    uint32_t ip = 0;
    uint32_t cidr = 0;

    struct hmap print_ht;
    struct print_key key;
    uint32_t hash_key;

    hmap_init(&print_ht);
    if (print_next_hop == NULL) {
        printf("please provide a function to print next_hop_info\n");
    }



    for(i=0;i<(1<<INITIAL_BITS);i++){
        
        if (trie->init[i].flags == 0) {
            continue;
        }

        ip = (i)<< (LENGTH-INITIAL_BITS);
        cidr = (trie->init)[i].flags >> PREFIX_HI;
        ip = ip & (0xFFFFFFFF << (LENGTH-cidr));

        if ( (trie->init)[i].flags & INIT_HAS_A_CHILD){
            print_mb_node(&(trie->init)[i].e.node, ip, cidr, print_next_hop, &print_ht);
        }
        else {
            
            key.ip = ip; 
            key.cidr = cidr;
            hash_key = hash_words((const uint32_t *)&key, sizeof(key)/sizeof(uint32_t), 0);

            if(print_hash_check(&print_ht, &key, hash_key)) {
                print_ptr(&key, print_next_hop, (trie->init)[i].e.ptr);

                struct print_hash_node *n = xzalloc(sizeof *n);
                n->key = key;

                hmap_insert(&print_ht, &n->node, hash_key); 
            }
        }
    }


//destory hash table
    struct print_hash_node *n,*next;
    HMAP_FOR_EACH_SAFE(n,next, node, &print_ht){
        free(n);
    }
    hmap_destroy(&print_ht);
}


#ifdef DEBUG_MEMORY_FREE
int mem_destroy;
#endif



#ifndef USE_MM
static void destroy_subtrie_first_level(struct mb_node_v6 *node, 
        uint32_t ip,
        uint32_t cidr,
        void(*destroy_nhi)(struct next_hop_info *nhi),
        struct hmap *nhi_ht)
#else
static void destroy_subtrie_first_level(struct mb_node_v6 *node, 
        uint32_t ip,
        uint32_t cidr,
        void(*destroy_nhi)(struct next_hop_info *nhi),
        struct hmap *nhi_ht,
        struct mc *m)
#endif

{
    if (test_bitmap(node->internal, (count_inl_bitmap(0,0)))) {
        struct print_key key;
        key.ip = ip;
        key.cidr = cidr;
        struct next_hop_info **nhi;
        uint32_t key_hash;
    
        nhi = (struct next_hop_info **)node->child_ptr - 
            count_ones(node->internal, count_inl_bitmap(0,0)) -1;
        key_hash = hash_words((const uint32_t *)&key, sizeof(key)/sizeof(uint32_t), 0);

        if (print_hash_check(nhi_ht, &key, key_hash)) {

            if (destroy_nhi) {
                destroy_nhi(*nhi);
            }

            struct print_hash_node *n = xzalloc(sizeof *n);
            n->key = key;
            hmap_insert(nhi_ht, &n->node, key_hash);
        }
        *nhi=NULL;
    }
#ifndef USE_MM
    destroy_subtrie(node, destroy_nhi);
#else
    destroy_subtrie(node, destroy_nhi, m, 0, 1);
#endif

}

void destroy_trie(struct lookup_trie *trie, void (*destroy_nhi)(struct next_hop_info* nhi))
{
    int i;
    struct hmap nhi_ht;
    uint32_t hash_key;
    struct print_key key;
    uint32_t ip;
    uint32_t cidr;

    if(!destroy_nhi) {
        printf("please provide a destroy func for next hop info\n");
    }

    hmap_init(&nhi_ht);

    for(i=0;i<(1<<INITIAL_BITS);i++) {
        if ( trie->init[i].flags == 0) {
            continue;
        }

        ip = (i)<< (LENGTH-INITIAL_BITS);
        cidr = (trie->init)[i].flags >> PREFIX_HI;
        ip = ip & (0xFFFFFFFF << (LENGTH-cidr));


        if ( trie->init[i].flags & INIT_HAS_A_CHILD) {
#ifndef USE_MM
            destroy_subtrie_first_level(&trie->init[i].e.node, ip, cidr, destroy_nhi, &nhi_ht);
#else
            destroy_subtrie_first_level(&trie->init[i].e.node, ip, cidr, destroy_nhi, &nhi_ht, &trie->mm);
#endif
        }
        else {
            if (destroy_nhi) { 

                key.ip = ip;
                key.cidr = cidr;
                hash_key = hash_words((const uint32_t *)&key, sizeof(key)/sizeof(uint32_t), 0);

                if(print_hash_check(&nhi_ht, &key, hash_key)){                
                    destroy_nhi(trie->init[i].e.ptr);

                    struct print_hash_node *n = xzalloc(sizeof *n);
                    n->key = key;
                    hmap_insert(&nhi_ht, &n->node, hash_key); 
                }
            }
            trie->init[i].flags = 0;
            trie->init[i].e.ptr = NULL;
        }
    }

    free(trie->init);
    trie->init = NULL;
#ifndef USE_MM
    destroy_subtrie(&trie->up_aux, NULL);
#else
    destroy_subtrie(&trie->up_aux, NULL, &trie->mm, 0, 0);
#endif


//destroy hash table
    struct print_hash_node *n,*next;
    HMAP_FOR_EACH_SAFE(n,next, node, &nhi_ht){
        free(n);
    }
    hmap_destroy(&nhi_ht);

#ifdef DEBUG_MEMORY_FREE
    //mem_destroy += (1<<INITIAL_BITS) * sizeof(*(trie->init));
    printf("mem destroy %d %dK %dM\n", mem_destroy, mem_destroy/(1024), mem_destroy/(1024*1024));
    mem_destroy = 0;
#endif
}




void level_sub_trie(struct mb_node_v6 * n, uint32_t *level_mem, uint32_t l)
{
    int stride;
    int pos;
    struct mb_node_v6 *next;
    int child_num = count_children(n->external);
    int rule_num = count_children(n->internal);
    

    level_mem[l] += (UP_RULE(rule_num) + UP_CHILD(child_num)) * NODE_SIZE;
    
    for (stride = 0; stride < (1<<STRIDE); stride ++ ){
        pos = count_enl_bitmap(stride);
        if (test_bitmap(n->external, pos)) {
            next = (struct mb_node_v6 *)n->child_ptr + count_ones(n->external, pos);
            level_sub_trie(next, level_mem, l+1);
        }
    }

    
}

void level_memory(struct lookup_trie *trie)
{
    uint32_t level_mem[LEVEL];
    memset(level_mem, 0, LEVEL * sizeof(uint32_t));

    level_mem[0] = (1<<INITIAL_BITS) * sizeof(struct init_node_v6);
    //printf("first level is initial array %d\n", level_mem[0]); 

    int i;
    for(i=0;i<(1<<INITIAL_BITS);i++){
        if (trie->init[i].flags == 0) {
            continue;
        }

        if (trie->init[i].flags & INIT_HAS_A_CHILD) {
            level_sub_trie(&trie->init[i].e.node, level_mem, 1);
        }

    }

    uint32_t total = 0;
    for(i=0;i<LEVEL;i++) {
        printf("level %d size %d %dK %fM\n", i, level_mem[i], 
                level_mem[i]/1024, level_mem[i]/(1024*1024.0));
        total += level_mem[i];
    }

    printf("total memory %.2fM\n", total/(1024*1024.0));
}






