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


#ifndef V3_CONFIG_DEBUG_RAMDISK
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

struct disk_state {
    uint8_t * disk_image;
    uint32_t capacity; // in bytes
};


static int read(uint8_t * buf, uint64_t lba, uint64_t num_bytes, void * private_data) {
    struct disk_state * disk = (struct disk_state *)private_data;

    PrintDebug("Reading %d bytes from %p to %p\n", (uint32_t)num_bytes, (uint8_t *)(disk->disk_image + lba), buf);

    if (lba + num_bytes > disk->capacity) {
	PrintError("read out of bounds:  lba=%llu (%p), num_bytes=%llu, capacity=%d (%p)\n", 
		   lba, (void *)(addr_t)lba, num_bytes, disk->capacity, (void *)(addr_t)disk->capacity);
	return -1;
    }

    memcpy(buf, (uint8_t *)(disk->disk_image + lba), num_bytes);

    return 0;
}


static int write(uint8_t * buf, uint64_t lba, uint64_t num_bytes, void * private_data) {
    struct disk_state * disk = (struct disk_state *)private_data;

    PrintDebug("Writing %d bytes from %p to %p\n", (uint32_t)num_bytes,  buf, (uint8_t *)(disk->disk_image + lba));

    if (lba + num_bytes > disk->capacity) {
	PrintError("write out of bounds: lba=%llu (%p), num_bytes=%llu, capacity=%d (%p)\n", 
		   lba, (void *)(addr_t)lba, num_bytes, disk->capacity, (void *)(addr_t)disk->capacity);
	return -1;
    }


    memcpy((uint8_t *)(disk->disk_image + lba), buf, num_bytes);

    return 0;
}


static uint64_t get_capacity(void * private_data) {
    struct disk_state * disk = (struct disk_state *)private_data;

    PrintDebug("Querying RAMDISK capacity %d\n", 
	       (uint32_t)(disk->capacity));

    return disk->capacity;
}

static struct v3_dev_blk_ops blk_ops = {
    .read = read, 
    .write = write,
    .get_capacity = get_capacity,
};




static int disk_free(struct disk_state * state) {

    V3_Free(state);
    return 0;
}

static struct v3_device_ops dev_ops = {
    .free = (int (*)(void *))disk_free,
};




static int disk_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct disk_state * disk = NULL;
    struct v3_cfg_file * file = NULL;
    char * dev_id = v3_cfg_val(cfg, "ID");
    char * filename = v3_cfg_val(cfg, "file");

    v3_cfg_tree_t * frontend_cfg = v3_cfg_subtree(cfg, "frontend");

    if (!filename) {
	PrintError("Missing filename (%s) for %s\n", filename, dev_id);
	return -1;
    }

    file = v3_cfg_get_file(vm, filename);

    if (!file) {
	PrintError("Invalid ramdisk file: %s\n", filename);
	return -1;
    }


    disk = (struct disk_state *)V3_Malloc(sizeof(struct disk_state));
    memset(disk, 0, sizeof(struct disk_state));

    disk->disk_image = file->data;
    disk->capacity = file->size;
    PrintDebug("Registering RAMDISK at %p (size=%d)\n", 
	       (void *)file->data, (uint32_t)file->size);


    struct vm_device * dev = v3_add_device(vm, dev_id, &dev_ops, disk);

    if (dev == NULL) {
	PrintError("Could not attach device %s\n", dev_id);
	V3_Free(disk);
	return -1;
    }


    if (v3_dev_connect_blk(vm, v3_cfg_val(frontend_cfg, "tag"), 
			   &blk_ops, frontend_cfg, disk) == -1) {
	PrintError("Could not connect %s to frontend %s\n", 
		   dev_id, v3_cfg_val(frontend_cfg, "tag"));
	v3_remove_device(dev);
	return -1;
    }
    

    return 0;
}


device_register("RAMDISK", disk_init)
