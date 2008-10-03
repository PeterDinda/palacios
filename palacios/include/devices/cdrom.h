
/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Zheng Cui<cuizheng@cs.unm.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Zheng Cui<cuizheng@cs.unm.edu> 
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __DEVICES_CDROM_H_
#define __DEVICES_CDROM_H_

#include <devices/ramdisk.h>
#include <devices/ide.h>
#include <palacios/vmm_types.h>




struct cdrom_interface;

struct cdrom_ops {
  
  void (*init)(struct cdrom_interface *cdrom);

  /* 
   * Load CD-ROM. Returns false if CD is not ready. 
   */
  rd_bool (*insert_cdrom)(struct cdrom_interface *cdrom, char *dev /*= NULL*/);

  /* 
   * Logically eject the CD.
   */
  void (*eject_cdrom)(struct cdrom_interface *cdrom);
  
  /* 
   * Read CD TOC. Returns false if start track is out of bounds.
   */
  rd_bool (*read_toc)(struct cdrom_interface * cdrom, uint8_t * buf, int* length, rd_bool msf, int start_track);
  
  /* 
   * Return CD-ROM capacity (in 2048 byte frames)
   */
  uint32_t (*capacity)(struct cdrom_interface *cdrom);
  
  /*
   * Read a single block from the CD
   */
  void (*read_block)(struct cdrom_interface *cdrom, uint8_t* buf, int lba);
  
  /*
   * Start (spin up) the CD.
   */
  int (*start_cdrom)(struct cdrom_interface *cdrom);
};


struct cdrom_interface {

  struct cdrom_ops ops;

  ulong_t fd; //memory address
  ulong_t capacity_B;
  ulong_t head; //current position

  uchar_t lba;

  char *path; //for ramdisk, NULL
  int using_file; //no
};

void init_cdrom(struct cdrom_interface *cdrom);

#endif
