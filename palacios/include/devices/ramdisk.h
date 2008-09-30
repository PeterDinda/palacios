/*
 * Zheng Cui
 * cuizheng@cs.unm.edu
 * July 2008
 */

#ifndef __DEVICES_RAMDISK_H_
#define __DEVICES_RAMDISK_H_

#include <stddef.h> //for off_t in C99
#include <sys/types.h> //for size_t 
#include <geekos/ktypes.h>
#include <devices/cdrom.h>
#include <palacios/vm_dev.h>

struct vm_device * create_ramdisk(void);

#endif
