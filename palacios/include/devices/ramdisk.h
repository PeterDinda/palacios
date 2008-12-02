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
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Zheng Cui <cuizheng@cs.unm.edu> 
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __DEVICES_RAMDISK_H__
#define __DEVICES_RAMDISK_H__

#ifdef __V3VEE__



#include <palacios/vmm_types.h>
#include <palacios/vm_dev.h>

struct cdrom_ops;


int v3_ramdisk_register_cdrom(struct vm_device * ide_dev, uint_t busID, uint_t driveID, struct cdrom_ops * cd, void * private_data);

struct vm_device * v3_create_ramdisk();


#endif // ! __V3VEE__

#endif
