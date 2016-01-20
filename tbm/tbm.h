#ifndef __TBM_H__
#define __TBM_H__

#include "lib/mb_node.h"

#define INIT_HAS_A_CHILD 0x00000001
#define PREFIX_HI 16


union init_entry{
    struct mb_node node;
    void *ptr;
};

struct init_node {
    uint32_t flags;
    union init_entry e;
};



#endif
