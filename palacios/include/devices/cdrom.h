
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

#ifndef __DEVICES_CDROM_H__
#define __DEVICES_CDROM_H__


#ifdef __V3VEE__

#include <devices/ramdisk.h>
#include <devices/ide.h>
#include <palacios/vmm_types.h>





struct cdrom_ops {
    /* 
     * Load CD-ROM. Returns false if CD is not ready. 
     */
    rd_bool (*insert_cdrom)(void * private_data);

    /* 
     * Logically eject the CD.
     */
    void (*eject_cdrom)(void * private_data);
  
    /* 
     * Read CD TOC. Returns false if start track is out of bounds.
     */
    rd_bool (*read_toc)(void * private_data, uchar_t * buf, int * length, rd_bool msf, int start_track);
  
    /* 
     * Return CD-ROM capacity (in 2048 byte frames)
     */
    uint32_t (*capacity)(void * private_data);
  
    /*
     * Read a single block from the CD
     */
    void (*read_block)(void * private_data, uchar_t * buf, int lba);
  
    /*
     * Start (spin up) the CD.
     */
    int (*start_cdrom)(void * private_data);

    void (*set_LBA)(void * private_data, uchar_t lba);
};




struct vm_device * v3_create_cdrom(struct vm_device * ramdisk_dev, void * ramdisk, uint_t ramdisk_size);



#endif // !__V3VEE__

#endif
