/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2013, Jack Lange <jacklange@cs.pitt.edu> 
 * Copyright (c) 2013, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jacklange@cs.pitt.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */
 


#ifndef __VMM_NUMA_INTERFACE_H_
#define __VMM_NUMA_INTERFACE_H_


#ifdef __V3VEE__

#include <palacios/vmm_types.h>
#include <palacios/vm_guest.h>

int v3_numa_gpa_to_node(struct v3_vm_info * vm, addr_t gpa);
int v3_numa_hpa_to_node(addr_t hpa);
int v3_numa_cpu_to_node(uint32_t cpu);
int v3_numa_get_distance(uint32_t node1, uint32_t node2);

#endif


struct v3_numa_hooks {
    int (*cpu_to_node)(int phys_cpu_id);
    int (*phys_addr_to_node)(void * addr);
    int (*get_distance)(int node1, int node2);
};



extern void V3_Init_NUMA(struct v3_numa_hooks * hooks);


#endif
