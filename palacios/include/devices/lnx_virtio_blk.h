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

#ifndef __DEVICES_LNX_VIRTIO_BLK_H__
#define __DEVICES_LNX_VIRTIO_BLK_H__

#ifdef __V3VEE__

#include <devices/block_dev.h>

int v3_virtio_register_cdrom(struct vm_device * dev, 
			     struct v3_cd_ops * ops, 
			     void * private_data);


int v3_virtio_register_harddisk(struct vm_device * dev, 
				struct v3_hd_ops * ops, 
				void * private_data);
			     

#endif


#endif
