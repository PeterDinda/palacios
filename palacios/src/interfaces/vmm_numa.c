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
 

#include <palacios/vmm.h>
#include <palacios/vm_guest.h>
#include <interfaces/vmm_numa.h>
#include <palacios/vm_guest_mem.h>


static struct v3_numa_hooks * numa_hooks = NULL;

void V3_Init_NUMA(struct v3_numa_hooks * hooks) {
    numa_hooks = hooks;
    V3_Print("V3 NUMA interface initialized\n");
    return;
}

int v3_numa_hpa_to_node(addr_t hpa) {
    if (!numa_hooks) {
	return 0;
    }

    return numa_hooks->phys_addr_to_node((void *)hpa);
}


int v3_numa_gpa_to_node(struct v3_vm_info * vm, addr_t gpa) {
    addr_t hpa = 0;

    if (!numa_hooks) {
	return 0;
    }

    if (v3_gpa_to_hpa(&(vm->cores[0]), gpa, &hpa) == -1) {
	PrintError("Tried to find NUMA node for invalid GPA (%p)\n", (void *)gpa);
	return -1;
    }
    
    return numa_hooks->phys_addr_to_node((void *)hpa);
    
}

int v3_numa_cpu_to_node(uint32_t cpu) {

    if (!numa_hooks){
	return 0;
    }
    
    return numa_hooks->cpu_to_node(cpu);
}

int v3_numa_get_distance(uint32_t node1, uint32_t node2) {

    if (!numa_hooks) {
	return 0;
    }


    return numa_hooks->get_distance(node1, node2);
}
