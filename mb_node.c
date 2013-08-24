#include "mb_node.h"
#include "fast_lookup.h"

#ifdef DEBUG_MEMORY_ALLOC
static int mem_v6;
#endif

void mem_alloc_stat_v6()
{
#ifdef DEBUG_MEMORY_ALLOC
    printf("mem alloc %d\n", mem_v6);
#endif
}


void * new_node(struct mc* m, int mb_cnt, int result_cnt, int level, int use_mm)
{
    // one for result table
    // one for child table
    int r_c = UP_RULE(result_cnt); 
    int size = (UP_CHILD(mb_cnt) + r_c) * NODE_SIZE;
    void *ret;
#ifndef USE_MM
    if (size != 0) 
        ret = calloc(1,size);
    else
        ret = NULL;
#ifdef DEBUG_MEMORY_ALLOC
    mem_v6 += (UP_CHILD(mb_cnt) + r_c ) * NODE_SIZE;
#endif
    //node_num += UP_CHILD(mb_cnt) + r_c;
#else
    if(use_mm)
        ret = alloc_node(m, (UP_CHILD(mb_cnt) + r_c), level);
    else {
        if (size != 0)
            ret = calloc(1,size);
        else
            ret = NULL;
#ifdef DEBUG_MEMORY_ALLOC
        mem_v6 += (UP_CHILD(mb_cnt) + r_c ) * NODE_SIZE;
#endif
    }
#endif
    ret = POINT(ret) + r_c;
    return ret;
}




void free_node(struct mc *m, void *ptr, uint32_t cnt, int level, int use_mm)
{
    if (ptr){ 
#ifndef USE_MM
        free(ptr);
        //if(cnt == 0)
        //    printf("strange\n");
#ifdef DEBUG_MEMORY_ALLOC
        mem_v6 -= cnt * NODE_SIZE;
#endif
        //node_num -= cnt;
#else
        if(use_mm) 
            dealloc_node(m, cnt, level, ptr);
        else {
            free(ptr);
#ifdef DEBUG_MEMORY_ALLOC
            mem_v6 -= cnt * NODE_SIZE;
#endif
        }
#endif
    }
}


//pos start from 1
//
void extend_rule(struct mc *m, struct mb_node_v6 *node, uint32_t pos, int level,
        struct next_hop_info *nhi, int use_mm)
{
    int child_num = count_children(node->external);
    int rule_num = count_children(node->internal) - 1;

    struct next_hop_info **i;
    struct next_hop_info **j;
    
    void *n = new_node(m, child_num, rule_num + 1, level, use_mm);

    if (child_num != 0){
        //copy the child
        memcpy(n,node->child_ptr,sizeof(struct mb_node_v6)*UP_CHILD(child_num));
#ifdef UP_STATS
        if (st_f){
            node_cpy += UP_CHILD(child_num);
        }
#endif
    }
    

#ifdef UP_STATS
    if (st_f) {
        node_alloc++;
    }
#endif

    //insert the rule at the pos position

    if (rule_num != 0) {
        //copy the 1~pos-1 part
        i = (struct next_hop_info**)node->child_ptr - pos + 1;
        j = (struct next_hop_info**)n - pos + 1;
        
        memcpy(j,i,(pos-1)*sizeof(struct next_hop_info*));

        //copy the pos~rule_num part
        i = (struct next_hop_info**)node->child_ptr - rule_num;
        j = (struct next_hop_info**)n - rule_num - 1;

        memcpy(j,i,(rule_num - pos + 1)*sizeof(struct next_hop_info*));
#ifdef UP_STATS
        if (st_f) {
            node_cpy += UP_RULE(rule_num);
        }
#endif
    }

    i = (struct next_hop_info**)n - pos;
    *i = nhi;
    
    //need to be atomic
    if (node->child_ptr) {
        free_node(m, POINT(node->child_ptr) - UP_RULE(rule_num), UP_CHILD(child_num) + UP_RULE(rule_num), level, use_mm);
    }
    node->child_ptr = n; 
}

