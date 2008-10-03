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

#ifndef _VMM_DEV_MGR
#define _VMM_DEV_MGR

#ifdef __V3VEE__

#include <palacios/vmm_types.h>
#include <palacios/vmm_list.h>
#include <palacios/vmm_string.h>

struct vm_device;
struct guest_info;


struct vmm_dev_mgr {
  uint_t num_devs;
  struct list_head dev_list;

  uint_t num_io_hooks;
  struct list_head io_hooks;
  
  uint_t num_mem_hooks;
  struct list_head mem_hooks;

};



// Registration of devices

//
// The following device manager functions should only be called
// when the guest is stopped
//

int v3_attach_device(struct guest_info *vm, struct vm_device * dev);
int v3_unattach_device(struct vm_device *dev);






struct dev_io_hook {
  ushort_t port;
  
  int (*read)(ushort_t port, void * dst, uint_t length, struct vm_device * dev);
  int (*write)(ushort_t port, void * src, uint_t length, struct vm_device * dev);

  struct vm_device * dev;

  // Do not touch anything below this  

  struct list_head dev_list;
  struct list_head mgr_list;
};

struct dev_mem_hook {
  void  *addr_start;
  void  *addr_end;

  struct vm_device * dev;

  // Do not touch anything below this
  struct list_head dev_list;
  struct list_head mgr_list;
};


int dev_mgr_init(struct guest_info * info);
int dev_mgr_deinit(struct guest_info * info);

void PrintDebugDevMgr(struct guest_info * info);
void PrintDebugDev(struct vm_device * dev);
void PrintDebugDevIO(struct vm_device * dev);
void PrintDebugDevMgrIO(struct vmm_dev_mgr * mgr);

#endif // ! __V3VEE__

#endif
