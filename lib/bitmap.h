#ifndef _BITMAP_H__
#define _BITMAP_H__

#include "mb_node.h"
#include "mm.h"

//Node Type

#define LEAF_NODE 0 
#define MID_NODE 1

//Traverse continue
#define TRAVERSE_CONT 1 

struct trace{
    struct mb_node *node;
    uint32_t pos;
};

struct lazy_travel {
    struct mb_node *lazy_p;
    uint32_t stride;
};

int update_nodes(struct mm *mm, struct trace *t, int total);
typedef int (*traverse_func) (struct mb_node *node, 
        uint8_t stride, uint8_t pos, uint8_t type, void *data);

int prefix_exist_func(struct mb_node *node, 
        uint8_t stride, uint8_t pos, uint8_t type, void *data);


#endif
