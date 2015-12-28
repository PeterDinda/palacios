/* 
 * Unit test for palacios-nautilus memory allocator 
 * Can be invoked in init_palacios_nautilus_mm 
 */

#include <nautilus/printk.h> // for panic

#include "palacios-nautilus-mm.h"
#include "palacios-nautilus-mm-test.h"
#include "palacios.h"

void test_palacios_mm(unsigned num_pages_limit) 
{
    uintptr_t some_ptr;
    unsigned int i = 0;
    unsigned alignment = 4096; // gonna keep this constant for now since palacios only uses 4k pages
    num_pages_limit -= 10;
    
    /* Allocate a gigantic piece of memory at once */
    some_ptr = alloc_palacios_pgs(num_pages_limit, alignment, 0, 0, 0);
    if(!some_ptr) {
	printk("ERROR IN PALACIOS-MM TEST: returned bogus address when not supposed to\n");
	panic();
    }
    free_palacios_pgs(some_ptr, num_pages_limit);

    /* check if free_palacios_pg worked */
    some_ptr = alloc_palacios_pgs(100, alignment, 0, 0, 0);
    if(!some_ptr) {
	printk("FREE_PALACIOS_PGS DIDN'T WORK\n");
	panic();
    }

    /* Allocate many small pieces of memory consecutively */
    for(i = 0; i < num_pages_limit/100; i++) {
	free_palacios_pgs(some_ptr, 100);
	some_ptr = alloc_palacios_pgs(100, alignment, 0, 0, 0);
	if (!some_ptr) {
	    printk("ERROR IN PALACIOS-MM TEST: returned bogus address when not supposed to\n");
	    panic();
	}
    }
    
    free_palacios_pgs(some_ptr, 100);
    
    uintptr_t ptrs[num_pages_limit];

    for(i = 0; i < num_pages_limit/100; i++) {
        ptrs[i] = alloc_palacios_pgs(100, alignment, 0, 0, 0);
    }
    
    // first free random pages and then try to allocate them again 
    free_palacios_pgs(ptrs[0], 100);
    free_palacios_pgs(ptrs[3], 100);
    free_palacios_pgs(ptrs[4], 100);
    
    ptrs[0] = alloc_palacios_pgs(100, alignment, 0, 0, 0);
    ptrs[3] = alloc_palacios_pgs(100, alignment, 0, 0, 0);
    ptrs[4] = alloc_palacios_pgs(100, alignment, 0, 0, 0);
    
    for(i = 0; i < num_pages_limit/100; i++) {
        free_palacios_pgs(ptrs[i], 100);
    }

    
    // TODO: WRITE MORE TESTS
    printk("ALL TESTS PASSED - FREED ALL MEMORY\n");
}