//return the extended node
//
struct mb_node_v6 * extend_child(struct mc *m, struct mb_node_v6 *node, int level,
        uint32_t pos, int use_mm)
{
    int child_num = count_children(node->external) - 1;
    int rule_num = count_children(node->internal);

    void *n = new_node(m, child_num +1 ,rule_num, level, use_mm);

    struct next_hop_info **i;
    struct next_hop_info **j;
    
    if (rule_num != 0) {
        //copy the rules
        i = (struct next_hop_info **)node->child_ptr - rule_num;
        j = (struct next_hop_info **)n - rule_num;
        memcpy(j,i,(rule_num)*sizeof(struct next_hop_info*));
#ifdef UP_STATS
        if (st_f)
            node_cpy += UP_RULE(rule_num);
#endif
    }

    if (child_num != 0) {
    //copy the  0~pos-1 part
        memcpy(n,node->child_ptr,(pos)*sizeof(struct mb_node_v6));
    //copy the pos~child_num-1 part to pos+1~child_num 
        memcpy((struct mb_node_v6*)n+pos+1,(struct mb_node_v6*)node->child_ptr + pos, (child_num - pos) * sizeof(struct mb_node_v6));
#ifdef UP_STATS
        if(st_f)
            node_cpy += UP_CHILD(child_num);
#endif
    }

#ifdef UP_STATS
    if(st_f)
        node_alloc ++;
#endif

    //need to be atomic
    if (node->child_ptr) {
        free_node(m, POINT(node->child_ptr) - UP_RULE(rule_num), UP_CHILD(child_num) + UP_RULE(rule_num), level, use_mm);
    }
    node->child_ptr = n;
    return (struct mb_node_v6*)n + pos;
}

void reduce_child(struct mc *m, struct mb_node_v6 *node, int pos, int level, int use_mm)
{
    int child_num = count_children(node->external);
    int rule_num = count_children(node->internal);

    if (child_num < 1){
        printf("reduce_rule: error!\n");
    }

    void *n = new_node(m, child_num -1 ,rule_num, level, use_mm);

    struct next_hop_info **i;
    struct next_hop_info **j;
    
    if (rule_num != 0) {
        //copy the rules
        i = (struct next_hop_info **)node->child_ptr - rule_num;
        j = (struct next_hop_info **)n - rule_num;
        memcpy(j,i,(rule_num)*sizeof(struct next_hop_info*));
#ifdef UP_STATS
        if (st_f)
            node_cpy += UP_RULE(rule_num);
#endif
    }

    if (child_num > 1) {
    //copy the  0~pos-1 part
        memcpy(n,node->child_ptr,(pos)*sizeof(struct mb_node_v6));
    //copy the pos+1~child_num part to pos~child_num-1 
        memcpy((struct mb_node_v6*)n+pos,(struct mb_node_v6*)node->child_ptr + pos + 1, (child_num - pos -1) * sizeof(struct mb_node_v6));
#ifdef UP_STATS
        if(st_f)
            node_cpy += UP_CHILD(child_num);
#endif
    }
#ifdef UP_STATS
    if(st_f)
        node_alloc++;
#endif

    //need to be atomic
    if (node->child_ptr) {
        free_node(m, POINT(node->child_ptr) - UP_RULE(rule_num), UP_CHILD(child_num) + UP_RULE(rule_num), level, use_mm);
    }
    node->child_ptr = n;
}

