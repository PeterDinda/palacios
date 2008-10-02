/* (c) 2008, Zheng Cui <cuizheng@cs.unm.edu> */
/* (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> */
/* (c) 2008, The V3VEE Project <http://www.v3vee.org> */

#ifndef __DEVICES_RAMDISK_H_
#define __DEVICES_RAMDISK_H_

#include <palacios/vmm_types.h>
#include <palacios/vm_dev.h>






struct vm_device * create_ramdisk(void);

#endif
