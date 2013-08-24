#include "mb_node.h"



#ifdef FAST_TREE_FUNCTION
BITMAP_TYPE fct[1<<(STRIDE - 1)];
#endif

void fast_table_init()
{
#ifdef FAST_TREE_FUNCTION
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
#endif
}



