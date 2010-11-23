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


struct blk_state {
    uint64_t capacity;
    addr_t blk_base_addr;
    uint8_t * blk_space;
};



static uint64_t blk_get_capacity(void * private_data) {
    struct blk_state * blk = (struct blk_state *)private_data;

    //  PrintDebug("SymBlk: Getting Capacity %d\n", (uint32_t)(blk->capacity));

    return blk->capacity;
}



static int blk_read(uint8_t * buf, uint64_t lba, uint64_t num_bytes, void * private_data) {
    struct blk_state * blk = (struct blk_state *)private_data;

    //    PrintDebug("TmpDisk Reading %d bytes to %p (lba=%p)\n", (uint32_t)num_bytes, buf, (void *)(addr_t)lba);

    if (lba + num_bytes > blk->capacity) {
	PrintError("TMPDISK Read past end of disk\n");
	return -1;
    }

    memcpy(buf, blk->blk_space + lba, num_bytes);

    return 0;
}




static int blk_write(uint8_t * buf,  uint64_t lba, uint64_t num_bytes, void * private_data) {
    struct blk_state * blk = (struct blk_state *)private_data;

    //    PrintDebug("TmpDisk Writing %d bytes to %p (lba=%p)\n", (uint32_t)num_bytes, buf, (void *)(addr_t)lba);

    if (lba + num_bytes > blk->capacity) {
	PrintError("TMPDISK Write past end of disk\n");
	return -1;
    }

    memcpy(blk->blk_space + lba, buf, num_bytes);

    return 0;
}


static int blk_free(struct vm_device * dev) {
    return -1;
}


static struct v3_dev_blk_ops blk_ops = {
    .read = blk_read, 
    .write = blk_write, 
    .get_capacity = blk_get_capacity,
};



static struct v3_device_ops dev_ops = {
    .free = blk_free,

};




static int blk_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct blk_state * blk = NULL;
    v3_cfg_tree_t * frontend_cfg = v3_cfg_subtree(cfg, "frontend");
    char * dev_id = v3_cfg_val(cfg, "ID");
    uint64_t capacity = atoi(v3_cfg_val(cfg, "size")) * 1024 * 1024;
    
    if (!frontend_cfg) {
	PrintError("Frontend Configuration not present\n");
	return -1;
    }

    PrintDebug("Intializing TMPDISK (capacity=%d)\n", (uint32_t)capacity);


    blk = (struct blk_state *)V3_Malloc(sizeof(struct blk_state));

    blk->capacity = capacity;
    
    blk->blk_base_addr = (addr_t)V3_AllocPages(blk->capacity / 4096);
    blk->blk_space = (uint8_t *)V3_VAddr((void *)(blk->blk_base_addr));
    memset(blk->blk_space, 0, capacity);


    struct vm_device * dev = v3_add_device(vm, dev_id, &dev_ops, blk);

    if (dev == NULL) {
	PrintError("Could not attach device %s\n", dev_id);
	V3_Free(blk);
	return -1;
    }

    if (v3_dev_connect_blk(vm, v3_cfg_val(frontend_cfg, "tag"), 
			   &blk_ops, frontend_cfg, blk) == -1) {
	PrintError("Could not connect %s to frontend %s\n", 
		   dev_id, v3_cfg_val(frontend_cfg, "tag"));
	v3_remove_device(dev);
	return -1;
    }


    return 0;
}

device_register("TMPDISK", blk_init)
