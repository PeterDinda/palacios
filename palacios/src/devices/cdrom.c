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
#include <palacios/vmm.h>

#ifdef DEBUG_RAMDISK
#define Ramdisk_Print_CD(_f, _a...) PrintTrace("cdrom.c(%d) "_f, __LINE__, ## _a)
#else
#define Ramdisk_Print_CD(_f, _a...)
#endif


extern ulong_t g_ramdiskImage;
extern ulong_t s_ramdiskSize;

static
void cdrom_init(struct cdrom_interface * cdrom)
{

  Ramdisk_Print_CD("[cdrom_init]\n");
  V3_ASSERT(g_ramdiskImage);
  cdrom->fd = g_ramdiskImage;
  PrintDebug("CDIMAGE located at: %x\n", cdrom->fd);
  cdrom->capacity_B = s_ramdiskSize; 
  //FIXME:lba
  cdrom->lba = 1; 
  return;
}

/* 
 * Load CD-ROM. Returns false if CD is not ready.
 */
 
static
rd_bool cdrom_insert(struct cdrom_interface * cdrom, char *dev /*= NULL*/)
{
  Ramdisk_Print_CD("[cdrom_insert]\n");
  return 1;
}

/*
 * Logically eject the CD.
 */
static
void cdrom_eject(struct cdrom_interface *cdrom)
{
  Ramdisk_Print_CD("[cdrom_eject]\n");
  return;
}

/*
 * Read CD TOC. Returns false if start track is out of bounds.
 */
static
rd_bool cdrom_read_toc(struct cdrom_interface *cdrom, uint8_t* buf, int* length, rd_bool msf, int start_track)
{
  Ramdisk_Print_CD("[cdrom_read_toc]\n");
  return 1;
}

/*
 * Return CD-ROM capacity (in 2048 byte frames)
 */
static
uint32_t cdrom_capacity(struct cdrom_interface *cdrom)
{
  Ramdisk_Print_CD("[cdrom_capacity] s_ramdiskSize = %d\n", cdrom->capacity_B);
  if (cdrom->lba) {
    if (cdrom->capacity_B % 2048) {
      Ramdisk_Print_CD("\t\t capacity in LBA is %d\n", cdrom->capacity_B/2048 + 1);
      return cdrom->capacity_B/2048 + 1;
    } else {
      Ramdisk_Print_CD("\t\t capacity in LBA is %d\n", cdrom->capacity_B/2048);
      return cdrom->capacity_B/2048;
    }
  } else {
    //FIXME CHS mode
    return 0;
  }
}

/*
 * Read a single block from the CD
 */
static
void cdrom_read_block(struct cdrom_interface *cdrom, uint8_t* buf, int lba)// __attribute__(regparm(2));
{

  V3_ASSERT(lba != 0);
  
  Ramdisk_Print_CD("[cdrom_read_block] lba = %d (cdrom_image_start=%x)\n", lba, cdrom->fd);
  memcpy(buf, (uint8_t *)(cdrom->fd + lba * 2048), 2048);
  PrintDebug("Returning from read block\n");
    return;
}

/*
 * Start (spin up) the CD.
 */
static
int cdrom_start(struct cdrom_interface *cdrom)
{
  Ramdisk_Print_CD("[cdrom_start]\n");
  return 1;
}


void init_cdrom(struct cdrom_interface *cdrom)
{
  V3_ASSERT(cdrom != NULL);
  
  cdrom->ops.init = &cdrom_init;
  cdrom->ops.insert_cdrom = &cdrom_insert;
  cdrom->ops.eject_cdrom = &cdrom_eject;
  cdrom->ops.read_toc = &cdrom_read_toc;
  cdrom->ops.capacity = &cdrom_capacity;
  cdrom->ops.read_block = &cdrom_read_block;
  cdrom->ops.start_cdrom = &cdrom_start;

  return;
}

