/*
 * Zheng Cui
 * cuizheng@cs.unm.edu
 * July 2008
 */

#ifndef __DEVICES_RAMDISK_H_
#define __DEVICES_RAMDISK_H_

#include <palacios/vmm_types.h>
#include <palacios/vm_dev.h>






struct vm_device * create_ramdisk(void);

#endif
