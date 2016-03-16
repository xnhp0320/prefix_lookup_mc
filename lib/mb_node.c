#include <stdio.h>
#include "mb_node.h"
#include "mm.h"


void * new_node(struct mm* m, int mb_cnt, int nh_cnt, int level)
{
    // one for result table
    // one for child table
    int r_c = UP_RULE(nh_cnt); 
    void *ret;

    ret = alloc_node(m, (UP_CHILD(mb_cnt) + r_c), level);
    if(!ret)
        return NULL;

    ret = POINT(ret) + r_c;
    return ret;
}


void free_node(struct mm *m, void *ptr, uint32_t cnt, int level)
{
    if (ptr){ 
        dealloc_node(m, cnt, level, ptr);
    }
}


#ifdef COMPRESS_NHI
int is_redund(struct mb_node *node, int pos, void *nhi) 
{
    BITMAP_TYPE tmp = node->internal;
    /* clear the 0~pos+1 bits */
    tmp = (tmp >> (pos+1) ) << (pos+1);
    if(tmp == 0)
        return 0;

    int idx = rightmost_1bit_idx(tmp);
    void **iter = pointer_to_nhi(node, idx);

    return *iter == nhi;
}

#endif


//pos start from 1
//
void extend_rule(struct mm *m, struct mb_node *node, uint32_t pos, int level, void *nhi)
{
    int child_num = count_children(node->external);
#ifndef COMPRESS_NHI
    int rule_num = count_children(node->internal);
#else

     /* a rule is a pointer, in 64-bit machine, each pointer takes
     *8 bytes, in 32-bit, each pointer takes 4 bytes, the same size 
     *as the bitmap, so, add rule_num by 1, we put a bitmap in 
     *rule arrays.
     */
    int rule_num = 0;
    if(node->internal)
        rule_num = count_children(*get_inl_mask(node)) + 1;
#endif

    void **i;
    void **j;

#ifdef COMPRESS_NHI
    if(is_redund(node, pos, nhi)) {
        //only update internal bitmap
        update_inl_bitmap(node, pos);
        return;
    }

    if(node->internal) { 
        set_bitmap(get_inl_mask(node), pos);
        //including the bitmap, the offset should add 1
        update_inl_bitmap(node, pos);
        pos = count_ones(*get_inl_mask(node), pos) + 1 + 1;
    }
    else {
        //including the bitmap vec
        update_inl_bitmap(node, pos);
        pos = 2;
    }
#else
    update_inl_bitmap(node, pos);
    pos = count_ones(node->internal, pos) + 1; 
#endif

    void *n = new_node(m, child_num, rule_num + 1, level);

    if (child_num != 0){
        //copy the child
        memcpy(n,node->child_ptr,sizeof(struct mb_node)*UP_CHILD(child_num));
    }
    

#ifdef COMPRESS_NHI
    //copy bitmap
    BITMAP_TYPE *new_mask = (BITMAP_TYPE *)n - 1;
    if(rule_num > 0) {
        *new_mask = *get_inl_mask(node);
    }
    else {
        *new_mask = node->internal;
    }
#endif
    //insert the rule at the pos position

    if (rule_num != 0) {
        //copy the 1~pos-1 part
        i = (void**)node->child_ptr - pos + 1;
        j = (void**)n - pos + 1;
        
        memcpy(j,i,(pos-1)*sizeof(void*));

        //copy the pos+1~rule_num part
        i = (void**)node->child_ptr - rule_num;
        j = (void**)n - rule_num - 1;

        memcpy(j,i,(rule_num - pos + 1)*sizeof(void*));
    }
    


    i = (void**)n - pos;
    *i = nhi;
    
    //need to be atomic
    if (node->child_ptr) {
        free_node(m, POINT(node->child_ptr) - UP_RULE(rule_num), UP_CHILD(child_num) + UP_RULE(rule_num), level);
    }
    node->child_ptr = n; 

    //if(node->internal) {
    //    if(node->internal != *get_inl_mask(node)) {
    //        printf("here\n");
    //    }
    //}

}

