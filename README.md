prefix_lookup_mc
================

*A high performance IP lookup engine based on the Tree Bitmap Algorithm.

	paper: Hardware/Software IP Lookups with Incremental Updates.
	link: http://cseweb.ucsd.edu/~varghese/PAPERS/ccr2004.pdf

*It can be used for both IPv4 and IPv6 lookup 

Performance: 5.3Gbps for low locality IP traces on a single Intel Xeon Core

*It support Incremental Update

Support dynamic network environment


*Example:

    ....
    struct lookup_trie trie;
    memset(&trie, 0, sizeof(struct lookup_trie));
    init_lookup_trie(&trie);
    
    ...
    insert_prefix(&trie, ip, cidr, (struct next_hop_info*)(key));
    ...
    delete_prefix(trie, ip, cidr, NULL);
    ....
    destory_trie(trie, NULL);
    
    
Check function ipv4_test() and ipv6_test in main.c for details. 


*Performance

Test on Intel E5506.

     2.3GHz 256K L1 Cache, 1M L2 Cache, 4M L3 Cache.
     127ns/lookup for 1M low locality IP traces.
     7Mpps for 64 byte packets.
     5Gbps for 64 Byte packets.
     
Memory Size: 

	 16MB on 64-bit platform for 350K core router prefixes.
     5.4 MB used for lookup data structure.
     11.6MB used for an aid data structure for update.
     
     
