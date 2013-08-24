#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "util.h"


void out_of_memory()
{
    printf("out of memory\n");
    exit(-1);
}



void *xcalloc(size_t count, size_t size)
{
    void *p = count && size ? calloc(count, size) : malloc(1);
    if (p == NULL) {
        out_of_memory();
    }
    return p;
}

void *xzalloc(size_t size)
{
    return xcalloc(1, size);
}

//copy from util.c

void *xmalloc(size_t size)
{
    void *p = malloc(size ? size : 1);
    if (p == NULL) {
        out_of_memory();
    }
    return p;
}


#ifdef UP_STATS
int node_cpy;
int node_alloc;
int node_set;
int st_f = 1;
#endif


void mem_op_v6()
{
#ifdef UP_STATS
    printf("node copy %d\n", node_cpy);
    printf("node alloc %d\n", node_alloc);
    printf("node set %d\n", node_set);
#endif
}

