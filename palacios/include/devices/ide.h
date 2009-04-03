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

#ifndef __DEVICES_IDE_H__
#define __DEVICES_IDE_H__

#ifdef __V3VEE__
#include <palacios/vm_dev.h>


#define ATAPI_BLOCK_SIZE 2048
#define IDE_SECTOR_SIZE 512

typedef enum {IDE_DISK, IDE_CDROM, IDE_NONE} v3_ide_dev_type_t;

struct v3_ide_cd_ops {
    uint32_t (*get_capacity)(void * private_data);
    // Reads always operate on 2048 byte blocks
    int (*read)(uint8_t * buf, int count, int lba, void * private_data);

};


struct v3_ide_hd_ops {
    uint32_t (*get_capacity)(void * private_data);
    // Reads always operate on 2048 byte blocks
    int (*read)(uint8_t * buf, int count, int lba, void * private_data);    

};


int v3_ide_register_cdrom(struct vm_device * ide, 
			  uint_t bus_num, 
			  uint_t drive_num, 
			  char * drive_name,
			  struct v3_ide_cd_ops * ops, 
			  void * private_data);

int v3_ide_register_harddisk(struct vm_device * ide, 
			     uint_t bus_num, 
			     uint_t drive_num, 
			     char * drive_name,
			     struct v3_ide_hd_ops * ops, 
			     void * private_data);



struct vm_device * v3_create_ide();



#endif // ! __V3VEE__


#endif

