#ifndef __MM_H__
#define __MM_H__

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include "list.h"


#define ADD_ALLOCATOR(alloc) \
    __attribute__((constructor)) void __allocator_register_##alloc() \
{ \
    int ret; \
    ret = add_allocator(&alloc); \
    assert(ret == 0); \
}


#define MAX_LEVEL 32

struct mem_stat {
    uint32_t mem;
    uint32_t node;
    uint32_t lmem[MAX_LEVEL];
    uint32_t lnode[MAX_LEVEL];
};



struct allocator {
    struct list_head list;
    struct mem_op *op;
};

struct mm {
    struct mem_stat ms;
    struct mem_op *op;
};

struct mem_op {
    int type;   
    int priv_size;
    void *priv;
    int (*init)(struct mm *m);
    void *(*alloc_node)(struct mm *m, uint32_t node_num, uint32_t level);
    void (*dealloc_node)(struct mm *m, uint32_t node_num, uint32_t level, void *p);
    void (*mm_profile)(struct mm *m);
    int (*uinit)(struct mm *m);
};

static inline void *mm_get_priv(struct mm *m)
{
    return m->op->priv;
}


void *alloc_node(struct mm *m, uint32_t node_num, uint32_t level);
void dealloc_node(struct mm *m, uint32_t node_num, uint32_t level, void *p);
int mm_init(struct mm *m, int type);
int mm_uinit(struct mm *m);
int add_allocator(struct mem_op *op);

void mm_profile(struct mm *m);

#define MEM_ALLOC_SIMPLE 0
#define MEM_ALLOC_COLOR 1

#endif
