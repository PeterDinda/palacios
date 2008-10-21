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



#include <palacios-glue/vmm_stubs.h>
#include <palacios-glue/vm.h>

#include <lwk/kernel.h>

#include <lwk/palacios.h>

#include <palacios/vmm.h>
#include <palacios/vmm_io.h>




int RunVMM() {
  struct v3_os_hooks os_hooks;
  struct v3_ctrl_ops v3_ops;
  struct guest_info * vm_info = 0;
  struct v3_vm_config vm_config;

  void * ramdiskImage=&initrd_start;
  ulong_t ramdiskSize=(&initrd_end)-(&initrd_start);

  memset(&os_hooks, 0, sizeof(struct v3_os_hooks));
  memset(&v3_ops, 0, sizeof(struct v3_ctrl_ops));
  memset(&vm_config, 0, sizeof(struct v3_vm_config));

  
  os_hooks.print_debug = &printk;  // serial print ideally
  os_hooks.print_info = &printk;   // serial print ideally
  os_hooks.print_trace = &printk;  // serial print ideally
  os_hooks.allocate_pages = &Allocate_VMM_Pages; // defined in vmm_stubs
  os_hooks.free_page = &Free_VMM_Page; // defined in vmm_stubs
  os_hooks.malloc = &kmem_alloc;
  os_hooks.free = &kmem_free;
  os_hooks.vaddr_to_paddr = &kitten_va_to_pa;
  os_hooks.paddr_to_vaddr = &kitten_pa_to_va;
  os_hooks.hook_interrupt = &kitten_hook_interrupt;
  os_hooks.ack_irq = &ack_irq;
  os_hooks.get_cpu_khz = &get_cpu_khz;


  
  Init_V3(&os_hooks, &v3_ops);

  
  vm_config.rombios = &rombios_start;
  vm_config.rombios_size = (&rombios_end)-(&rombios_start);
  vm_config.vgabios = &vgabios_start;
  vm_config.vgabios_size = (&vgabios_end)-(&vgabios_start);
  
  
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
  PrintBoth("Starting Guest\n");
  //Clear_Screen();

  (v3_ops).start_guest(vm_info);
  
    return 0;
}
