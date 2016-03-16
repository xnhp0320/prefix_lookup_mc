#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "tbm.h"
#include "tbmv6.h"
#include "lib/bitmap.h"
#include <arpa/inet.h>


int tbmv6_init_trie(struct tbmv6_trie *trie)
{
    if (!trie) {
        printf("Wrong argument\n");
        return -1;
    }

    trie->init = (struct init_node*)calloc(1,(1 << INITIAL_BITS_v6) * sizeof(struct init_node));

    trie->up_aux.external= 0;
    trie->up_aux.internal= 0;
    trie->up_aux.child_ptr=NULL;

    if (!trie->init) {
        perror("no memory:");
        exit(-1);
    }

    mm_init(&trie->m, MEM_ALLOC_SIMPLE);
    mm_init(&trie->up_m, MEM_ALLOC_SIMPLE);

    return 0;
}


//SAME: assume the init table is completed
//ip = 192.168.1.0
//cidr = 24
//the unmask part of ip need to be zero
void tbmv6_insert_prefix(struct tbmv6_trie *trie, struct ip_v6 ip, int cidr, void *nhi)
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
                    bitmapv6_insert_prefix(&((trie->init)[index + i].e.node), &trie->m, ipzero, 0, nhi);
#ifdef UP_STATS
#endif
                }
            }
            else{
                if (( (trie->init)[index + i].flags >> PREFIX_HI ) > cidr) {
                    continue;
                }
                else {
                    (trie->init)[index + i].flags = (cidr << PREFIX_HI);
                    (trie->init)[index + i].e.ptr = (void*)nhi;
#ifdef UP_STATS
#endif
                }
            }

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
            bitmapv6_insert_prefix(&((trie->init)[index].e.node), &trie->m, iptmp, 
                    cidr - INITIAL_BITS_v6, nhi);

#ifdef UP_STATS
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

                void *info  = (trie->init)[index].e.ptr;
                (trie->init)[index].e.ptr = NULL;
                bitmapv6_insert_prefix(&((trie->init)[index].e.node), &trie->m, ipzero, 0, info);
            }

            (trie->init)[index].flags |= INIT_HAS_A_CHILD;
            iptmp = ip;
            lshift_ipv6(&iptmp, INITIAL_BITS_v6);
            bitmapv6_insert_prefix(&((trie->init)[index].e.node), 
                    &trie->m, iptmp, cidr - INITIAL_BITS_v6,
                    nhi);

#ifdef UP_STATS
#endif
        }

    }

    // build an extra trie to help the update 
    bitmapv6_insert_prefix(&trie->up_aux, &trie->up_m, ip, cidr, nhi);

}

struct overlap_nhi_data{
    void (*func)(void* nhi);
    void *nhi_near;
};

static int overlap_nhi(struct mb_node *node, uint8_t stride, uint8_t pos, 
        uint8_t type, void *user_data)
{
    if (type == LEAF_NODE) {
        void ** nhi;
        struct overlap_nhi_data *ond  = (struct overlap_nhi_data *)(user_data);

        nhi = (void **)node->child_ptr - 
            count_ones(node->internal, pos) - 1; 

        if (ond->func)
            ond->func(*nhi);
        *nhi = ond->nhi_near;
    }
    return TRAVERSE_CONT;
}


//return 1 means the prefix exists
int tbmv6_prefix_exist(struct tbmv6_trie *trie, struct ip_v6 ip, uint8_t cidr)
{
    return bitmapv6_prefix_exist(&trie->up_aux, ip, cidr);
}

