#include "mm.h"
#include "mm_color.h"
#include "mb_node.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MEMORY_SIZE (16*1024*1024)
#define PAGE_SHIFT 12
#define PAGE_SIZE (1<<PAGE_SHIFT)


//----need chage when platform changed-------
#define CACHE_OVERLAP (128*1024)
#define HPAGE_SIZE (2*1024*1024)
#define PAGE_COLORS 32
//-------------------------------------------

#define PAGEMAP_MASK_PFN        (((uint64_t)1 << 55) - 1)
#define PAGEMAP_PAGE_PRESENT    ((uint64_t)1 << 63)

static int addr_pfn(struct mc_priv *m, int n)
{
    char    fname[32];
    int     fid, i;

    sprintf(fname, "/proc/%d/pagemap", getpid());
    fid = open(fname, O_RDONLY);

    if(fid < 0)
    {
        perror("failed to open pagemap address translator");
        return -1;
    }

    for(i=0;i<n;i++) {
        if(lseek(fid, (((unsigned long)m->addr + i*HPAGE_SIZE) >> PAGE_SHIFT) * 8, SEEK_SET) == (off_t)-1)
        {
            perror("failed to seek to translation start address");
            close(fid);
            return -1;
        }

        if(read(fid, m->pfnbuf+i, 8) < 8)
        {
            perror("failed to read in all pfn info");
            close(fid);
            return -1;
        }

        if(m->pfnbuf[i] & PAGEMAP_PAGE_PRESENT)
        {
            m->pfnbuf[i] &= PAGEMAP_MASK_PFN;
        }
        else
        {
            m->pfnbuf[i] = 0;
        }

        printf("vaddr %p paddr 0x%lx\n", m->addr + i*HPAGE_SIZE, m->pfnbuf[i] << PAGE_SHIFT);
    }

    close(fid);
    return 0;
}

static int mc_init(struct mm *mm)
{
    struct mc_priv *m = mm_get_priv(mm);

    //8 colors
    m->cs[0].size = 32 * 1024;
    //22 colors
    m->cs[1].size = 22 * 4 * 1024;
    //2 colors
    m->cs[2].size = 2 * 4 * 1024;

    m->fd = open("/mnt/hugetlb/mem", O_CREAT | O_RDWR, S_IRWXU);
    if (m->fd < 0) {
        perror("open");
        return -1;
    }

    m->addr = mmap(0, MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, m->fd, 0); 
    if(m->addr == MAP_FAILED){ 
        perror("Err: "); 
        goto error;
    }   

    int nr = MEMORY_SIZE/HPAGE_SIZE;

    m->pfnbuf = (uint64_t*)malloc(nr * sizeof(uint64_t));
    if (!m->pfnbuf) {
        perror("malloc");
        goto merror;
    }

    int k;
    for(k=0;k<nr;k++)
    {
        *(unsigned char*)(m->addr + k * HPAGE_SIZE) = 'x';
    }


    addr_pfn(m, nr);


    int i,j;
    int old_size = 0;
    int next_color = 0;

    for(i=0;i<SHARE;i++){
        INIT_LIST_HEAD(&m->lm[i]);
        for(j=0;j<(1<<STRIDE)*2;j++){
            INIT_LIST_HEAD(&m->free_head[i][j]);
        }

        struct lm_area *lma = (struct lm_area*)malloc(sizeof(*lma));
        if(!lma) {
            perror("lma");
            goto pfnerror;
        }

        lma->start = m->addr + old_size; 
        lma->alloc = lma->start;
        lma->left = m->cs[i].size;
        lma->pcs = m->cs + i;
        old_size += lma->pcs->size;

        if(i==0) {
            m->cs[i].page_color = m->pfnbuf[0] % PAGE_COLORS;
            next_color = (m->cs[i].page_color + (m->cs[i].size / PAGE_SIZE)) % PAGE_COLORS;
        }
        else {
            m->cs[i].page_color = next_color;
            next_color = (m->cs[i].page_color + (m->cs[i].size / PAGE_SIZE)) % PAGE_COLORS;
        }

        INIT_LIST_HEAD(&lma->list);

        list_add(&lma->list, &m->lm[i]);
    }





    return 0;

pfnerror:
    free(m->pfnbuf);
merror:
    munmap(m->addr, MEMORY_SIZE); 
error:
    close(m->fd); 
    unlink("/mnt/hugetlb/mem");
    return -1;

}

static int lma_init(struct list_head *lma_list, struct lm_area *new_lma, 
        struct lm_area *last_lma, struct mc_priv *m)
{
    unsigned long last_page = (unsigned long)(last_lma->start - m->addr) / HPAGE_SIZE;

    unsigned long new_page = 
        (unsigned long)(last_lma->start + CACHE_OVERLAP + last_lma->pcs->size - m->addr) / HPAGE_SIZE;

    if(new_page != last_page) {
        //which means we need to find the new page
        //printf("new page\n");
        unsigned int start_page_color = m->pfnbuf[new_page] % PAGE_COLORS;
        if ( start_page_color < last_lma->pcs->page_color) {
            new_lma->start = m->addr + new_page * HPAGE_SIZE + 
                (last_lma->pcs->page_color - start_page_color)* PAGE_SIZE;
        }
        else if ( start_page_color > last_lma->pcs->page_color) {
            new_lma->start = m->addr + new_page * HPAGE_SIZE + 
                (PAGE_COLORS + last_lma->pcs->page_color - start_page_color) * PAGE_SIZE;
        }
        else {
            new_lma->start = m->addr + new_page * HPAGE_SIZE;
        }
    }
    else {
        new_lma->start = last_lma->start + CACHE_OVERLAP;
    }
    new_lma->alloc = new_lma->start;

