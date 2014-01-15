#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "bitmap_v6.h"
#include "mb_node.h"
#include "fast_lookup.h"
#include "hash.h"
#include "hmap.h"
#include <arpa/inet.h>


int init_lookup_trie_v6(struct lookup_trie_v6 *trie)
{
    if (!trie) {
        printf("Wrong argument\n");
        return -1;
    }
    trie->init = (struct init_node_v6*)calloc(1,(1 << INITIAL_BITS_v6) * sizeof(struct init_node_v6));
    fast_table_init();

    trie->up_aux.external= 0;
    trie->up_aux.internal= 0;
    trie->up_aux.child_ptr=NULL;

    if (!trie->init) {
        perror("no memory:");
        exit(-1);
    }

#ifdef USE_MM
    mc_init(&trie->mm);
#else
    memset(&trie->mm, 0, sizeof(struct mc));
#endif

    return 0;
}


//1~127
static INLINE void lshift_ipv6(struct ip_v6 *ip, uint8_t bits)
{
    uint64_t head;
    if (bits < 64) {
        head = ip->iplo >> (64 - bits);
        ip->iphi <<= bits;
        ip->iplo <<= bits;
        ip->iphi |= head;
    }
    else if (bits > 64) {
        head = ip->iplo << (bits - 64);
        ip->iphi = head;
        ip->iplo = 0;

    }
    else {
        ip->iphi = ip->iplo;
        ip->iplo = 0;
    }

}

static INLINE void rshift_ipv6(struct ip_v6 *ip, uint8_t bits)
{
    uint64_t tail;
    if (bits < 64) {
        tail = ip->iphi << (64 -bits);
        ip->iphi >>= bits;
        ip->iplo >>= bits;
        ip->iplo |= tail;
    }
    else if (bits > 64) {
        tail = ip->iphi >> (bits - 64);
        ip->iplo = tail;
        ip->iphi = 0;
    }
    else {
        ip->iplo = ip->iphi;
        ip->iphi = 0;
    }

}


