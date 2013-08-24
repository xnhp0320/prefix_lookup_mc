#ifndef _MB_NODE_H_
#define _MB_NODE_H_


#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "mm_color.h"

//multi-bit node data structure


struct mb_node_v6{
    uint64_t external;
    uint64_t internal;
    void     *child_ptr;
}__attribute__((aligned(8)));

struct next_hop_info {
};

struct mem_stats_v6 {
    uint32_t mem;
    uint32_t node;
};

#define PREFIX_HI 4 
#define INIT_HAS_A_CHILD 0x01
#define INIT_HAS_A_NEXT  0x02


union node_entity_v6 {
    struct mb_node_v6 node;
    struct next_hop_info *ptr;
};

// uint8_t flags 8 bits  
// | 7 6 5 4 | 3 2 1 0 |
// | prefix  | flags   |
// | 0 ~ 13  |     0 0 |
struct init_node_v6{
    uint8_t flags; 
    union node_entity_v6 e;
}__attribute__((aligned(8)));

#define POINT(X) ((struct mb_node_v6*)(X))
#define NODE_SIZE  (sizeof(struct mb_node_v6))
#define BITMAP_TYPE uint64_t

static inline uint32_t UP_RULE(uint32_t x)
{
    return (x*sizeof(struct next_hop_info*) + NODE_SIZE - 1)/(NODE_SIZE);
}

static inline uint32_t UP_CHILD(uint32_t x)
{
    return x;
}



void * new_node(struct mc* mm, int mb_cnt, int result_cnt, int level, int use_mm);
void free_node(struct mc *mm, void *ptr, uint32_t cnt, int level, int use_mm);

//count 1's from the right of the pos, not including the 1
//pos = 0 ~ 64
//pos == 64 means to counts how many 1's in the bitmap
//I'm not sure this is a implementation specific.
static inline int count_ones(BITMAP_TYPE bitmap, uint8_t pos)
{
//    if (pos == 0)
//        return 0;
//    return __builtin_popcountl((bitmap<<(64 - pos)));
#if __SIZEOF_LONG__ == 8 
    return __builtin_popcountl(bitmap>>pos) - 1;
#else
    return __builtin_popcountll(bitmap>>pos) - 1;
#endif

}

static inline int count_children(BITMAP_TYPE bitmap)
{
#if __SIZEOF_LONG__ == 8
    return __builtin_popcountl(bitmap);
#else
    return __builtin_popcountll(bitmap);
#endif
}

static inline uint32_t count_inl_bitmap(uint32_t bit, int cidr)
{
    uint32_t pos = (1<<cidr) + bit;
    return (pos - 1);
}

static inline uint32_t count_enl_bitmap(uint32_t bits)
{
    return (bits);
}

static inline void update_inl_bitmap(struct mb_node_v6 *node, int pos)
{
    node->internal |= (1ULL << pos);
}

static inline void update_enl_bitmap(struct mb_node_v6 *node, int pos)
{
    node->external |= (1ULL << pos);
}

static inline BITMAP_TYPE test_bitmap(BITMAP_TYPE bitmap, int pos)
{
    return (bitmap & (1ULL << pos));
}


// ----child_ptr-----
// --------|---------
// |rules  | child--| 
// |-------|--------|
// to get the head of the memory : POINT(x) - UP_RULE(x)
// 

struct mb_node_v6 * extend_child(struct mc *mm, struct mb_node_v6 *node,
        int level, uint32_t pos, int use_mm);

void extend_rule(struct mc *mm, struct mb_node_v6 *node, uint32_t pos,
        int level, struct next_hop_info *nhi, int use_mm);
void reduce_child(struct mc *mm, struct mb_node_v6 *node, int pos, int level, int use_mm);
void reduce_rule(struct mc *mm, struct mb_node_v6 *node, uint32_t pos,
        int level, int use_mm);

void mem_subtrie(struct mb_node_v6 *n, struct mem_stats_v6 *ms);
int tree_function(BITMAP_TYPE bitmap, uint8_t stride);
int find_overlap_in_node(BITMAP_TYPE bitmap, uint8_t stride, uint8_t *mask, int limit_inside);

static inline void clear_bitmap(BITMAP_TYPE *bitmap, int pos)
{
    *bitmap &= (~(1ULL << pos));
}

static inline void set_bitmap(BITMAP_TYPE *bitmap, int pos)
{
    *bitmap |= (1ULL<<pos);
}

#define DEBUG_MEMORY_FREE
#define DEBUG_MEMORY_ALLOC

#ifdef DEBUG_MEMORY_FREE
extern int mem_destroy;
#endif


#define FAST_TREE_FUNCTION
//#define USE_MM

#ifndef USE_MM
void destroy_subtrie(struct mb_node_v6 *node, void (*destroy_nhi)(struct next_hop_info *nhi));
#else
void destroy_subtrie(struct mb_node_v6 *node, void (*destroy_nhi)(struct next_hop_info *nhi), struct mc *m, int depth, int use_mm);
#endif


#endif
