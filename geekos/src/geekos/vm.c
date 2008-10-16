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



#include <geekos/vmm_stubs.h>

#include <geekos/debug.h>
#include <geekos/serial.h>
#include <geekos/vm.h>
#include <geekos/screen.h>

#include <palacios/vmm.h>
#include <palacios/vmm_io.h>


extern void * g_ramdiskImage;
extern ulong_t s_ramdiskSize;


int RunVMM(struct Boot_Info * bootInfo) {
  struct vmm_os_hooks os_hooks;
  struct vmm_ctrl_ops vmm_ops;
  struct guest_info * vm_info = 0;
  struct v3_vm_config vm_config;



  memset(&os_hooks, 0, sizeof(struct vmm_os_hooks));
  memset(&vmm_ops, 0, sizeof(struct vmm_ctrl_ops));
  memset(&vm_config, 0, sizeof(struct v3_vm_config));

  
  os_hooks.print_debug = &SerialPrint;
  os_hooks.print_info = &Print;
  os_hooks.print_trace = &SerialPrint;
  os_hooks.allocate_pages = &Allocate_VMM_Pages;
  os_hooks.free_page = &Free_VMM_Page;
  os_hooks.malloc = &VMM_Malloc;
  os_hooks.free = &VMM_Free;
  os_hooks.vaddr_to_paddr = &Identity;
  os_hooks.paddr_to_vaddr = &Identity;
  os_hooks.hook_interrupt = &geekos_hook_interrupt;
  os_hooks.ack_irq = &ack_irq;
  os_hooks.get_cpu_khz = &get_cpu_khz;


  
  Init_V3(&os_hooks, &vmm_ops);


  extern char _binary___palacios_vm_kernel_start;
  PrintBoth(" Guest Load Addr: 0x%x\n", &_binary___palacios_vm_kernel_start);
  
  vm_config.vm_kernel = &_binary___palacios_vm_kernel_start;
  
  
  if (g_ramdiskImage != NULL) {
    vm_config.use_ramdisk = 1;
    vm_config.ramdisk = g_ramdiskImage;
    vm_config.ramdisk_size = s_ramdiskSize;
  }



  vm_info = (vmm_ops).allocate_guest();

  Init_Stubs(vm_info);

  PrintBoth("Allocated Guest\n");

  (vmm_ops).config_guest(vm_info, &vm_config);

  PrintBoth("Configured guest\n");

  (vmm_ops).init_guest(vm_info);
  PrintBoth("Starting Guest\n");
  //Clear_Screen();

  (vmm_ops).start_guest(vm_info);
  
    return 0;
}