//notes: cidr > 0 
//if cidr == 0, then the ip must be 0
//or something terrible will happan
//this is due to a strange implementation
//that ip32<<32 == ip32 ip64 << 64 == ip64
static void insert_entry(
        struct lookup_trie_v6 *trie,
        struct mb_node_v6 *node,
        struct ip_v6 ip, int cidr, 
        struct next_hop_info *nhi,
        int use_mm
        )
{
    uint8_t pos;
    uint8_t stride;
    uint8_t level=0;
    struct ip_v6 iptmp;
    struct next_hop_info **i;

    for (;;) {
        iptmp = ip;
        if (cidr < STRIDE) {
            // if the node has the prefix already
            //need to be atomic
            //
            rshift_ipv6(&iptmp, LENGTH_v6 - cidr);
            stride = iptmp.iplo;
            pos = count_inl_bitmap(stride, cidr);
            if(test_bitmap(node->internal, pos)) {
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
            rshift_ipv6(&iptmp, LENGTH_v6 - STRIDE);
            stride = iptmp.iplo;
            pos = count_enl_bitmap(stride);

            if (test_bitmap(node->external, pos)){
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
            lshift_ipv6(&ip, STRIDE);
            level ++;
            //ip <<= STRIDE;
        }
    }
}


//SAME: assume the init table is completed
//ip = 192.168.1.0
//cidr = 24
//the unmask part of ip need to be zero
void insert_prefix_v6(struct lookup_trie_v6 *trie, struct ip_v6 ip, int cidr, struct next_hop_info *nhi)
{
    struct ip_v6 iptmp = ip;
    struct ip_v6 ipzero = {0,0};
    rshift_ipv6(&iptmp, LENGTH_v6 - INITIAL_BITS_v6);
    uint32_t index = iptmp.iplo;
    uint32_t i;

    if (cidr == 0) {
        printf("cidr == 0 is a default rules\n");
        return ;
    }


    if (cidr <= INITIAL_BITS_v6) {
        for (i=0; i<(1<<(INITIAL_BITS_v6 - cidr)); i++){
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
                    insert_entry(trie, &((trie->init)[index + i].e.node), ipzero, 0, nhi, 1);
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
            //    //if one init entry has a child, its prefix == INITIAL_BITS_v6 + 1
            //    //so cidr <= INITAIL_BITS will not taken the entry
            //    if (flags_prefix == INITIAL_BITS_v6 + 1 ) {
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
        //cidr > INITIAL_BITS_v6
        //which means the inserted prefix will follow the init entry to add a branch
        //if the entry has a child, it will "update" the entry, to change the ptr node into the trie node


        //if the entry is a empty one, the code will "insert" a new path.
        if ( (trie->init)[index].flags & INIT_HAS_A_CHILD) {
            // if the node is a ptr node, then we have to change the node to a trie node;
            // and add the entry
            iptmp = ip;
            lshift_ipv6(&iptmp, INITIAL_BITS_v6);
            insert_entry(trie, &((trie->init)[index].e.node), iptmp, 
                    cidr - INITIAL_BITS_v6, nhi, 1);

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
            //if it's a ptr node, its prefix_hi > 0 and <= INITIAL_BITS_v6
            //then we first insert the ptr and then insert the nexthop
            //set the child bit

            if ( ((trie->init)[index].flags >> PREFIX_HI) > 0 ) {
                // if it's a ptr node

                struct next_hop_info *info  = (trie->init)[index].e.ptr;
                (trie->init)[index].e.ptr = NULL;
                (trie->init)[index].flags &= (~INIT_HAS_A_NEXT);
                insert_entry(trie, &((trie->init)[index].e.node), ipzero, 0, info, 1);
            }

            (trie->init)[index].flags |= INIT_HAS_A_CHILD;
            iptmp = ip;
            lshift_ipv6(&iptmp, INITIAL_BITS_v6);
            insert_entry(trie, &((trie->init)[index].e.node), iptmp, cidr - INITIAL_BITS_v6,
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

struct trace_v6{
    struct mb_node_v6 *node;
    uint32_t pos;
};



static int update_nodes(struct mc *mm, struct trace_v6 *t, int total, int use_mm)
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

typedef void (*traverse_func_v6) (struct mb_node_v6 *node, uint8_t stride, int cidr, void *data);


static void traverse_trie(struct lookup_trie_v6 *trie, struct mb_node_v6 *node, struct ip_v6 ip, int cidr,
        traverse_func_v6 func, void *user_data)
{

    uint8_t pos;
    uint8_t stride;
    struct ip_v6 iptmp;

    for (;;) {

        iptmp = ip;
        if (cidr < STRIDE) {
            rshift_ipv6(&iptmp, LENGTH_v6 - cidr);
            stride = iptmp.iplo;
            pos = count_inl_bitmap(stride, cidr);

            func(node, stride, cidr, user_data);

            break;
        }
        //push the "cidr == stride" node into the next child
        else {
            rshift_ipv6(&iptmp, LENGTH_v6 - STRIDE);
            stride = iptmp.iplo;
            pos = count_enl_bitmap(stride);

            func(node, stride, cidr, user_data);
            node = (struct mb_node_v6*) node->child_ptr + count_ones(node->external, pos); 

            cidr -= STRIDE;
            //ip <<= STRIDE;
            lshift_ipv6(&ip, STRIDE);
        }
    }
}

typedef void (*destroy_func)(struct next_hop_info*);

struct overlap_nhi_data{
    destroy_func func;
    struct next_hop_info *nhi_near;
};

static void overlap_nhi(struct mb_node_v6 *node, uint8_t stride, int cidr, void *user_data)
{
    if (cidr < STRIDE) {
        uint8_t pos;
        struct next_hop_info ** nhi;
        struct overlap_nhi_data *ond  = (struct overlap_nhi_data *)(user_data);

        pos = count_inl_bitmap(stride, cidr);

        nhi = (struct next_hop_info **)node->child_ptr - 
            count_ones(node->internal, pos) - 1; 

        if (ond->func)
            ond->func(*nhi);
        *nhi = ond->nhi_near;
    }
}




static int delete_entry(struct lookup_trie_v6 *trie, struct mb_node_v6 *node, struct ip_v6 ip, int cidr,
        void (*destroy_nhi)(struct next_hop_info *nhi), int use_mm)
{
    uint8_t pos;
    uint8_t stride;
    struct ip_v6 iptmp;
    struct trace_v6 trace_node[UPDATE_LEVEL_v6];
    int i = 0;

    for (;;) {

        iptmp = ip;
        if (cidr < STRIDE) {
            // if the node has the prefix already
            //need to be atomic
            //
            rshift_ipv6(&iptmp, LENGTH_v6 - cidr);
            stride = iptmp.iplo;
            pos = count_inl_bitmap(stride, cidr);

            if (destroy_nhi) {
                struct next_hop_info **nhi;
                nhi = (struct next_hop_info **)node->child_ptr - count_ones(node->internal,
                        pos) - 1;
                destroy_nhi(*nhi);
            }

            trace_node[i].node = node;
            trace_node[i].pos =  pos; 

            break;
        }
        //push the "cidr == stride" node into the next child
        else {
            rshift_ipv6(&iptmp, LENGTH_v6 - STRIDE);
            stride = iptmp.iplo;
            pos = count_enl_bitmap(stride);

            
            trace_node[i].node = node;
            trace_node[i].pos  = pos; 

            node = (struct mb_node_v6*) node->child_ptr + count_ones(node->external, pos); 

            cidr -= STRIDE;
            //ip <<= STRIDE;
            lshift_ipv6(&ip, STRIDE);
        }
        i++;
    }
    return update_nodes(&trie->mm, trace_node, i, use_mm);
}


static uint8_t detect_overlap(struct lookup_trie_v6 *trie, struct ip_v6 ip, uint8_t cidr, uint32_t leaf_pushing_bits, struct next_hop_info **nhi_over);

//return 1 means the prefix exists
int prefix_exist_v6(struct lookup_trie_v6 *trie, struct ip_v6 ip, uint8_t cidr)
{
    uint8_t pos;
    uint8_t stride;
    struct mb_node_v6 *node = &trie->up_aux;
    struct ip_v6 iptmp;

    for (;;) {
        iptmp = ip;

        if (cidr < STRIDE) {
            // if the node has the prefix already
            //need to be atomic
            //
            rshift_ipv6(&iptmp, LENGTH_v6 - cidr);
            stride = iptmp.iplo;
            pos = count_inl_bitmap(stride, cidr);
            if ( !(test_bitmap(node->internal, pos))) {
                //printf("ERROR: try to delete a non-exist prefix\n");
                return 0;
            }

            break;
        }
        //push the "cidr == stride" node into the next child
        else {
            rshift_ipv6(&iptmp, LENGTH_v6 - STRIDE);
            stride = iptmp.iplo;
            pos = count_enl_bitmap(stride);

            if( !(test_bitmap(node->external, pos))) {
                //printf("ERROR: try to delete a non-exist prefix\n");
                return 0;
            }

            node = (struct mb_node_v6*) node->child_ptr + count_ones(node->external, pos); 

            cidr -= STRIDE;
            //ip <<= STRIDE;
            lshift_ipv6(&ip, STRIDE);
        }
    }

    return 1;
}

void delete_prefix_v6(struct lookup_trie_v6 *trie, struct ip_v6 ip, int cidr, 
        void (*destroy_nhi)(struct next_hop_info *nhi))
{
    struct ip_v6 iptmp = ip;
    struct ip_v6 ipzero = {0,0};
    rshift_ipv6(&iptmp, LENGTH_v6 - INITIAL_BITS_v6);
    uint32_t index = (uint32_t)iptmp.iplo;

    uint32_t i;
    int ret;
    uint8_t prefix_near; 
    uint8_t prefix;
    struct next_hop_info *nhi_near = NULL;

    if (cidr == 0){
        printf("can't delete: cidr == 0\n");
        return;
    }

    if (cidr <= INITIAL_BITS_v6) {

//13-bits is like leaf pushing a trie into the 13-bits
//if a prefix with length of 8 is deleted
//we should also see if there is /7,/6,...share the same prefix with this
// /8 prefix. if there is, we need push this prefix to the /13 bits

        prefix_near = detect_overlap(trie, ip, cidr,INITIAL_BITS_v6, &nhi_near);
        struct overlap_nhi_data ond;
        ond.nhi_near = nhi_near;

        for (i=0; i<(1<<(INITIAL_BITS_v6 - cidr)); i++){
            //if it has a child
            prefix = (trie->init)[index + i].flags >> PREFIX_HI;
            if ( (trie->init)[index + i].flags & INIT_HAS_A_CHILD) {
                if ( prefix > cidr){
                    continue;
                }
                else {
                    if (prefix_near == 0){
                        ret = delete_entry(trie, &((trie->init)[index + i].e.node), ipzero, 0, destroy_nhi, 1);
                        
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
                        //insert_entry(trie, &((trie->init)[index + i].e.node), ipzero, 0, nhi_near); 
                        ond.func = destroy_nhi;
                        traverse_trie(trie, &((trie->init)[index + i].e.node), ipzero, 0, overlap_nhi, &ond);
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
                        destroy_nhi((trie->init)[index + i ].e.ptr);
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

            if (destroy_nhi)
                //we only need to destory once!!!!
                destroy_nhi = NULL;

        }
    }
    else {
        if ( (trie->init)[index].flags & INIT_HAS_A_CHILD) {
            // if the node is a ptr node, then we have to change the node to a trie node;
            // and add the entry
            iptmp = ip;
            lshift_ipv6(&iptmp, INITIAL_BITS_v6);
            ret = delete_entry(trie, &((trie->init)[index].e.node), iptmp, 
                    cidr - INITIAL_BITS_v6, destroy_nhi, 1);
            if (ret){
                prefix_near = detect_overlap(trie, ip, cidr,INITIAL_BITS_v6, &nhi_near);
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

//INLINE int tree_function(uint16_t bitmap, uint8_t stride)


   
static struct next_hop_info * do_search(struct mb_node_v6 *n, struct ip_v6 *ip)
{
    uint8_t stride;
    int pos;
    struct ip_v6 iptmp;
    struct next_hop_info **longest = NULL;
    //int depth = 1;

    for (;;){
        iptmp = *ip;
        rshift_ipv6(&iptmp, LENGTH_v6 - STRIDE);
        stride = iptmp.iplo;
        pos = tree_function(n->internal, stride);

        if (pos != -1){
            longest = (struct next_hop_info**)n->child_ptr - count_ones(n->internal, pos) - 1;
        }
        if (test_bitmap(n->external, count_enl_bitmap(stride))) {
            //printf("%d %p\n", depth, n);
            n = (struct mb_node_v6*)n->child_ptr + count_ones(n->external, count_enl_bitmap(stride));
            lshift_ipv6(ip, STRIDE);
            //ip = (uint32_t)(ip << STRIDE);
            //depth ++;
        }
        else {
            break;
        }
    }
//    printf("depth %d\n",depth);
    return (longest == NULL)?NULL:*longest;
}



static uint8_t detect_overlap(struct lookup_trie_v6 *trie, struct ip_v6 ip, uint8_t cidr, uint32_t leaf_pushing_bits, struct next_hop_info **nhi_over)
{
    //uint8_t ret;
    uint8_t stride;
    uint8_t mask = 0;
    uint8_t final_mask = 0;
    uint8_t curr_mask = 0;

    uint8_t step = 0;
    struct ip_v6 iptmp;
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
        iptmp = ip;
        rshift_ipv6(&iptmp, LENGTH_v6 - STRIDE);
        stride = iptmp.iplo;
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
            lshift_ipv6(&ip, STRIDE);
            //ip = (uint32_t)(ip << STRIDE);
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

struct lazy_travel_v6 {
    struct mb_node_v6 *lazy_p;
    uint32_t stride;
};

static struct lazy_travel_v6 lazy_mark[LEVEL_v6]; 

//for accelerating, using pointer
static INLINE struct next_hop_info * do_search_lazy(struct mb_node_v6 *n, struct ip_v6 *ip)
{
    uint8_t stride;
    int pos;
    struct next_hop_info **longest = NULL;
    struct ip_v6 iptmp;
    int travel_depth = -1;
//    int depth = 1;

    for (;;){
        iptmp = *ip;
        rshift_ipv6(&iptmp, LENGTH_v6 - STRIDE);
        stride = iptmp.iplo;

        if (test_bitmap(n->external, count_enl_bitmap(stride))) {
            travel_depth++;
            lazy_mark[travel_depth].lazy_p = n;
            lazy_mark[travel_depth].stride = stride;
            n = (struct mb_node_v6*)n->child_ptr + count_ones(n->external, count_enl_bitmap(stride));
            //__builtin_prefetch(n);
            //ip = (uint32_t)(ip << STRIDE);
            lshift_ipv6(ip, STRIDE);
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




struct next_hop_info * search_v6(struct lookup_trie_v6 *trie, struct ip_v6 *ip)
{
    struct ip_v6 iptmp;
    iptmp = *ip;
    rshift_ipv6(&iptmp, LENGTH_v6 - INITIAL_BITS_v6);
    struct init_node_v6 *n = &((trie->init)[iptmp.iplo]);
    iptmp = *ip;
    lshift_ipv6(&iptmp, INITIAL_BITS_v6);

    //printf("1 %p\n",n);

    if ( n->flags & INIT_HAS_A_CHILD ) {
#ifndef USE_LAZY
        return do_search(&(n->e.node),&iptmp);
#else
        return do_search_lazy(&(n->e.node),&iptmp);
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
    struct ip_v6 ip;
    uint32_t cidr;
};

struct print_hash_node{
    struct print_key key;
    struct hmap_node node; 
};

static int print_hash_check(struct hmap *h, struct print_key *key, uint32_t key_hash)
{
    struct print_hash_node *n;
    HMAP_FOR_EACH_WITH_HASH(n, node, key_hash, h) {
        if (n->key.ip.iplo == key->ip.iplo 
                && n->key.ip.iphi == key->ip.iphi 
                && n->key.cidr == key->cidr){
            return 0;
        }
    }
    return 1;

}

static inline void swap(unsigned char *ip, int i, int j)
{
    unsigned char tmp;
    tmp = ip[i];
    ip[i] = ip[j];
    ip[j] = tmp;
}

void hton_ipv6(struct in6_addr *ip)
{
    int i = 0;
#if BYTE_ORDER == __LITTLE_ENDIAN
    for(;i<LENGTH_v6/16;i++) 
        swap((unsigned char *)ip,i,LENGTH_v6/8 - i - 1);
#endif

}

static void print_ptr(struct print_key *key, void (*print_next_hop)(struct next_hop_info *nhi), struct next_hop_info *nhi)
{
    struct in6_addr addr;
    char str[INET6_ADDRSTRLEN];

    memcpy(&addr, &key->ip, sizeof(addr));
    hton_ipv6(&addr);

    inet_ntop(AF_INET6, (const void *)&addr, str, INET6_ADDRSTRLEN);

    printf("%s/%d ",str , key->cidr);

    if (print_next_hop)
        print_next_hop(nhi);

    printf("\n");
}

static void print_mb_node_iter(struct mb_node_v6 *node, struct ip_v6 ip, uint32_t left_bits, 
        uint32_t cur_cidr, void (*print_next_hop)(struct next_hop_info *nhi)
        )
{
    int bit=0;
    int cidr=0;
    int stride = 0;
    int pos;
    struct next_hop_info **nhi;
    struct mb_node_v6 *next;
    struct print_key key;

    struct ip_v6 stride_bits = {0,0};
    struct ip_v6 iptmp;

    //internal prefix first
    for (cidr=0;cidr<= STRIDE -1;cidr ++ ){
        for (bit=0; bit< (1<<cidr); bit++) {
            pos = count_inl_bitmap(bit,cidr);
            if (test_bitmap(node->internal, pos)) {
                nhi = (struct next_hop_info**)node->child_ptr - count_ones(node->internal, pos) - 1;

                //here the ugly code
                stride_bits.iplo = bit;
                stride_bits.iphi = 0;
                iptmp = ip;
                lshift_ipv6(&stride_bits, left_bits - cidr);
                iptmp.iphi |= stride_bits.iphi;
                iptmp.iplo |= stride_bits.iplo;
                //end
                
                key.ip = iptmp;
                key.cidr = cur_cidr + cidr; 
                print_ptr(&key, print_next_hop, *nhi); 
            }
        }
    }

    memset(&stride_bits, 0, sizeof(stride_bits));

    for (stride = 0; stride < (1<<STRIDE); stride ++ ){
        pos = count_enl_bitmap(stride);
        if (test_bitmap(node->external, pos)) {

            //here the ugly code
            stride_bits.iplo = stride;
            stride_bits.iphi = 0;
            iptmp = ip;
            lshift_ipv6(&stride_bits, left_bits - STRIDE);
            iptmp.iphi |= stride_bits.iphi;
            iptmp.iplo |= stride_bits.iplo;
            //end
            
            next = (struct mb_node_v6 *)node->child_ptr + count_ones(node->external, pos);
            print_mb_node_iter(next, iptmp, left_bits - STRIDE, cur_cidr + STRIDE, print_next_hop); 
        }
    }
}


static void print_mb_node(struct mb_node_v6 *node, struct ip_v6 ip, uint32_t cidr, 
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
    //tmp.internal &= (~( 1ULL << (count_inl_bitmap(0,0))));

    print_mb_node_iter(&tmp, ip, LENGTH_v6-INITIAL_BITS_v6, INITIAL_BITS_v6, print_next_hop);
}

void print_prefix_v6(struct lookup_trie_v6 *trie, void (*print_next_hop)(struct next_hop_info *nhi))
{
    int i;
    struct ip_v6 ip = {0, 0};

    uint64_t iphi = 0;
    uint32_t cidr = 0;

    struct hmap print_ht;
    struct print_key key;
    uint32_t hash_key;

    hmap_init(&print_ht);
    if (print_next_hop == NULL) {
        printf("please provide a function to print next_hop_info\n");
    }



    for(i=0;i<(1<<INITIAL_BITS_v6);i++){
        
        if (trie->init[i].flags == 0) {
            continue;
        }


//it's safe just to keep it inside 64 bit.
//however makes this code ugly
        iphi = ((uint64_t)i)<< (64-INITIAL_BITS_v6);
        cidr = (trie->init)[i].flags >> PREFIX_HI;
        iphi = iphi & (0xFFFFFFFFFFFFFFFFULL << (64-cidr));
        ip.iphi = iphi;

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



#ifndef USE_MM
static void destroy_subtrie_first_level(struct mb_node_v6 *node, 
        struct ip_v6 ip,
        uint32_t cidr,
        void(*destroy_nhi)(struct next_hop_info *nhi),
        struct hmap *nhi_ht)
#else
static void destroy_subtrie_first_level(struct mb_node_v6 *node, 
        struct ip_v6 ip,
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


void destroy_trie_v6(struct lookup_trie_v6 *trie, void (*destroy_nhi)(struct next_hop_info* nhi))
{
    int i;
    struct ip_v6 ip = {0, 0};
    uint64_t iphi = 0;
    uint32_t cidr = 0;

    struct hmap nhi_ht;
    uint32_t hash_key;
    struct print_key key;


    if(!destroy_nhi) {
        printf("please provide a destroy func for next hop info\n");
    }

    hmap_init(&nhi_ht);

    for(i=0;i<(1<<INITIAL_BITS_v6);i++) {
        if ( trie->init[i].flags == 0) {
            continue;
        }

        iphi = ((uint64_t)i)<< (64-INITIAL_BITS_v6);
        cidr = (trie->init)[i].flags >> PREFIX_HI;
        iphi = iphi & (0xFFFFFFFFFFFFFFFFULL << (64-cidr));
        ip.iphi = iphi;

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
            trie->init[i].e.ptr = NULL;
            trie->init[i].flags = 0;
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
    //mem_destroy += (1<<INITIAL_BITS_v6) * sizeof(*(trie->init));
    printf("mem destroy %d\n", mem_destroy);
    mem_destroy = 0;
#endif



}

struct mem_stats_v6 mem_trie_v6(struct lookup_trie_v6 *trie)
{
    struct mem_stats_v6 ms = {0,0};
    struct mem_stats_v6 ret = {0,0};
    int i;


    ret.mem += (1<<INITIAL_BITS_v6) * sizeof(struct init_node_v6);
    printf("the initial array is %d bytes, %d KB\n", ret.mem, ret.mem/1024);


    for(i=0;i<(1<<INITIAL_BITS_v6);i++) {
        if ( trie->init[i].flags == 0) {
            //printf("%d\n",0);
            continue;
        }


        if ( trie->init[i].flags & INIT_HAS_A_CHILD) {
            mem_subtrie(&trie->init[i].e.node, &ms);
            ret.mem += ms.mem;
            ret.node += ms.node;
            //printf("ms.mem %d\n", ms.mem);
            //printf("%d\n", ms.node);
            ms.mem = 0;
            ms.node = 0;
        }
        else {
            //printf("%d\n", 0);
        }

    }
    return ret;
}


