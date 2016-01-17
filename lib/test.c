#ifdef TEST_CHECK
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <arpa/inet.h>
#include "bitmap_v4.h"
#include "bitmap_v6.h"

void print_nhi(void *nhi)
{
    uint64_t key = (uint64_t)nhi;
    printf("%lu", key);
}

int del_routes(struct mb_node *root, struct mm *m, FILE *fp)
{
    int i = 0;
    char *line = NULL;
    ssize_t read = 0;
    size_t len = 0;
    
    uint32_t ip = 0;
    uint32_t org_ip = 0;
    uint32_t cidr;
    char buf[128];

    uint32_t key = 1;
    rewind(fp);

    while((read = getline(&line, &len, fp)) != -1){
        if (i & 0x01) {

            //if (i ==111433){
            //    printf("here\n");
            //}
            cidr = atoi(line);
            org_ip = ip;
            ip = ip & (0xffffffff << (32-cidr));
            //if (ip == 0x7a99ff00){
            //    printf("here\n");
            //}
            //insert_prefix(ip,cidr,(struct next_hop_info*)(key));
            //if (i == 107081) {
            //    printf("here\n");
            //}
            bitmap_delete_prefix(root, m, ip, cidr, NULL);
            
            void *a = bitmap_do_search(root, org_ip);
            key = (uint32_t)(uint64_t)a;
            if (key != 0) {
                //printf("line %s, cidr %d key %d, org_ip %x\n", buf, cidr, key,org_ip );
            }
            if (bitmap_prefix_exist(root, ip, cidr)){
                printf("prefix exist ! error\n");
            }


            //hash_trie_insert(ip,cidr,(struct next_hop_info*)(key));

            //if (i ==111433){
            //    //printf("here\n");
	    //    break;
            //}
            //key ++;
        }
        else {
            //printf("line %s", line);
            ip = inet_network(line);
            strcpy(buf, line);
        }
        
        i++;
        //if (i == 8179) {
        //    printf("here\n");
        //}

    }
    printf("del routes %d\n", i/2 );
    rewind(fp);
    
    while((read = getline(&line, &len, fp)) != -1){
        if (i & 0x01) {

            //if (i ==111433){
            //    printf("here\n");
            //}
            cidr = atoi(line);
            org_ip = ip;
            ip = ip & (0xffffffff << (32-cidr));
            //if (ip == 0x7a99ff00){
            //    printf("here\n");
            //}
            //insert_prefix(ip,cidr,(struct next_hop_info*)(key));
            //if (org_ip == 0x77c25d00) {
            //    printf("here\n");
            //}
            //delete_prefix(ip, cidr);
            
            void *a = bitmap_do_search(root, org_ip);
            key = (uint32_t)(uint64_t)a;
            if (key != 0) {
                printf("line %s, cidr %d key %d, org_ip %x\n", buf, cidr, key,org_ip );
            }
            //hash_trie_insert(ip,cidr,(struct next_hop_info*)(key));

            //if (i ==111433){
            //    //printf("here\n");
	    //    break;
            //}
            //key ++;
        }
        else {
            //printf("line %s", line);
            ip = inet_network(line);
            strcpy(buf, line);
        }
        
        i++;
        //if (i == 8179) {
        //    printf("here\n");
        //}

    }

    return i/2 ;

}

int load_routes(struct mb_node *root, struct mm *m, FILE *fp)
{
    int i = 0;
    char *line = NULL;
    ssize_t read = 0;
    size_t len = 0;
    
    uint32_t ip = 0;
    uint32_t cidr;
    uint64_t key = 1;

    while((read = getline(&line, &len, fp)) != -1){
        if (i & 0x01) {

            //if (i ==111433){
            //    printf("here\n");
            //}
            cidr = atoi(line);
            ip = ip & (0xffffffff << (32-cidr));
            //if (key == 4835){
            //    printf("here\n");
            //}
            bitmap_insert_prefix(root, m, ip, cidr,(void*)(key));
            //hash_trie_insert(ip,cidr,(struct next_hop_info*)(key));

            //if (i ==111433){
            //    //printf("here\n");
	    //    break;
            //}
            key ++;
        }
        else {
            //printf("line %s", line);
            ip = inet_network(line);
        }
        
        i++;
        //if (i == 8179) {
        //    printf("here\n");
        //}

    }
    printf("load routes %d\n", i/2 );
    return i/2 ;
}

