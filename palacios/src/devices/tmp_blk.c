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


#define BLK_CAPACITY (500 * 1024 * 1024)




struct blk_state {

    struct vm_device * blk_dev;

    uint64_t capacity;
    uint8_t * blk_space;
    addr_t blk_base_addr;
};



static uint64_t blk_get_capacity(void * private_data) {
    struct vm_device * dev = (struct vm_device *)private_data;
    struct blk_state * blk = (struct blk_state *)(dev->private_data);

    PrintDebug("SymBlk: Getting Capacity %d\n", (uint32_t)(blk->capacity));

    return blk->capacity / HD_SECTOR_SIZE;
}



static int blk_read(uint8_t * buf, int sector_count, uint64_t lba,  void * private_data) {
    struct vm_device * dev = (struct vm_device *)private_data;
    struct blk_state * blk = (struct blk_state *)(dev->private_data);
    uint32_t offset = lba * HD_SECTOR_SIZE;
    uint32_t length = sector_count * HD_SECTOR_SIZE;

    memcpy(buf, blk->blk_space + offset, length);

    return 0;
}




static int blk_write(uint8_t * buf, int sector_count, uint64_t lba, void * private_data) {
    struct vm_device * dev = (struct vm_device *)private_data;
    struct blk_state * blk = (struct blk_state *)(dev->private_data);
    uint32_t offset = lba * HD_SECTOR_SIZE;
    uint32_t length = sector_count * HD_SECTOR_SIZE;

    memcpy(blk->blk_space + offset, buf, length);

    return 0;
}


static int blk_free(struct vm_device * dev) {
    return -1;
}


static struct v3_hd_ops hd_ops = {
    .read = blk_read, 
    .write = blk_write, 
    .get_capacity = blk_get_capacity,
};



static struct v3_device_ops dev_ops = {
    .free = blk_free,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};




static int blk_init(struct guest_info * vm, void * cfg_data) {
    struct blk_state * blk = NULL;
    struct vm_device * virtio_blk = v3_find_dev(vm, (char *)cfg_data);

    if (!virtio_blk) {
	PrintError("could not find Virtio backend\n");
	return -1;
    }

    PrintDebug("Creating Blk Device\n");

    if (virtio_blk == NULL) {
	PrintError("Blk device requires a virtio block device\n");
	return -1;
    }

    blk = (struct blk_state *)V3_Malloc(sizeof(struct blk_state) + ((BLK_CAPACITY / 4096) / 8));

    blk->blk_dev = virtio_blk;
    blk->capacity = BLK_CAPACITY;

    blk->blk_base_addr = (addr_t)V3_AllocPages(blk->capacity / 4096);
    blk->blk_space = (uint8_t *)V3_VAddr((void *)(blk->blk_base_addr));
    memset(blk->blk_space, 0, BLK_CAPACITY);


    struct vm_device * dev = v3_allocate_device("TMP_BLK", &dev_ops, blk);

    if (v3_attach_device(vm, dev) == -1) {
	PrintError("Could not attach device %s\n", "TMP_BLK");
	return -1;
    }


    v3_virtio_register_harddisk(virtio_blk, &hd_ops, dev);


    return 0;
}

device_register("TMP_BLK", blk_init)
