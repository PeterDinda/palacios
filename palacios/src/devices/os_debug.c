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


#include <devices/os_debug.h>
#include <palacios/vmm.h>


#define DEBUG_PORT1 0xcc




static int handle_gen_write(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
  PrintError("OS_DEBUG Write\n");

  switch (length) {
  case 1:
    PrintDebug("OS_DEBUG ->0x%.2x\n", *(uchar_t*)src);
    break;
  case 2:
    PrintDebug("OS_DEBUG ->0x%.4x\n", *(ushort_t*)src);
    break;
  case 4:
    PrintDebug("OS_DEBUG ->0x%.8x\n", *(uint_t*)src);
    break;
  default:
    PrintError("OS_DEBUG -> Invalid length in handle_gen_write\n");
    return -1;
    break;
  }

  return length;
}


static int debug_init(struct vm_device * dev) {

  v3_dev_hook_io(dev, DEBUG_PORT1,  NULL, &handle_gen_write);

  
  return 0;
}

static int debug_deinit(struct vm_device * dev) {
  v3_dev_unhook_io(dev, DEBUG_PORT1);


  return 0;
};




static struct vm_device_ops dev_ops = {
  .init = debug_init,
  .deinit = debug_deinit,
  .reset = NULL,
  .start = NULL,
  .stop = NULL,
};


struct vm_device * v3_create_os_debug() {

  PrintDebug("Creating OS Debug Device\n");
  struct vm_device * device = v3_create_device("OS Debug", &dev_ops, NULL);



  return device;
}
