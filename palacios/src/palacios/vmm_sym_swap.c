/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */
#include <palacios/vmm.h>


#include <palacios/vmm_sym_swap.h>

// this is hardcoded in linux, but we should expose it via a sym interface
#define SWAP_DEV_SHIFT 5 

int v3_init_sym_swap(struct guest_info * info) {

    memset(&(info->swap_state), 0, sizeof(struct v3_sym_swap_state));

    return 0;
}


int v3_register_swap_disk(struct guest_info * info, int dev_index, 
			  struct v3_swap_ops * ops, void * private_data) {
    struct v3_sym_swap_state * swap_state = &(info->swap_state);

    swap_state->devs[dev_index].present = 1;
    swap_state->devs[dev_index].private_data = private_data;
    swap_state->devs[dev_index].ops = ops;

    return 0;
}


addr_t v3_get_swapped_pg_addr(struct guest_info * info, pte32_t * pte) {
    struct v3_sym_swap_state * swap_state = &(info->swap_state);
    uint32_t dev_index = *(uint32_t *)pte & ((1 << SWAP_DEV_SHIFT) - 1);
    uint32_t pg_index = (*(uint32_t *)pte) >> SWAP_DEV_SHIFT;
    struct v3_swap_dev * swp_dev = &(swap_state->devs[dev_index]);

    return (addr_t)swp_dev->ops->get_swap_entry(pg_index, swp_dev->private_data);
}



int v3_swap_out_notify(struct guest_info * info, int pg_index, int dev_index) {
    return -1;
}
