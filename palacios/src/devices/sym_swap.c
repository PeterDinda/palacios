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

#include <devices/sym_swap.h>
#include <devices/lnx_virtio.h>



struct swap_state {
    
    struct vm_device * blk_dev;

};


static int swap_init(struct vm_device * dev) {
    return -1;
}


static int swap_deinit(struct vm_device * dev) {
    return -1;
}



static struct vm_device_ops dev_ops = {
    .init = swap_init, 
    .deinit = swap_deinit,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};



struct vm_device * v3_create_swap(struct vm_device * virtio_blk) {
    struct swap_state * swap = NULL;

    PrintDebug("Creating Swap Device\n");

    if (virtio_blk == NULL) {
	PrintError("Swap device requires a virtio block device\n");
	return NULL;
    }

    swap = (struct swap_state *)V3_Malloc(sizeof(struct swap_state));

    swap->blk_dev = virtio_blk;

    return v3_create_device("SYM_SWAP", &dev_ops, swap);
}
