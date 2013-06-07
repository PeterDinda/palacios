/* NUMA topology 
 * (c) Jack Lange, 2013
 */

#include <linux/mm.h>

#include "palacios.h"
#include <interfaces/vmm_numa.h>



#if 0
struct mem_region {
    u64 start_addr;
    u64 end_addr;
    u32 node_id;
};


static struct {
    unsigned int num_nodes;
    unsigned int num_cpus;
    unsigned int num_mem_regions;

    u32 * cpu_to_node_map;

    // For now an array, but we might want to move this to an rbtree
    struct mem_region * mem_to_node_map;
    
    u32 * distance_table;
} topology}


int create_numa_topology_from_user(void __user * argp) {
    struct v3_numa_topo user_topo;

    if (copy_from_user(&user_topo, argp, sizeof(struct v3_numa_topo))) {
	ERROR("Could not read in NUMA topology from user\n");
	return -1;
    }
	
    argp += sizeof(struct v3_numa_topo);

    topology.num_nodes = user_topo.num_nodes;
    topology.num_cpus = user_topo.num_cpus;
    topology.num_mem_regions = user_topo.num_mem_regions;


    /* Read in the CPU to Node mapping */
    {
	topology.cpu_to_node_map = kmalloc(GFP_KERNEL, 
					   sizeof(u32) * 
					   topology.num_cpus);
	
	if (IS_ERR(topology.cpu_to_node_map)) {
	    ERROR("Could  not allocate cpu to node map\n");
	    return -1;
	}
	

	if (copy_from_user(topology.cpu_to_node_map, argp,
			   sizeof(u32) * topology.num_cpus)) {
	    ERROR("Could not copy cpu to node map from user space\n");
	    kfree(topology.cpu_to_node_map);
	    return -1;
	}
	
	argp += sizeof(u32) * topology.num_cpus;
    }

    /* Read in the memory to Node Mapping */
    {
	int i = 0;

	topology.mem_to_node_map = kmalloc(GFP_KERNEL, 
					   sizeof(struct mem_region) * 
					   topology.num_mem_regions);

	if (IS_ERR(topology.mem_to_node_map)) {
	    ERROR("Could not allocate mem to node map\n");
	    kfree(topology.cpu_to_node_map);
	    return -1;
	}

	if (copy_from_user(topology.mem_to_node_map, argp,
			   sizeof(struct mem_region) * topology.num_mem_regions)) {
	    ERROR("Coudl not copy mem to node map from user space\n");
	    kfree(topology.cpu_to_node_map);
	    kfree(topology.mem_to_node_map);
	    return -1;
	}

	/* The memory range comes in as the base_addr and the number of pages
	   We need to fix it up to be the base addr and end addr instead 
	   We just perform the transformation inline
	*/
	for (i = 0; i < topology.num_mem_regions; i++) {
	    struct mem_region * region = &(topology.mem_to_node_map[i]);

	    region->end_addr = region->base_addr + (region->end_addr * 4096);
	}

    }

    
    /* Read in the distance table */
    {
	topology.distance_table = kmalloc(GFP_KERNEL,
					  sizeof(u32) * 
					  (topology.num_nodes * topology.num_nodes));
	
	if (IS_ERR(topology.distance_table)) {
	    ERROR("Could not allocate distance table\n");
	    kfree(topology.cpu_to_node_map);
	    kfree(topology.mem_to_node_map);
	    return -1;
	}
	

	if (copy_from_user(topology.distance_table, argp,
			   sizeof(u32) * (topology.num_nodes * topology.num_nodes))) {
	    ERROR("Could not copy distance table from user space\n");
	    kfree(topology.cpu_to_node_map);
	    kfree(topology.mem_to_node_map);
	    kfree(topology.distance_table);
	    return -1;
	}

    }
    
    /* Report what we found */
    {
	int i = 0;
	int j = 0;

	printk("Created NUMA topology from user space\n");
	printk("Number of Nodes: %d, CPUs: %d, MEM regions: %d\n", 
	       topology.num_nodes, topology.num_cpus, topology.num_mem_regions);

	printk("CPU mapping\n");
	for (i = 0; i < topology.num_cpus; i++) {
	    printk("\tCPU %d -> Node %d\n", i, topology.cpu_to_node_map[i]);
	}

	printk("Memory mapping\n");

	for (i = 0; i < topology.num_mem_regions; i++) {
	    struct mem_region * region = &(topology.mem_to_node_map[i]);
	    printk("\tMEM %p - %p -> Node %d\n", 
		   region->start_addr, 
		   region->end_addr, 
		   region->node_id);
	}


	printk("Distance Table\n");
	for (i = 0; i < topology.num_nodes; i++) {
	    printk("\t%d", i);
	}
	printk("\n");
	
	for (i = 0; i < topology.num_nodes; i++) {
	    printk("%d", i);

	    for (j = 0; j < topology.num_nodes; j++) {
		printk("\t%d", topology.distance_table[j + (i * topology.num_nodes)]);
	    }

	    printk("\n");

	}	    


    }
    return 0;

}


#endif



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
  
    return 0;
}
