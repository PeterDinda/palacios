/* NUMA topology 
 * (c) Jack Lange, 2013
 */


#ifndef __NUMA_H__
#define __NUMA_H__

int palacios_init_numa( void );
int palacios_deinit_numa(void);

int numa_num_nodes(void );
int numa_cpu_to_node(int cpu_id);
int numa_addr_to_node(uintptr_t phys_addr);
int numa_get_distance(int node1, int node2);



#endif

