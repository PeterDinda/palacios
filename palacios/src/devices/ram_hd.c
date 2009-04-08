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
#include <devices/ram_hd.h>
#include <devices/ide.h>



#ifndef DEBUG_IDE
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

struct hd_state {
    addr_t disk_image;
    uint32_t capacity; // in bytes

    struct vm_device * ide;

    uint_t bus;
    uint_t drive;
};


// HDs always read 2048 byte blocks... ?
static int hd_read(uint8_t * buf, int sector_count, uint64_t lba,  void * private_data) {
    struct vm_device * hd_dev = (struct vm_device *)private_data;
    struct hd_state * hd = (struct hd_state *)(hd_dev->private_data);
    int offset = lba * IDE_SECTOR_SIZE;
    int length = sector_count * IDE_SECTOR_SIZE;

    PrintDebug("Reading RAM HD at (LBA=%d) offset %d (length=%d)\n", (uint32_t)lba, offset, length);

    memcpy(buf, (uint8_t *)(hd->disk_image + offset), length);



    return 0;
}


static uint64_t hd_get_capacity(void * private_data) {
    struct vm_device * hd_dev = (struct vm_device *)private_data;
    struct hd_state * hd = (struct hd_state *)(hd_dev->private_data);
    PrintDebug("Querying RAM HD capacity (bytes=%d) (ret = %d)\n", 
	       hd->capacity, hd->capacity  / IDE_SECTOR_SIZE);
    return hd->capacity / IDE_SECTOR_SIZE;
}

static struct v3_ide_hd_ops hd_ops = {
    .read = hd_read, 
    .get_capacity = hd_get_capacity,
};


static int hd_init(struct vm_device * dev) {
    struct hd_state * hd = (struct hd_state *)(dev->private_data);

    if (v3_ide_register_harddisk(hd->ide, hd->bus, hd->drive, "V3-RAM-HD", &hd_ops, dev) == -1) {
	return -1;
    }
    
    return 0;
}


static int hd_deinit(struct vm_device * dev) {
    return 0;
}

static struct vm_device_ops dev_ops = {
    .init = hd_init, 
    .deinit = hd_deinit,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};

struct vm_device * v3_create_ram_hd(struct vm_device * ide, 
				    uint_t bus, uint_t drive, 
				    addr_t ramdisk, uint32_t size) {
    struct hd_state * hd = NULL;

    if (size % IDE_SECTOR_SIZE) {
	PrintError("HD image must be an integral of sector size (IDE_SECTOR_SIZE=%d)\n", IDE_SECTOR_SIZE);
	return NULL;
    }

    hd = (struct hd_state *)V3_Malloc(sizeof(struct hd_state));

    PrintDebug("Registering Ram HDD at %p (size=%d)\n", (void *)ramdisk, size);

    hd->disk_image = ramdisk;
    hd->capacity = size;

    hd->ide = ide;
    hd->bus = bus;
    hd->drive = drive;
	
    struct vm_device * hd_dev = v3_create_device("RAM-HD", &dev_ops, hd);

    return hd_dev;
}