void reduce_rule(struct mc *m, struct mb_node_v6 *node, uint32_t pos, int level, int use_mm)
{
    int child_num = count_children(node->external);
    int rule_num = count_children(node->internal);

    struct next_hop_info **i;
    struct next_hop_info **j;
    
    if (rule_num < 1){
        printf("reduce_rule: error!\n");
        return;
    }

    void *n = new_node(m, child_num, rule_num - 1, level, use_mm);

    if (child_num != 0){
        //copy the child
        memcpy(n,node->child_ptr,sizeof(struct mb_node_v6)*UP_CHILD(child_num));
#ifdef UP_STATS
        if(st_f)
            node_cpy += UP_CHILD(child_num);
#endif
    }
    


    //delete the rule at the pos position

    if (rule_num > 1) {
        //copy the 1~pos-1 part
        i = (struct next_hop_info**)node->child_ptr - pos + 1;
        j = (struct next_hop_info**)n - pos + 1;
        
        memcpy(j,i,(pos-1)*sizeof(struct next_hop_info*));

        //copy the pos~rule_num part
        i = (struct next_hop_info**)node->child_ptr - rule_num;
        j = (struct next_hop_info**)n - rule_num + 1;

        memcpy(j,i,(rule_num - pos)*sizeof(struct next_hop_info*));
#ifdef UP_STATS
        if(st_f)
            node_cpy += UP_RULE(rule_num);
#endif
    }

#ifdef UP_STATS
    if(st_f)
        node_alloc++;
#endif

    //need to be atomic
    if (node->child_ptr) {
        free_node(m, POINT(node->child_ptr) - UP_RULE(rule_num), UP_CHILD(child_num) + UP_RULE(rule_num), level, use_mm);
    }
    node->child_ptr = n; 
}

void mem_subtrie(struct mb_node_v6 *n, struct mem_stats_v6 *ms)
{
    int stride;
    int pos;
    struct mb_node_v6 *next;
    int child_num = count_children(n->external);
    int rule_num = count_children(n->internal);
    

    ms->mem += (UP_RULE(rule_num) + UP_CHILD(child_num)) * NODE_SIZE;
    ms->node += (UP_RULE(rule_num) + UP_CHILD(child_num));

    
    for (stride = 0; stride < (1<<STRIDE); stride ++ ){
        pos = count_enl_bitmap(stride);
        if (test_bitmap(n->external, pos)) {
            next = (struct mb_node_v6 *)n->child_ptr + count_ones(n->external, pos);
            mem_subtrie(next, ms);
        }
    }


}


int tree_function(BITMAP_TYPE bitmap, uint8_t stride)
{
#ifndef FAST_TREE_FUNCTION
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
#else
    BITMAP_TYPE ret;
    int pos;

    ret = fct[(stride>>1)] & bitmap;
    if(ret){
        pos = __builtin_clzll(ret);
        return 63 - pos;
    }
    else
        return -1;
#endif
}

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

#ifdef DEBUG_MEMORY_FREE
int mem_destroy;
#endif

#ifndef USE_MM
void destroy_subtrie(struct mb_node_v6 *node, void (*destroy_nhi)(struct next_hop_info *nhi))
#else
void destroy_subtrie(struct mb_node_v6 *node, void (*destroy_nhi)(struct next_hop_info *nhi), struct mc *m, int depth, int use_mm)
#endif
{
    int bit;
    int cidr;
    int pos;
    struct next_hop_info ** nhi = NULL;
    int stride;
    struct mb_node_v6 *next = NULL;

    
    int cnt_rules;
    struct mb_node_v6 *first = NULL;

    for (cidr=0;cidr<= STRIDE -1;cidr ++ ){
        for (bit=0;bit< (1<<cidr);bit++) {
            pos = count_inl_bitmap(bit,cidr);
            if (test_bitmap(node->internal, pos)) {
                nhi = (struct next_hop_info**)node->child_ptr - count_ones(node->internal, pos) - 1;
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
            next = (struct mb_node_v6 *)node->child_ptr + count_ones(node->external, pos);
#ifndef USE_MM
            destroy_subtrie(next, destroy_nhi);
#else
            destroy_subtrie(next, destroy_nhi, m, depth + 1, use_mm);
#endif
        }
    }

    cnt_rules = count_children(node->internal);
    first = POINT(node->child_ptr) - UP_RULE(cnt_rules);


#ifdef DEBUG_MEMORY_FREE
    int cnt = count_children(node->internal);
    mem_destroy += UP_RULE(cnt) * NODE_SIZE;
    cnt = count_children(node->external);
    mem_destroy += UP_CHILD(cnt) * NODE_SIZE;
#endif


    node->internal = 0;
    node->external = 0;
    node->child_ptr = NULL;

#ifdef USE_MM
    //printf("not supported\n");
    int cnt_children = count_children(node->external);
    free_node(m, first, UP_RULE(cnt_rules) + UP_CHILD(cnt_children), depth, use_mm);
#else
    free(first);
#endif

}


