#ifndef _MB_NODE_H_
#define _MB_NODE_H_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

//A multi-bit node data structure

#if __SIZEOF_LONG__ == 8
#define BITMAP_TYPE uint64_t
#define STRIDE 6
#define BITMAP_BITS (1<<STRIDE)
#else
#define BITMAP_TYPE uint32_t
#define STRIDE 5 
#define BITMAP_BITS (1<<STRIDE)
#endif

#define POINT(X) ((struct mb_node*)(X))
#define NODE_SIZE  (sizeof(struct mb_node))
#define FAST_TREE_FUNCTION
//#define COMPRESS_NHI

struct mb_node{
    BITMAP_TYPE external;
    BITMAP_TYPE internal;
    void     *child_ptr;
}__attribute__((aligned(8)));


static inline BITMAP_TYPE *get_inl_mask(struct mb_node *n)
{
    BITMAP_TYPE *inl_mask = (BITMAP_TYPE *)n->child_ptr - 1;
    return inl_mask;
}

static inline uint32_t UP_RULE(uint32_t x)
{
    return (x*sizeof(void*) + NODE_SIZE - 1)/(NODE_SIZE);
}

static inline uint32_t UP_CHILD(uint32_t x)
{
    return x;
}


static inline int count_ones(BITMAP_TYPE bitmap, uint8_t pos)
{
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

static inline void update_inl_bitmap(struct mb_node *node, int pos)
{
    node->internal |= (1ULL << pos);
}

static inline void update_enl_bitmap(struct mb_node *node, int pos)
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
#include "mm.h"
void *new_node(struct mm* mm, int mb_cnt, int result_cnt, int level);
void free_node(struct mm *mm, void *ptr, uint32_t cnt, int level);

struct mb_node * extend_child(struct mm *mm, struct mb_node *node,
        int level, uint32_t pos);

void extend_rule(struct mm *mm, struct mb_node *node, uint32_t pos,
        int level, void *nhi);
void reduce_child(struct mm *mm, struct mb_node *node, int pos, int level);
void reduce_rule(struct mm *mm, struct mb_node *node, uint32_t pos,
        int level);

//void mem_subtrie(struct mb_node *n, struct mem_stats *ms);
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

static inline int
raw_ctz(uint64_t n)
{
    /* With GCC 4.7 on 32-bit x86, if a 32-bit integer is passed as 'n', using
     * a plain __builtin_ctzll() here always generates an out-of-line function
     * call. The test below helps it to emit a single 'bsf' instruction. */
    return (__builtin_constant_p(n <= UINT32_MAX) && n <= UINT32_MAX
            ? __builtin_ctz(n)
            : __builtin_ctzll(n));
}

static inline int ctz(BITMAP_TYPE n)
{
#if __SIZEOF_LONG__ == 8
    return n ? raw_ctz(n) : 64;
#else
    return n ? raw_ctz(n) : 32;
#endif
}

/* Returns the index of the rightmost 1-bit in 'x' (e.g. 01011000 => 3), or an
 * undefined value if 'x' is 0. */
static inline int rightmost_1bit_idx(BITMAP_TYPE x)
{
    return ctz(x);
}


static inline struct mb_node *next_child(struct mb_node *current, uint8_t pos)
{
    return (struct mb_node*)current->child_ptr + count_ones(current->external, pos); 
}

//NHI next hop information
static inline void **pointer_to_nhi(struct mb_node *current, uint8_t pos)
{
#ifndef COMPRESS_NHI
    return (void**)current->child_ptr - count_ones(current->internal, pos) - 1;
#else
    return (void**)current->child_ptr - count_ones(*get_inl_mask(current), pos) - 2;
#endif
}

void destroy_subtrie(struct mb_node *node, struct mm *m, void (*destroy_nhi)(void *nhi), int depth);

//GCC optimize
#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)


#endif
