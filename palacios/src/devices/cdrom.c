/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Zheng Cui <cuizheng@cs.unm.edu> 
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Zheng Cui <cuizheng@cs.unm.edu> 
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */



#include <devices/cdrom.h>
#include <devices/ide.h>
#include <palacios/vmm.h>

#ifndef DEBUG_RAMDISK
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif



struct cdrom_state {
  uchar_t * image_addr; //memory address
  ulong_t capacity_in_bytes;
  ulong_t head; //current position

  struct vm_device * ide_dev;

  uchar_t lba;
};




/* 
 * Load CD-ROM. Returns false if CD is not ready.
 */
 
static rd_bool cdrom_insert(void * private_data) {
  PrintDebug("[cdrom_insert]\n");
  return 1;
}

/*
 * Logically eject the CD.
 */
static void cdrom_eject(void * private_data) {
  PrintDebug("[cdrom_eject]\n");
  return;
}

/*
 * Read CD TOC. Returns false if start track is out of bounds.
 */
static rd_bool cdrom_read_toc(void * private_data, uint8_t* buf, int* length, rd_bool msf, int start_track)
{
  *length = 4;
  PrintDebug("[cdrom_read_toc]\n");
  return 1;
}

/*
 * Return CD-ROM capacity (in 2048 byte frames)
 */
static uint32_t cdrom_capacity(void * private_data) {
  struct cdrom_state * cdrom = (struct cdrom_state *)private_data;

  PrintDebug("[cdrom_capacity] s_ramdiskSize = %d\n", cdrom->capacity_in_bytes);

  if (cdrom->lba) {
    if (cdrom->capacity_in_bytes % 2048) {
      PrintDebug("\t\t capacity in LBA is %d\n", (cdrom->capacity_in_bytes / 2048) + 1);
      return (cdrom->capacity_in_bytes / 2048) + 1;
    } else {
      PrintDebug("\t\t capacity in LBA is %d\n", cdrom->capacity_in_bytes / 2048);
      return cdrom->capacity_in_bytes / 2048;
    }
  } else {
    PrintError("Unsupported CDROM mode in capacity query\n");
    //FIXME CHS mode
    return 0;
  }
}

/*
 * Read a single block from the CD
 */
static void cdrom_read_block(void * private_data, uint8_t * buf, int lba)/* __attribute__(regparm(2)); */ {
  struct cdrom_state * cdrom = (struct cdrom_state *)private_data;

  V3_ASSERT(lba != 0);
  
  PrintDebug("[cdrom_read_block] lba = %d (cdrom_image_start=%x)\n", lba, cdrom->image_addr);
  memcpy(buf, (uchar_t *)(cdrom->image_addr + lba * 2048), 2048);
  //PrintDebug("Returning from read block\n");
  return;
}

static void set_LBA(void * private_data, uchar_t lba) {
  struct cdrom_state * cdrom = (struct cdrom_state *)private_data;
  cdrom->lba = lba;
}


/*
 * Start (spin up) the CD.
 */
static int cdrom_start(void * private_data) {
  PrintDebug("[cdrom_start]\n");
  return 1;
}


static struct cdrom_ops cd_ops = {
  .insert_cdrom = cdrom_insert,
  .eject_cdrom = cdrom_eject,
  .read_toc = cdrom_read_toc,
  .capacity = cdrom_capacity,
  .read_block = cdrom_read_block,
  .start_cdrom = cdrom_start,
  .set_LBA = set_LBA,
};




static int cdrom_device_init(struct vm_device * dev) {
  struct cdrom_state * cdrom = (struct cdrom_state *)dev->private_data;
  PrintDebug("[cdrom_init]\n");
  PrintDebug("CDIMAGE located at: %x\n", cdrom->image_addr);

  //FIXME:lba
  cdrom->lba = 1; 

  v3_ramdisk_register_cdrom(cdrom->ide_dev, 1, 0, &cd_ops, cdrom);

  return 0;
}


static int cdrom_device_deinit(struct vm_device * dev) {
  return 0;
}

static struct vm_device_ops dev_ops = {
  .init = cdrom_device_init,
  .deinit = cdrom_device_deinit,
  .reset = NULL,
  .start = NULL,
  .stop = NULL,
};

struct vm_device *  v3_create_cdrom(struct vm_device * ramdisk_dev, void * ramdisk, uint_t ramdisk_size){
  struct cdrom_state * cd = (struct cdrom_state *)V3_Malloc(sizeof(struct cdrom_state));
  V3_ASSERT(cd != NULL);

  memset(cd, 0, sizeof(struct cdrom_state));

  cd->image_addr = (uchar_t *)ramdisk;
  cd->capacity_in_bytes = ramdisk_size;
  cd->ide_dev = ramdisk_dev;
  
  PrintDebug("Creating RamDISK CDROM\n");

  struct vm_device * cd_dev = v3_create_device("Ram Based CD", &dev_ops, cd);

  return cd_dev;
}