void test_lookup_valid(struct mb_node *root) 
{
    FILE *fp = fopen("ret_5","r");
    if (fp == NULL)
        exit(-1);

    int i = 0;

    char *line = NULL;
    ssize_t read = 0;
    size_t len = 0;

    uint32_t *ips = (uint32_t *)malloc(1000000 * sizeof(uint32_t));
    uint32_t ip;
    uint32_t key = 1;

    while((read = getline(&line, &len, fp)) != -1){
        if (i & 0x01) {
        }
        else {
            //printf("line %s", line);
            ip = inet_network(line);
            ips[i/2] = ip;
        }
        i++;
    }
    int ip_cnt = i/2;


    int cnt=0;
    for (i = 0; i< ip_cnt ;i++){

        //if (ips[i] == 0x7a99ff00) {
        //    printf("here\n");
        //}
        //struct next_hop_info *a = hash_trie_search(ips[i]);
        void *a = bitmap_do_search_lazy(root, ips[i]);
        uint32_t b = (uint32_t)(uint64_t)a;
        if ( b == key ) {
            cnt ++;
        }
        else {
            //struct in_addr addr;
            //addr.s_addr = htonl(array[i].test_ip);


            printf("search(0x%x); result %d; i %d\n", ips[i], b ,i);
            //printf("the truth is ip_test %s  key %d ip %x\n", inet_ntoa(addr),array[i].key*2, array[i].ip);
        }
        //printf("search(0x%x);\n", array[i].ip);

        //printf("search(0x%x); reulst %x\n", array[i].ip, b);
        key ++;
    }

    printf("match %d\n", cnt);
    fclose(fp);
    free(ips);
    //mem_usage(&trie->mm);
}


void ipv4_test()
{
    //FILE *fp = fopen("rrc00(2013080808).txt.port","r");
    FILE *fp = fopen("ret_5","r");
    if (fp == NULL)
        exit(-1);
    
    struct mb_node root = {0,0,NULL};
    struct mm m;
    memset(&m, 0, sizeof(m));
    mm_init(&m, MEM_ALLOC_SIMPLE);

    printf("ipv4 test\n");

    load_routes(&root, &m, fp);
    //load_fib(&trie, fp);

    test_lookup_valid(&root);
    //mem_alloc_stat_v6();

    //rewind(fp);

    //bitmap_print_all_prefix(&root, print_nhi);
    mm_profile(&m);
    del_routes(&root, &m, fp);
    //test_random_ips(&trie);
    bitmap_destroy_trie(&root, &m, NULL);
    //mc_profile(&trie.mm);
    
    printf("\n");

}

void test_lookup_valid_v6(struct mb_node *root, FILE *fp)
{
    struct ip_v6 ip;
    uint32_t i=1;
    
    char *line = NULL;
    ssize_t read = 0;
    size_t len = 0;

    int r;
    int match = 0;


    struct in6_addr addr;
    
    while((read = getline(&line, &len, fp)) != -1){
        line[strlen(line) - 1] = '\0';
        r = inet_pton(AF_INET6, line, (void *)&addr);
        if ( r == 0 ){
            printf("wrong format\n");
            continue;
        }
        hton_ipv6(&addr);
        memcpy(&ip, &addr, sizeof(addr));

        void *ret = bitmapv6_do_search(root, ip);
        uint64_t key = (uint64_t)ret;
        if ( key != i) {
            printf("overlapped or error %s key %d ret %lu\n", line, i, key);
        }
        else {
            match++;
        }
        i++;
    }

    printf("test %d, match %d\n", i -1 , match);
}

