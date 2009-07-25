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
#include <devices/ram_cd.h>
#include <palacios/vmm_dev_mgr.h>
#include <devices/ide.h>

#ifndef DEBUG_IDE
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


struct cd_state {
    addr_t disk_image;
    uint32_t capacity; // in bytes

    struct vm_device * ide;

    uint_t bus;
    uint_t drive;
};


// CDs always read 2048 byte blocks... ?
static int cd_read(uint8_t * buf, int block_count, uint64_t lba,  void * private_data) {
    struct vm_device * cd_dev = (struct vm_device *)private_data;
    struct cd_state * cd = (struct cd_state *)(cd_dev->private_data);
    int offset = lba * ATAPI_BLOCK_SIZE;
    int length = block_count * ATAPI_BLOCK_SIZE;

    PrintDebug("Reading RAM CD at (LBA=%d) offset %d (length=%d)\n", (uint32_t)lba, offset, length);

    memcpy(buf, (uint8_t *)(cd->disk_image + offset), length);

    return 0;
}


static uint32_t cd_get_capacity(void * private_data) {
    struct vm_device * cd_dev = (struct vm_device *)private_data;
    struct cd_state * cd = (struct cd_state *)(cd_dev->private_data);
    PrintDebug("Querying RAM CD capacity (bytes=%d) (ret = %d)\n", 
	       cd->capacity, cd->capacity  / ATAPI_BLOCK_SIZE);
    return cd->capacity / ATAPI_BLOCK_SIZE;
}

static struct v3_ide_cd_ops cd_ops = {
    .read = cd_read, 
    .get_capacity = cd_get_capacity,
};




static int cd_free(struct vm_device * dev) {
    return 0;
}

static struct v3_device_ops dev_ops = {
    .free = cd_free,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};


static int cd_init(struct guest_info * vm, void * cfg_data) {
    struct cd_state * cd = NULL;
    struct ram_cd_cfg * cfg = (struct ram_cd_cfg *)cfg_data;

    if (cfg->size % ATAPI_BLOCK_SIZE) {
	PrintError("CD image must be an integral of block size (ATAPI_BLOCK_SIZE=%d)\n", ATAPI_BLOCK_SIZE);
	return -1;
    }

    cd = (struct cd_state *)V3_Malloc(sizeof(struct cd_state));

    PrintDebug("Registering Ram CD at %p (size=%d)\n", (void *)ramdisk, size);

  
    cd->disk_image = cfg->ramdisk;
    cd->capacity = cfg->size;

    cd->ide = v3_find_dev(vm, cfg->ide);

    if (cd->ide == 0) {
	PrintError("Could not find backend %s\n", cfg->ide);
	return -1;
    }

    cd->bus = cfg->bus;
    cd->drive = cfg->drive;
	
    struct vm_device * dev = v3_allocate_device("RAM-CD", &dev_ops, cd);

    if (v3_attach_device(vm, dev) == -1) {
	PrintError("Could not attach device %s\n", "RAM-CD");
	return -1;
    }


    if (v3_ide_register_cdrom(cd->ide, cd->bus, cd->drive, "RAM-CD", &cd_ops, dev) == -1) {
	return -1;
    }
    
    return 0;
}


device_register("RAM-CD", cd_init)