//return the extended node
//
struct mb_node * extend_child(struct mm *m, struct mb_node *node, int level,
        uint32_t pos)
{
    int child_num = count_children(node->external);
#ifndef COMPRESS_NHI
    int rule_num = count_children(node->internal);
#else
    int rule_num = 0;
    if(node->internal)
        rule_num = count_children(*get_inl_mask(node)) + 1;
#endif
    update_enl_bitmap(node, pos);
    pos = count_ones(node->external, pos);

    void *n = new_node(m, child_num +1 ,rule_num, level);

    void **i;
    void **j;
    
    if (rule_num != 0) {
        //copy the rules
        i = (void **)node->child_ptr - rule_num;
        j = (void **)n - rule_num;
        memcpy(j,i,(rule_num)*sizeof(void*));
    }

    if (child_num != 0) {
        //copy the  0~pos-1 part
        memcpy(n,node->child_ptr,(pos)*sizeof(struct mb_node));
        //copy the pos~child_num-1 part to pos+1~child_num 
        memcpy((struct mb_node*)n+pos+1,(struct mb_node*)node->child_ptr + pos, (child_num - pos) * sizeof(struct mb_node));
    }


    //need to be atomic
    if (node->child_ptr) {
        free_node(m, POINT(node->child_ptr) - UP_RULE(rule_num), UP_CHILD(child_num) + UP_RULE(rule_num), level);
    }

    node->child_ptr = n;
    //if(node->internal) {
    //    if(node->internal != *get_inl_mask(node)) {
    //        printf("here\n");
    //    }
    //}


    return (struct mb_node*)n + pos;
}

void reduce_child(struct mm *m, struct mb_node *node, int pos, int level)
{
    int child_num = count_children(node->external);
#ifndef COMPRESS_NHI
    int rule_num = count_children(node->internal);
#else
    int rule_num = 0;
    if(node->internal)
        rule_num = count_children(*get_inl_mask(node)) + 1;
#endif
    int clear_bit_idx = pos;
    pos = count_ones(node->external, pos);
    clear_bitmap(&node->external, clear_bit_idx);

    if (child_num < 1){
        printf("reduce_child: error!\n");
    }

    void *n = new_node(m, child_num -1 ,rule_num, level);

    void **i;
    void **j;
    
    if (rule_num != 0) {
        //copy the rules
        i = (void **)node->child_ptr - rule_num;
        j = (void **)n - rule_num;
        memcpy(j,i,(rule_num)*sizeof(void*));
    }

    if (child_num > 1) {
        //copy the 0~pos-1 part
        memcpy(n,node->child_ptr,(pos)*sizeof(struct mb_node));
        //copy the pos+1~child_num part to pos~child_num-1 
        memcpy((struct mb_node*)n+pos,(struct mb_node*)node->child_ptr + pos + 1, (child_num - pos -1) * sizeof(struct mb_node));
    }


    //need to be atomic
    if (node->child_ptr) {
        free_node(m, POINT(node->child_ptr) - UP_RULE(rule_num), UP_CHILD(child_num) + UP_RULE(rule_num), level);
    }
    node->child_ptr = n;
    //if(node->internal) {
    //    if(node->internal != *get_inl_mask(node)) {
    //        printf("here\n");
    //    }
    //}
}