void tbmv6_delete_prefix(struct tbmv6_trie *trie, struct ip_v6 ip, int cidr, 
        void (*destroy_nhi)(void *nhi))
{
    struct ip_v6 iptmp = ip;
    struct ip_v6 ipzero = {0,0};
    rshift_ipv6(&iptmp, LENGTH_v6 - INITIAL_BITS_v6);
    uint32_t index = (uint32_t)iptmp.iplo;

    uint32_t i;
    int ret;
    uint8_t prefix_near; 
    uint8_t prefix;
    void *nhi_near = NULL;

    if (cidr == 0){
        printf("can't delete: cidr == 0\n");
        return;
    }

    if (cidr <= INITIAL_BITS_v6) {

//13-bits is like leaf pushing a trie into the 13-bits
//if a prefix with length of 8 is deleted
//we should also see if there is /7,/6,...share the same prefix with this
// /8 prefix. if there is, we need push this prefix to the /13 bits

        prefix_near = bitmapv6_detect_overlap_generic(&trie->up_aux, ip, cidr, INITIAL_BITS_v6, &nhi_near);
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
                        ret = bitmapv6_delete_prefix(
                                &((trie->init)[index + i].e.node), 
                                &trie->m,
                                ipzero, 0, destroy_nhi);
                        
                        if (!ret) {
                            (trie->init)[index + i].flags = 0 | INIT_HAS_A_CHILD;
                        }
                        else {
                            (trie->init)[index + i].flags = 0;
                            (trie->init)[index + i].e.ptr = NULL;
                        }
#ifdef UP_STATS
#endif
                    }
                    else {
                        //printf("insert prefix_near %d\n", prefix_near);

                        (trie->init)[index + i].flags = (prefix_near << PREFIX_HI) | INIT_HAS_A_CHILD;
                        ond.func = destroy_nhi;
                        bitmapv6_traverse_branch(&((trie->init)[index + i].e.node), ipzero, 0, overlap_nhi, &ond);
#ifdef UP_STATS
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
                        (trie->init)[index + i].flags = (prefix_near << PREFIX_HI); 
                        (trie->init)[index + i].e.ptr = nhi_near;
                    }
#ifdef UP_STATS
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
            ret = bitmapv6_delete_prefix(&((trie->init)[index].e.node), 
                    &trie->m, 
                    iptmp, 
                    cidr - INITIAL_BITS_v6, destroy_nhi);
            if (ret){
                prefix_near = bitmapv6_detect_overlap_generic(&trie->up_aux, 
                        ip, cidr, INITIAL_BITS_v6, &nhi_near);
                if( !prefix_near) {
                    trie->init[index].flags = 0;
                    trie->init[index].e.ptr = NULL;
                }
                else {
                    trie->init[index].flags = (prefix_near << PREFIX_HI); 
                    trie->init[index].e.ptr = nhi_near;
                }

#ifdef UP_STATS
#endif
            }
        }
    }
    //printf("ip %x\n, cidr %d\n", ip, cidr);
    bitmapv6_delete_prefix(&trie->up_aux, &trie->up_m, ip, cidr, NULL);
}


void * tbmv6_search(struct tbmv6_trie *trie, struct ip_v6 ip)
{
    rshift_ipv6(&ip, LENGTH_v6 - INITIAL_BITS_v6);
    struct init_node *n = &((trie->init)[ip.iplo]);
    lshift_ipv6(&ip, INITIAL_BITS_v6);

    //printf("1 %p\n",n);

    if (likely(n->flags & INIT_HAS_A_CHILD)) {
        return bitmapv6_do_search_lazy(&(n->e.node), ip);
    }
    else {
//        printf("depth 1\n");
        return (n->e).ptr;
    }
}

void tbmv6_print_all_prefix(struct tbmv6_trie *trie, 
        void (*print_next_hop)(void *nhi)) 
{
    bitmapv6_print_all_prefix(&trie->up_aux, print_next_hop);  
}


void tbmv6_destroy_trie(struct tbmv6_trie *trie, void (*destroy_nhi)(void* nhi))
{
    int i;

    if(!destroy_nhi) {
        printf("please provide a destroy func for next hop info\n");
    }

    destroy_subtrie(&trie->up_aux, &trie->up_m, destroy_nhi, 0);


    for(i=0;i<(1<<INITIAL_BITS_v6);i++) {
        if ( trie->init[i].flags == 0) {
            continue;
        }

        destroy_subtrie(&trie->init[i].e.node, 
                &trie->m, NULL, 0);
        
    }

    free(trie->init);
    trie->init = NULL;
}
