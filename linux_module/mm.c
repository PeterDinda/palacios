/* Palacios memory manager 
 * (c) Jack Lange, 2010
 */

#include <asm/page_64_types.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/mm.h>
//static struct list_head pools;


struct mempool {
    uintptr_t base_addr;
    u64 num_pages;

    u8 * bitmap;
};


static struct mempool pool;

static inline int get_page_bit(int index) {
    int major = index / 8;
    int minor = index % 8;

    return (pool.bitmap[major] & (0x1 << minor));
}

static inline void set_page_bit(int index) {
    int major = index / 8;
    int minor = index % 8;

    pool.bitmap[major] |= (0x1 << minor);
}

static inline void clear_page_bit(int index) {
    int major = index / 8;
    int minor = index % 8;

    pool.bitmap[major] &= ~(0x1 << minor);
}



static uintptr_t alloc_contig_pgs(u64 num_pages, u32 alignment) {
    int step = 1;
    int i = 0;
    int start = 0;

    printk("Allocating %llu pages (align=%lu)\n", 
	   num_pages, (unsigned long)alignment);

    if (pool.bitmap == NULL) {
	printk("ERROR: Attempting to allocate from non initialized memory\n");
	return 0;
    }

    if (alignment > 0) {
	step = alignment / 4096;
    }

    // Start the search at the correct alignment 
    if (pool.base_addr % alignment) {
	start = ((alignment - (pool.base_addr % alignment)) >> 12);
    }

    printk("\t Start idx %d (base_addr=%p)\n", start, (void *)(u64)pool.base_addr);

    for (i = start; i < (pool.num_pages - num_pages); i += step) {
	if (get_page_bit(i) == 0) {
	    int j = 0;
	    int collision = 0;

	    for (j = i; (j - i) < num_pages; j++) {
		if (get_page_bit(j) == 1) {
		    collision = 1;
		    break;
		}
	    }

	    if (collision == 1) {
		break;
	    }

	    for (j = i; (j - i) < num_pages; j++) {
		set_page_bit(j);
	    }

	    return pool.base_addr + (i * 4096);
	}
    }

    /* printk("PALACIOS BAD: LARGE PAGE ALLOCATION FAILED\n"); */

    return 0;
}


// alignment is in bytes
uintptr_t alloc_palacios_pgs(u64 num_pages, u32 alignment) {
    uintptr_t addr = 0; 

    if ((num_pages < 12)) {
	struct page * pgs = NULL;
	int order = get_order(num_pages * PAGE_SIZE);
	 
	pgs = alloc_pages(GFP_DMA32, order);
    
	WARN(!pgs, "Could not allocate pages\n");
 
        /* if (!pgs) { printk("PALACIOS BAD: SMALL PAGE ALLOCATION FAILED\n");  } */
       
	/* printk("%llu pages (order=%d) aquired from alloc_pages\n", 
	       num_pages, order); */

	addr = page_to_pfn(pgs) << PAGE_SHIFT; 
    } else {
	//printk("Allocating %llu pages from bitmap allocator\n", num_pages);
	//addr = pool.base_addr;
	addr = alloc_contig_pgs(num_pages, alignment);
    }


    //printk("Returning from alloc addr=%p, vaddr=%p\n", (void *)addr, __va(addr));
    return addr;
}



void free_palacios_pgs(uintptr_t pg_addr, int num_pages) {
    //printk("Freeing Memory page %p\n", (void *)pg_addr);

    if ((pg_addr >= pool.base_addr) && 
	(pg_addr < pool.base_addr + (4096 * pool.num_pages))) {
	int pg_idx = (pg_addr - pool.base_addr) / 4096;
	int i = 0;

	if ((pg_idx + num_pages) > pool.num_pages) {
	    printk("Freeing memory bounds exceeded\n");
	    return;
	}

	for (i = 0; i < num_pages; i++) {
	    WARN(get_page_bit(pg_idx + i) == 0, "Trying to free unallocated page\n");

	    clear_page_bit(pg_idx + i);
	}
    } else {
	__free_pages(pfn_to_page(pg_addr >> PAGE_SHIFT), get_order(num_pages * PAGE_SIZE));
    }
}


int add_palacios_memory(uintptr_t base_addr, u64 num_pages) {
    /* JRL: OK.... so this is horrible, terrible and if anyone else did it I would yell at them.
     * But... the fact that you can do this in C is so ridiculous that I can't help myself.
     * Note that we're repurposing "true" to be 1 here
     */

    int bitmap_size = (num_pages / 8) + ((num_pages % 8) > 0); 

    if (pool.num_pages != 0) {
	printk("ERROR: Memory has already been added\n");
	return -1;
    }

    printk("Managing %dMB of memory starting at %llu (%lluMB)\n", 
	   (unsigned int)(num_pages * 4096) / (1024 * 1024), 
	   (unsigned long long)base_addr, 
	   (unsigned long long)(base_addr / (1024 * 1024)));


    pool.bitmap = kmalloc(bitmap_size, GFP_KERNEL);
    
    if (IS_ERR(pool.bitmap)) {
	printk("Error allocating Palacios MM bitmap\n");
	return -1;
    }
    
    memset(pool.bitmap, 0, bitmap_size);

    pool.base_addr = base_addr;
    pool.num_pages = num_pages;

    return 0;
}



int palacios_init_mm( void ) {
    //    INIT_LIST_HEAD(&(pools));
    pool.base_addr = 0;
    pool.num_pages = 0;
    pool.bitmap = NULL;

    return 0;
}

int palacios_deinit_mm( void ) {
    kfree(pool.bitmap);
    
    return 0;
}
