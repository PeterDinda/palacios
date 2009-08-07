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
#include <devices/lnx_virtio_blk.h>

#define SWAP_CAPACITY (150 * 1024 * 1024)

struct swap_state {
    
    struct vm_device * blk_dev;

    uint_t swapped_pages;
    uint_t unswapped_pages;

    uint64_t capacity;
    uint8_t * swap_space;
    addr_t swap_base_addr;
};



static uint64_t swap_get_capacity(void * private_data) {
    struct vm_device * dev = (struct vm_device *)private_data;
    struct swap_state * swap = (struct swap_state *)(dev->private_data);

    PrintDebug("SymSwap: Getting Capacity %d\n", (uint32_t)(swap->capacity));

    return swap->capacity / HD_SECTOR_SIZE;
}

static int swap_read(uint8_t * buf, int sector_count, uint64_t lba,  void * private_data) {
    struct vm_device * dev = (struct vm_device *)private_data;
    struct swap_state * swap = (struct swap_state *)(dev->private_data);
    int offset = lba * HD_SECTOR_SIZE;
    int length = sector_count * HD_SECTOR_SIZE;

    
    PrintDebug("SymSwap: Reading %d bytes to %p from %p\n", length,
	       buf, (void *)(swap->swap_space + offset));
    
    if (length % 4096) {
	PrintError("Swapping in length that is not a page multiple\n");
    }

    memcpy(buf, swap->swap_space + offset, length);

    swap->unswapped_pages += (length / 4096);

    PrintDebug("Swapped in %d pages\n", length / 4096);

    return 0;
}

static int swap_write(uint8_t * buf, int sector_count, uint64_t lba, void * private_data) {
    struct vm_device * dev = (struct vm_device *)private_data;
    struct swap_state * swap = (struct swap_state *)(dev->private_data);
    int offset = lba * HD_SECTOR_SIZE;
    int length = sector_count * HD_SECTOR_SIZE;
    /*
      PrintDebug("SymSwap: Writing %d bytes to %p from %p\n", length, 
      (void *)(swap->swap_space + offset), buf);
    */
    if (length % 4096) {
	PrintError("Swapping out length that is not a page multiple\n");
    }

    memcpy(swap->swap_space + offset, buf, length);

    swap->swapped_pages += (length / 4096);

    PrintDebug("Swapped out %d pages\n", length / 4096);

    return 0;
}


static int swap_free(struct vm_device * dev) {
    return -1;
}


static struct v3_hd_ops hd_ops = {
    .read = swap_read, 
    .write = swap_write, 
    .get_capacity = swap_get_capacity,
};



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
    swap->capacity = SWAP_CAPACITY;

    swap->swapped_pages = 0;
    swap->unswapped_pages = 0;

    swap->swap_base_addr = (addr_t)V3_AllocPages(swap->capacity / 4096);
    swap->swap_space = (uint8_t *)V3_VAddr((void *)(swap->swap_base_addr));
    memset(swap->swap_space, 0, SWAP_CAPACITY);


    struct vm_device * dev = v3_allocate_device("SYM_SWAP", &dev_ops, swap);

    if (v3_attach_device(vm, dev) == -1) {
	PrintError("Could not attach device %s\n", "SYM_SWAP");
	return -1;
    }


    v3_virtio_register_harddisk(virtio_blk, &hd_ops, dev);

    return 0;
}



device_register("SYM_SWAP", swap_init)