    new_lma->left = last_lma->pcs->size;
    new_lma->pcs = last_lma->pcs;

    INIT_LIST_HEAD(&new_lma->list);
    list_add_tail(&new_lma->list, lma_list);
    return 0;
}

static struct lm_area *find_lma(struct list_head *lma_list, uint32_t node_num, struct mc_priv *m)
{
    struct lm_area *lma;
    list_for_each_entry(lma, lma_list, list) {
        if(lma->left > node_num * NODE_SIZE) {
            return lma;
        }
    }

    //find all the lma, no one can be used
    //so we have to built new lma

    struct lm_area *new_lma = (struct lm_area *)malloc(sizeof(*new_lma));
    struct lm_area *last_lma = list_entry(lma_list->prev, struct lm_area, list);
    
    if(!new_lma) {
        perror("malloc");
        return NULL;
    } 
    //profile
    m->lma++;

    lma_init(lma_list, new_lma, last_lma, m);
    return new_lma;
}


static void * lma_alloc(struct lm_area *lma, uint32_t node_num)
{
    void *ret = NULL;
    if(lma->left > node_num * NODE_SIZE) {
        ret = lma->alloc;
        lma->alloc += node_num * NODE_SIZE;
        lma->left -= node_num * NODE_SIZE;
        memset(ret, 0, node_num * NODE_SIZE);
    }
    return ret;
}


static void *lm_alloc(struct mc_priv *m, uint32_t node_num, uint32_t share)
{
    //try free head

    void *ret = NULL;

    struct free_pointer *fp= NULL;

    if(!list_empty(&(m->free_head[share][node_num]))) {
        fp = list_first_entry(&(m->free_head[share][node_num]), 
                struct free_pointer, list);

        list_del(&fp->list);
        ret = fp->p;
        memset(ret, 0, node_num * NODE_SIZE);
        free(fp);
    }
    else {
        struct lm_area *lma = find_lma(&(m->lm[share]), node_num, m);

        if(lma==NULL) {
            printf("lma==NULL\n");
            return NULL;
        }

        ret = lma_alloc(lma, node_num);
    }
    return ret;

}

static void *mc_alloc_node(struct mm *mm, uint32_t node_num, uint32_t level)
{
    struct mc_priv *m = mm_get_priv(mm);

    void *ret = NULL;

    if(node_num == 0) {
        printf("alloc 0\n");
        return NULL;
    }

    if(node_num > (1<<STRIDE) * 2) {
        printf("ahhhh~~\n");
        return NULL;
    }

    switch(level) {
        case 0:
            ret = lm_alloc(m, node_num, 0);
            break;
        case 1:
            ret = lm_alloc(m, node_num, 1);
            break;
        default:
            ret = lm_alloc(m, node_num, 2);
            break;
    }

    return ret;

}

static void mc_dealloc_node(struct mm *mm, uint32_t node_num, uint32_t level, void *p)
{
    struct mc_priv *m = mm_get_priv(mm); 

    struct free_pointer *fp = (struct free_pointer*)malloc(sizeof(*fp));
    if(!fp) {
        perror("malloc fp");
        exit(-1);
    }

    if(node_num > (1<<STRIDE) * 2) {
        printf("ah~~~\n");
        free(fp);
        return;
    }

    if(node_num == 0) {
        //printf("err free node_num 0\n");
    }

    int share;

    switch(level) {
        case 0:
            share =0;
            break;
        case 1:
            share = 1;
            break;
        default:
            share = 2;
            break;
    }

    

    fp->p = p;
    //memset(p, 0, node_num * NODE_SIZE);
    INIT_LIST_HEAD(&fp->list);
    list_add(&fp->list, &(m->free_head[share][node_num])); 

}


static void mc_profile(struct mm *mm)
{
    int i,j;
    struct free_pointer *fp;
    struct mc_priv *m = mm_get_priv(mm);
    
    for(i=0;i<SHARE;i++) {
        printf("SHARE 1:\n");
        for(j=0;j<(1<<STRIDE)*2;j++) 
        {
            list_for_each_entry(fp, &m->free_head[i][j], list){ 
                ((m->free_node)[i][j])++;
            }
            printf("free node %d %d\n", j, m->free_node[i][j]);
        }
    }

    printf("using lma %d\n", m->lma);
}


static int mc_uinit(struct mm *mm)
{
    struct mc_priv *m = mm_get_priv(mm);

    munmap(m->addr, MEMORY_SIZE); 
    close(m->fd); 
    free(m->pfnbuf);
    unlink("/mnt/hugetlb/mem"); 
    return 0; 
}

static struct mem_op color = {
    .type = MEM_ALLOC_COLOR,
    .priv_size = sizeof(struct mc_priv),
    .init = mc_init,
    .uinit = mc_uinit,
    .alloc_node = mc_alloc_node,
    .dealloc_node = mc_dealloc_node,
    .mm_profile = mc_profile, 
};

ADD_ALLOCATOR(color);


