/* Copyright (c) 2007, Sandia National Laboratories */

#ifndef _LWK_BUDDY_H
#define _LWK_BUDDY_H

#include <linux/list.h>
#include <linux/rbtree.h>



struct buddy_memzone {
    unsigned long    max_order;    /** max size of memory pool = 2^max_order */
    unsigned long    min_order;    /** minimum allocatable block size */


    struct list_head *avail;       /** one free list for each block size,
				    * indexed by block order:
				    *   avail[i] = free list of 2^i blocks
				    */

    spinlock_t lock;               /** For now we will lock all zone operations...
				    *   Hopefully this does not cause a performance issue 
				    */

    unsigned int node_id;         /** The NUMA node this zone allocates from 
				   */

    
    unsigned long num_pools;
    struct rb_root mempools;
};

/**
 * This structure stores the state of a buddy system memory allocator object.
 */
struct buddy_mempool {
    struct buddy_memzone * zone; 

    unsigned long    base_addr;    /** physical base address of the memory pool */


    unsigned long    pool_order;   /** Size of this memory pool = 2^pool_order */

    unsigned long    num_blocks;   /** number of bits in tag_bits */
    unsigned long    *tag_bits;    /** one bit for each 2^min_order block
				    *   0 = block is allocated
				    *   1 = block is available
				    */

    unsigned long num_free_blocks;

    struct rb_node tree_node;
};


/**
 * Each free block has one of these structures at its head. The link member
 * provides linkage for the mp->avail[order] free list, where order is the
 * size of the free block.
 */
struct block {
    struct buddy_mempool * mp;
    struct list_head link;
    unsigned long order;
};


extern struct buddy_memzone *
buddy_init(unsigned long pool_order,
	   unsigned long min_order,
	   unsigned int node_id);

extern void 
buddy_deinit(struct buddy_memzone * zone);

/* Add pool at given physical address */
extern int 
buddy_add_pool(struct buddy_memzone * zone, 
	       unsigned long base_addr, 
	       unsigned long pool_order);
			  

/* Remove pool based at given physical address */
extern int
buddy_remove_pool(struct buddy_memzone * zone, 
		  unsigned long base_addr, 
		  unsigned char force);


/* Allocate pages, returns physical address */
extern uintptr_t 
buddy_alloc(struct buddy_memzone * zone,
	    unsigned long order);


/* Free a physical address */
extern void
buddy_free(struct buddy_memzone * zone,
	   uintptr_t  addr,
	   unsigned long order);


extern void
buddy_dump_memzone(struct buddy_memzone * zone);

#endif
