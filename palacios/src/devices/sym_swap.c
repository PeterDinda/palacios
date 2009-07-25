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
#include <palacios/vmm_dev_mgr.h>
#include <devices/lnx_virtio.h>



struct swap_state {
    
    struct vm_device * blk_dev;

};




static int swap_free(struct vm_device * dev) {
    return -1;
}



static struct v3_device_ops dev_ops = {
    .free = swap_free,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};




static int swap_init(struct guest_info * vm, void * cfg_data) {
    struct swap_state * swap = NULL;
    struct vm_device * virtio_blk = v3_find_dev(vm, (char *)cfg_data);

    if (!virtio_blk) {
	PrintError("could not find Virtio backend\n");
	return -1;
    }

    PrintDebug("Creating Swap Device\n");

    if (virtio_blk == NULL) {
	PrintError("Swap device requires a virtio block device\n");
	return -1;
    }

    swap = (struct swap_state *)V3_Malloc(sizeof(struct swap_state));

    swap->blk_dev = virtio_blk;

    struct vm_device * dev = v3_allocate_device("SYM_SWAP", &dev_ops, swap);

    if (v3_attach_device(vm, dev) == -1) {
	PrintError("Could not attach device %s\n", "SYM_SWAP");
	return -1;
    }

    return 0;
}



device_register("SYM_SWAP", swap_init)
