#ifndef MM_COLOR_H_
#define MM_COLOR_H_

#include "list.h"
#include <stdint.h>


#define SHARE 3 
#define STRIDE 6 

struct cache_share
{
    uint32_t page_color;
    uint32_t size;
};

struct lm_area
{
    void *start;
    void * alloc;
    uint32_t left;

    struct list_head list;
    struct cache_share *pcs;
};

struct free_pointer
{
    void * p;
    struct list_head list;
};


struct mc
{
    void *addr;
    int fd;
    uint64_t *pfnbuf;

    struct cache_share cs[SHARE];
    struct list_head lm[SHARE];
    struct list_head free_head[SHARE][(1<<STRIDE)*2];


    //profile info
    int lma;
    int free_node[SHARE][(1<<STRIDE)*2];
};




int mc_init(struct mc *m);

void *alloc_node(struct mc *m, uint32_t node_num, uint32_t level);
void dealloc_node(struct mc *m, uint32_t node_num, uint32_t level, void *p);
int mc_uinit(struct mc *m);
int mc_profile(struct mc *m);

#endif
