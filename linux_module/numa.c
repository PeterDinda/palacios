/* NUMA topology 
 * (c) Jack Lange, 2013
 */

#include <linux/mm.h>

#include <interfaces/vmm_numa.h>

#include "palacios.h"





int numa_num_nodes(void) {
    return num_online_nodes();
}



int numa_addr_to_node(uintptr_t phys_addr) {
    return page_to_nid(pfn_to_page(phys_addr >> PAGE_SHIFT));
}

int numa_cpu_to_node(int cpu_id) {
    return cpu_to_node(cpu_id);
}


int numa_get_distance(int node1, int node2) {
    return node_distance(node1, node2);
}


/* Ugly fix for interface type differences... */
static int phys_ptr_to_node(void * phys_ptr) {
    return numa_addr_to_node((uintptr_t)phys_ptr);
}

struct v3_numa_hooks numa_hooks = {
    .cpu_to_node = numa_cpu_to_node,
    .phys_addr_to_node = phys_ptr_to_node,
    .get_distance = numa_get_distance,
};


int palacios_init_numa( void ) {
 
    V3_Init_NUMA(&numa_hooks);

    INFO("palacios numa interface initialized\n");
  
    return 0;
}

int palacios_deinit_numa(void) {
    INFO("palacios numa interface deinitialized\n");
    return 0;
}
