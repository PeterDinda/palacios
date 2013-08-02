/* Palacios memory manager 
 * (c) Jack Lange, 2010
 */

#include <asm/page_64_types.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/mm.h>
//static struct list_head pools;

#include "palacios.h"
#include "mm.h"
#include "buddy.h"
#include "numa.h"


static struct buddy_memzone ** memzones = NULL;
static uintptr_t * seed_addrs = NULL;


// alignment is in bytes
uintptr_t alloc_palacios_pgs(u64 num_pages, u32 alignment, int node_id) {
    uintptr_t addr = 0; 

    if (node_id == -1) {
	int cpu_id = get_cpu();
	put_cpu();
	
	node_id = numa_cpu_to_node(cpu_id);
    } else if (numa_num_nodes() == 1) {
	node_id = 0;
    } else if (node_id >= numa_num_nodes()) {
	ERROR("Requesting memory from an invalid NUMA node. (Node: %d) (%d nodes on system)\n", 
	      node_id, numa_num_nodes());
	return 0;
    }

    addr = buddy_alloc(memzones[node_id], get_order(num_pages * PAGE_SIZE) + PAGE_SHIFT);

    //DEBUG("Returning from alloc addr=%p, vaddr=%p\n", (void *)addr, __va(addr));
    return addr;
}



void free_palacios_pgs(uintptr_t pg_addr, u64 num_pages) {
    int node_id = numa_addr_to_node(pg_addr);

    //DEBUG("Freeing Memory page %p\n", (void *)pg_addr);
    buddy_free(memzones[node_id], pg_addr, get_order(num_pages * PAGE_SIZE) + PAGE_SHIFT);
    
    return;
}


int pow2(int i)
{
    int x=1;
    for (;i>0;i--) { x*=2; } 
    return x;
}

int add_palacios_memory(struct v3_mem_region *r) {
    int pool_order = 0;
    int node_id = 0;

    struct v3_mem_region *keep;

    // fixup request regardless of its type
    if (r->num_pages*4096 < V3_CONFIG_MEM_BLOCK_SIZE) { 
	WARNING("Allocating a memory pool smaller than the Palacios block size - may not be useful\n");
    }

    if (pow2(get_order(r->num_pages)) != r->num_pages) { 
	WARNING("Allocating a memory pool that is not a power of two - it will be rounded down!\n");
	r->num_pages=pow2(get_order(r->num_pages));
    }


    if (!(keep=palacios_alloc(sizeof(struct v3_mem_region)))) { 
	ERROR("Error allocating space for tracking region\n");
	return -1;
    }


    if (r->type==REQUESTED || r->type==REQUESTED32) { 
	struct page * pgs = alloc_pages_node(r->node, 
					     r->type==REQUESTED ? GFP_KERNEL :
					     r->type==REQUESTED32 ? GFP_DMA32 : GFP_KERNEL, 
					     get_order(r->num_pages));
	if (!pgs) { 
	    ERROR("Unable to satisfy allocation request\n");
	    palacios_free(keep);
	    return -1;
	}
	r->base_addr = page_to_pfn(pgs) << PAGE_SHIFT;
    }
	

    *keep = *r;

    node_id = numa_addr_to_node(r->base_addr);

    if (node_id == -1) {
	ERROR("Error locating node for addr %p\n", (void *)(r->base_addr));
	return -1;
    }

    pool_order = get_order(r->num_pages * PAGE_SIZE) + PAGE_SHIFT;

    if (buddy_add_pool(memzones[node_id], r->base_addr, pool_order, keep)) {
	ERROR("ALERT ALERT ALERT Unable to add pool to buddy allocator...\n");
	if (r->type==REQUESTED || r->type==REQUESTED32) { 
	    free_pages((uintptr_t)__va(r->base_addr), get_order(r->num_pages));
	}
	palacios_free(keep);
	return -1;
    }

    return 0;
}



int palacios_remove_memory(uintptr_t base_addr) {
    int node_id = numa_addr_to_node(base_addr);
    struct v3_mem_region *r;

    if (buddy_remove_pool(memzones[node_id], base_addr, 0, (void**)(&r))) { //unforced remove
	ERROR("Cannot remove memory at base address 0x%p because it is in use\n", (void*)base_addr);
	return -1;
    }

    if (r->type==REQUESTED || r->type==REQUESTED32) { 
	free_pages((uintptr_t)__va(r->base_addr), get_order(r->num_pages));
    } else {
	// user space resposible for onlining
    }
    
    palacios_free(r);

    return 0;
}



int palacios_deinit_mm( void ) {

    int i = 0;

    if (memzones) {
	for (i = 0; i < numa_num_nodes(); i++) {
	    
	    if (memzones[i]) {
		buddy_deinit(memzones[i]);
	    }
	    
	    // note that the memory is not onlined here - offlining and onlining
	    // is the resposibility of the caller
	    
	    if (seed_addrs[i]) {
		// free the seed regions
		free_pages((uintptr_t)__va(seed_addrs[i]), MAX_ORDER - 1);
	    }
	}
	
	palacios_free(memzones);
	palacios_free(seed_addrs);
    }

    return 0;
}

int palacios_init_mm( void ) {
    int num_nodes = numa_num_nodes();
    int node_id = 0;

    memzones = palacios_alloc_extended(sizeof(struct buddy_memzone *) * num_nodes, GFP_KERNEL);

    if (!memzones) { 
	ERROR("Cannot allocate space for memory zones\n");
	palacios_deinit_mm();
	return -1;
    }

    memset(memzones, 0, sizeof(struct buddy_memzone *) * num_nodes);

    seed_addrs = palacios_alloc_extended(sizeof(uintptr_t) * num_nodes, GFP_KERNEL);

    if (!seed_addrs) { 
	ERROR("Cannot allocate space for seed addrs\n");
	palacios_deinit_mm();
	return -1;
    }

    memset(seed_addrs, 0, sizeof(uintptr_t) * num_nodes);

    for (node_id = 0; node_id < num_nodes; node_id++) {
	struct buddy_memzone * zone = NULL;

	// Seed the allocator with a small set of pages to allow initialization to complete. 
	// For now we will just grab some random pages, but in the future we will need to grab NUMA specific regions
	// See: alloc_pages_node()

	{
	    struct page * pgs = alloc_pages_node(node_id, GFP_KERNEL, MAX_ORDER - 1);

	    if (!pgs) {
		ERROR("Could not allocate initial memory block for node %d\n", node_id);
		BUG_ON(!pgs);
		palacios_deinit_mm();
		return -1;
	    }

	    seed_addrs[node_id] = page_to_pfn(pgs) << PAGE_SHIFT;
	}

	zone = buddy_init(get_order(V3_CONFIG_MEM_BLOCK_SIZE) + PAGE_SHIFT, PAGE_SHIFT, node_id);

	if (zone == NULL) {
	    ERROR("Could not initialization memory management for node %d\n", node_id);
	    palacios_deinit_mm();
	    return -1;
	}

	printk("Zone initialized, Adding seed region (order=%d)\n", 
	       (MAX_ORDER - 1) + PAGE_SHIFT);

	if (buddy_add_pool(zone, seed_addrs[node_id], (MAX_ORDER - 1) + PAGE_SHIFT,0)) { 
	    ERROR("Could not add pool to buddy allocator\n");
	    palacios_deinit_mm();
	    return -1;
	}

	memzones[node_id] = zone;
    }

    return 0;

}