int del_routes_v6(struct mb_node *root, struct mm *m, FILE *fp)
{
    struct ip_v6 ip;
    uint32_t cidr;
    char *slash;
    uint32_t i=1;

    char *line = NULL;
    ssize_t read = 0;
    size_t len = 0;
    int r;
   

    char ip_v6[INET6_ADDRSTRLEN]; 
    char prelen[4];

    struct in6_addr addr;
    
    while((read = getline(&line, &len, fp)) != -1){
        slash = strchr(line, '/');
        if(slash == NULL) {
            printf("wrong format line\n");
            continue;
        }

        memcpy(ip_v6, line, slash - line);
        ip_v6[slash - line] ='\0';

        r = inet_pton(AF_INET6, ip_v6, (void *)&addr);
        if ( r == 0 ){
            printf("wrong format\n");
            continue;
        }
        hton_ipv6(&addr);
        memcpy(&ip, &addr, sizeof(addr));

        memcpy(prelen, slash + 1, strlen(line) - (slash - line) -1);
        prelen[strlen(line) - (slash - line) - 1] ='\0';
        cidr = atoi(prelen);

        void *ret = bitmapv6_do_search(root, ip);
        uint64_t key = (uint64_t)ret;
        if ( key != i) {
            //printf("overlapped or error %s key %d ret %d\n", line, i, key);
        }

        r = bitmapv6_prefix_exist(root, ip, cidr);
        if( r != 1) {
            printf("prefix_exist error\n");
            printf("IP v6 addr: %d %s\n", i, line);
        }
        
        bitmapv6_delete_prefix(root, m, ip, cidr, NULL); 
        r = bitmapv6_prefix_exist(root, ip, cidr);
        if ( r == 1) {
            printf("prefix_exist error\n");
        }

        i++;
    }

    return 0;    
}



int load_routes_v6(struct mb_node *node, struct mm *m, FILE *fp)
{
    int i = 0;
    char *line = NULL;
    ssize_t read = 0;
    size_t len = 0;
   
    struct ip_v6 ip;
    uint32_t cidr;
    uint64_t key = 1;
    char *slash;
    

    char ip_v6[INET6_ADDRSTRLEN]; 
    char prelen[4];

    struct in6_addr addr;
    int ret;
    
    while((read = getline(&line, &len, fp)) != -1){
        slash = strchr(line, '/');
        if(slash == NULL) {
            printf("wrong format line\n");
            continue;
        }

        memcpy(ip_v6, line, slash - line);
        ip_v6[slash - line] ='\0';

        ret = inet_pton(AF_INET6, ip_v6, (void *)&addr);
        if (ret == 0) {
            printf("transform fail\n");
            continue;
        }
        hton_ipv6(&addr);
        memcpy(&ip, &addr, sizeof(addr));

        memcpy(prelen, slash + 1, strlen(line) - (slash - line) -1);
        prelen[strlen(line) - (slash - line) - 1] ='\0';
        cidr = atoi(prelen);
        
        bitmapv6_insert_prefix(node, m, ip, cidr, (void *)key);
        key ++;
        i++;
    }
    printf("load routes %d\n", i);
    return i ;
}

void ipv6_test()
{
    FILE *fp = fopen("ipv6_fib","r");
    if (fp == NULL)
        exit(-1);
    
    struct mb_node root = {0,0,NULL};
    struct mm m;
    memset(&m, 0, sizeof(m));
    mm_init(&m, MEM_ALLOC_SIMPLE);
    printf("ipv6 test\n");

    load_routes_v6(&root, &m, fp);
    bitmapv6_print_all_prefix(&root, print_nhi);
    mm_profile(&m);

    rewind(fp);
    del_routes_v6(&root, &m, fp);

    bitmapv6_destroy_trie(&root, &m, NULL);
    mm_profile(&m);

    printf("\n");

}

int main()
{
    ipv4_test();
    //ipv6_test();
    return 0;
}

#endif