void reduce_rule(struct mm *m, struct mb_node *node, uint32_t pos, int level)
{
    int child_num = count_children(node->external);
#ifndef COMPRESS_NHI
    int rule_num = count_children(node->internal);
#else
    //including the bitmap vec, the rule_num should be 1
    int rule_num = 0; 
    if(node->internal)
        rule_num = count_children(*get_inl_mask(node)) + 1;
#endif

    void **i;
    void **j;

    int clear_bit_idx = pos;
#ifdef COMPRESS_NHI
    if(!test_bitmap(*get_inl_mask(node), pos)) {
        //only clear internal bitmap
        clear_bitmap(&node->internal, clear_bit_idx);
        return;
    }
    pos = count_ones(*get_inl_mask(node), pos) + 1 + 1;
    clear_bitmap(get_inl_mask(node), clear_bit_idx);
#else
    pos = count_ones(node->internal, pos) + 1;
#endif
    clear_bitmap(&node->internal, clear_bit_idx);
   
#ifdef COMPRESS_NHI
    if (rule_num <= 1){
#else
    if (rule_num < 1){
#endif
        printf("reduce_rule: error!\n");
        return;
    }

#ifndef COMPRESS_NHI
    void *n = new_node(m, child_num, rule_num - 1, level);
#else
    void *n;
    if(rule_num == 2)
        n = new_node(m, child_num, 0, level);
    else 
        n = new_node(m, child_num, rule_num -1, level);
#endif

    if (child_num != 0){
        //copy the child
        memcpy(n,node->child_ptr,sizeof(struct mb_node)*UP_CHILD(child_num));
    }

    //delete the rule at the pos position
#ifndef COMPRESS_NHI
    if (rule_num > 1) {
#else
    if (rule_num > 2) {
#endif
        //copy the 1~pos-1 part
        i = (void**)node->child_ptr - pos + 1;
        j = (void**)n - pos + 1;
        
        memcpy(j,i,(pos-1)*sizeof(void*));

        //copy the pos~rule_num part
        i = (void**)node->child_ptr - rule_num;
        j = (void**)n - rule_num + 1;

        memcpy(j,i,(rule_num - pos)*sizeof(void*));
    }

    //need to be atomic
    if (node->child_ptr) {
        free_node(m, POINT(node->child_ptr) - UP_RULE(rule_num), UP_CHILD(child_num) + UP_RULE(rule_num), level);
    }
    node->child_ptr = n; 

    //if(node->internal) {
    //    if(node->internal != *get_inl_mask(node)) {
    //        printf("here\n");
    //    }
    //}
}


#ifdef FAST_TREE_FUNCTION
static BITMAP_TYPE fct[1<<(STRIDE - 1)];

__attribute__((constructor)) void fast_lookup_init()
{
    uint32_t i;
    int j;
    int pos;

    uint32_t stride;
    for(i=0;i<(1<<(STRIDE-1));i++) {
        stride = i;
        for(j = STRIDE -1; j>=0; j--) {
            pos = count_inl_bitmap(stride, j);
            set_bitmap(fct+i, pos);
            stride >>= 1;
        }
    }
}

int tree_function(BITMAP_TYPE bitmap, uint8_t stride)
{
    BITMAP_TYPE ret;
    int pos;

    ret = fct[(stride>>1)] & bitmap;
    if(ret){
        pos = __builtin_clzll(ret);
        return BITMAP_BITS - 1 - pos;
    }
    else
        return -1;
}

#else

int tree_function(BITMAP_TYPE bitmap, uint8_t stride)
{
    int i;
    int pos;
    if (bitmap == 0ULL)
        return -1;
    for(i=STRIDE-1;i>=0;i--){
        stride >>= 1;
        pos = count_inl_bitmap(stride, i); 
        if (test_bitmap(bitmap, pos)){
            return pos;
        }
    }
    return -1;
}
#endif


int find_overlap_in_node(BITMAP_TYPE bitmap, uint8_t stride, uint8_t *mask, int limit_inside)
{
    int i;
    int pos;
    if (bitmap == 0ULL)
        return -1;
    //calulate the beginning bits
    stride >>= (STRIDE - limit_inside);

    for(i=limit_inside-1;i>=0;i--){
        stride >>= 1;
        *mask = i;
        pos = count_inl_bitmap(stride, i); 
        if (test_bitmap(bitmap, pos)){
            return pos;
        }
    }

    return -1;
}


void destroy_subtrie(struct mb_node *node, struct mm *m, void (*destroy_nhi)(void *nhi), int depth)
{
    int bit;
    int cidr;
    int pos;
    void ** nhi = NULL;
    int stride;
    struct mb_node *next = NULL;

    
    int cnt_rules;
    struct mb_node *first = NULL;

    for (cidr=0;cidr<= STRIDE -1;cidr ++ ){
        for (bit=0;bit< (1<<cidr);bit++) {
            pos = count_inl_bitmap(bit,cidr);
            if (test_bitmap(node->internal, pos)) {
                //nhi = (struct next_hop_info**)node->child_ptr - count_ones(node->internal, pos) - 1;
                nhi = pointer_to_nhi(node, pos);
                if (destroy_nhi && *nhi != NULL) {
                    destroy_nhi(*nhi);
                }
                *nhi = NULL;
            }
        }

    }


    for (stride = 0; stride < (1<<STRIDE); stride ++ ){
        pos = count_enl_bitmap(stride);
        if (test_bitmap(node->external, pos)) {
            //next = (struct mb_node *)node->child_ptr + count_ones(node->external, pos);
            next = next_child(node, pos);
            destroy_subtrie(next, m, destroy_nhi, depth + 1);
        }
    }

#ifndef COMPRESS_NHI
    cnt_rules = count_children(node->internal);
#else
    if(node->internal)
        cnt_rules = count_children(*get_inl_mask(node)) + 1;
    else
        cnt_rules = 0;
#endif
    first = POINT(node->child_ptr) - UP_RULE(cnt_rules);

    node->internal = 0;
    node->external = 0;
    node->child_ptr = NULL;

    int cnt_children = count_children(node->external);
    free_node(m, first, UP_RULE(cnt_rules) + UP_CHILD(cnt_children), depth);
}

