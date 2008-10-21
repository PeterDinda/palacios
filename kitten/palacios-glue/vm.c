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



#include <lwk/kernel.h>
#include <lwk/palacios.h>

#include <palacios/vmm.h>
#include <palacios/vmm_io.h>




int RunVMM() {
  struct v3_ctrl_ops v3_ops = {};
  struct guest_info * vm_info = 0;

  void * ramdiskImage=initrd_start;
  uintptr_t ramdiskSize=initrd_end-initrd_start;


  
  Init_V3(&v3vee_os_hooks, &v3_ops);

  
	struct v3_vm_config vm_config = {
		.rombios = &rombios_start,
		.rombios_size = (&rombios_end)-(&rombios_start),
		.vgabios = &vgabios_start,
		.vgabios_size = (&vgabios_end)-(&vgabios_start),
	};
  
  
  if (ramdiskImage != NULL) {
    vm_config.use_ramdisk = 1;
    vm_config.ramdisk = ramdiskImage;
    vm_config.ramdisk_size = ramdiskSize;
  }



  vm_info = (v3_ops).allocate_guest();

  Init_Stubs(vm_info);

  //PrintBoth("Allocated Guest\n");

  (v3_ops).config_guest(vm_info, &vm_config);

  //PrintBoth("Configured guest\n");

  (v3_ops).init_guest(vm_info);
  printk("Starting Guest\n");
  //Clear_Screen();

  (v3_ops).start_guest(vm_info);
  
    return 0;
}
