#include "palacios-nautilus-mm.h"
#include "palacios-nautilus-mm-test.h"

/*

  Page-granularity memory management

  This impedence-matches between Nautilus's singular allocator (malloc/free)
  and page-level memory allocation needed in Palacios.   It does so via 
  a last-fit-optimized bitmap allocator that operates over a large pool 
  allocated from Nautilus at startup.

  Note that this allocation currently ignores NUMA and other constraints
  as well as general filter expressions.   

*/

static uint64_t get_order(uint64_t n)
{
    uint64_t top_bit_pos;

    top_bit_pos = 63 - __builtin_clz(n);

    return top_bit_pos + !!(n & ~(1<<top_bit_pos));

}

struct mempool {
    u8 * bitmap;
    u64 num_pages;
    u64 num_free_pages;
    u64 pool_start;
    u64 base_addr;
    u64 alloc_base_addr;
};

static struct mempool pool;


static inline int get_page_bit(int index) {
    int major = index / 8;
    int minor = index % 8;
    return pool.bitmap[major] & (0x1 << minor);
}

static inline void clear_page_bit(int index) {
    int major = index / 8;
    int minor = index % 8;
    pool.bitmap[major] &= ~(0x1 << minor);
    ++pool.num_free_pages;
}

static inline void set_page_bit(int index) {
    int major = index / 8;
    int minor = index % 8;
    pool.bitmap[major] |= (0x1 << minor);
    --pool.num_free_pages;
}



int init_palacios_nautilus_mm(uint64_t memsize) {

    INFO("Palacios MM: init\n");
    INFO("Palacios MM: Nautilus page size is %d\n", PAGE_SIZE);
    unsigned num_nk_pages = memsize / PAGE_SIZE;
    unsigned num_palacios_pages = memsize / PALACIOS_PAGE_SIZE;

    INFO("Palacios MM: Initializing with %u 4K pages (%u pages on Nautilus)\n", num_palacios_pages, num_nk_pages);

    unsigned bitmap_size = (num_palacios_pages / 8) + ((num_palacios_pages % 8) > 0);
    pool.bitmap = palacios_alloc(bitmap_size);

    if (!pool.bitmap) { 
	ERROR("Palacios MM: Failed to allocate bitmap\n");
	return -1;
    }
    // note that this may not be aligned
    pool.alloc_base_addr = (u64) palacios_alloc(PAGE_SIZE * num_nk_pages);
    
    if (!pool.alloc_base_addr) { 
	ERROR("Palacios MM: FAILED TO ALLOCATE MEMORY\n");
	return -1;
    } else {
	INFO("Palacios MM: success, alloc_base_addr=%p\n",pool.alloc_base_addr);
    }
    
    // Align our memory to a page boundary
    pool.base_addr = (u64) (((uint64_t)pool.alloc_base_addr & (~0xfffULL)) + PALACIOS_PAGE_SIZE);
    
    INFO("Palacios MM: success, cleaned up base_addr=%p\n",pool.base_addr);

    // We have one fewer pages than requested due to the need to align
    // the result of the malloc
    pool.num_pages = num_palacios_pages - 1 ;
    pool.num_free_pages = num_palacios_pages - 1;
    pool.pool_start = 0;

    // do unit test if desired
    //test_palacios_mm(num_palacios_pages);

    return 0;

}


int deinit_palacios_nautilus_mm(void) {
    // free pages from nk
    free((void*)pool.alloc_base_addr); pool.alloc_base_addr = 0;
    free((void*)pool.bitmap); pool.bitmap = 0;
    
    return 0;
}

static uintptr_t alloc_contig_pgs(u64 num_pages, u32 alignment) 
{
    
    int step = 1;
    int i = 0;
    int j = 0;
    
    if (num_pages > pool.num_free_pages) {
	ERROR("ERROR(PALACIOS MM) : NOT ENOUGH MEMORY\n");
	return 0;
    }

    //INFO("Allocating %llu pages (align=%lu)\n", num_pages, (unsigned long)alignment);

    if (!pool.bitmap || !pool.base_addr) {
        ERROR("ERROR: Attempting to allocate from uninitialized memory pool \n");
        return 0;
    }

    if (alignment > 0) {
	if (alignment != 4096) { 
	    ERROR("ERROR: cannot handle alignment that is not 4KB\n");
	    return 0;
	}
        step = alignment / 4096;
    }

    // scan pages from last search forward
    for (i = pool.pool_start; i < (pool.num_pages - num_pages + 1) ; ) {

	for (j = i; j < (i+num_pages); j++) {
	    if (get_page_bit(j)) {
		break;
	    }
	 }
	 
	if (j==(i+num_pages)) {
	    for (j = i; j<(i+num_pages); j++) {
		set_page_bit(j);
	    }
	    
	    pool.pool_start = j % pool.num_pages;
	    
	    return  (void*) (pool.base_addr + (i * 4096));
	    
	}  else {
	    i = j+1;
	}
    }
    

    // scan from front if we didn't find it
    for (i = 0;  i < (pool.num_pages - num_pages + 1) ; ) {

	for (j = i; j < (i+num_pages); j++) {
	    if (get_page_bit(j)) {
		break;
	    }
	 }
	 
	if (j==(i+num_pages)) {
	    for (j = i; j<(i+num_pages); j++) {
		set_page_bit(j);
	    }
	    
	    pool.pool_start = j % pool.num_pages;
	    
	    return (void*)( pool.base_addr + (i * 4096));
	    
	}  else {
	    i = j+1;
	}
    }


    ERROR("Palacios MM: ERROR! Cannot allocate memory...\n");
    ERROR("Palacios MM: Pool has %d pages, trying to allocate %d pages\n", pool.num_pages, num_pages);

    return 0;
}

uintptr_t alloc_palacios_pgs(u64 num_pages, u32 alignment, int node_id, int (*filter_func)(void *paddr, void *filter_state), void *filter_state)
{
    uintptr_t addr = 0;
    addr = alloc_contig_pgs(num_pages, alignment);
    return addr;
}


void free_palacios_pgs(uintptr_t pg_addr, u64 num_pages)
{
    int pg_idx = ((u64)pg_addr - pool.base_addr) / PALACIOS_PAGE_SIZE;
    int i = 0;
    for (i = pg_idx; i < pg_idx+num_pages; i++) {
        clear_page_bit(i);
    }
}


void free_palacios_pg(uintptr_t pg_addr)
{
    free_palacios_pgs(pg_addr, 1);
}
