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


#include <devices/apic.h>
#include <palacios/vmm.h>
#include <palacios/vmm_msr.h>

#define BASE_ADDR_MSR 0x0000001B


struct apic_state {
  v3_msr_t base_addr_reg;

};


static int read_base_addr(uint_t msr, v3_msr_t * dst, void * priv_data) {
  struct vm_device * dev = (struct vm_device *)priv_data;
  struct apic_state * apic = (struct apic_state *)dev->private_data;
  PrintDebug("READING APIC BASE ADDR: HI=%x LO=%x\n", apic->base_addr_reg.hi, apic->base_addr_reg.lo);

  return -1;
}


static int write_base_addr(uint_t msr, v3_msr_t src, void * priv_data) {
  //  struct vm_device * dev = (struct vm_device *)priv_data;
  //  struct apic_state * apic = (struct apic_state *)dev->private_data;

  PrintDebug("WRITING APIC BASE ADDR: HI=%x LO=%x\n", src.hi, src.lo);

  return -1;
}


static int apic_deinit(struct vm_device * dev) {
  struct guest_info * info = dev->vm;

  v3_unhook_msr(info, BASE_ADDR_MSR);

  return 0;
}


static int apic_init(struct vm_device * dev) {
  struct guest_info * info = dev->vm;

  v3_hook_msr(info, BASE_ADDR_MSR, read_base_addr, write_base_addr, dev);

  return 0;
}



static struct vm_device_ops dev_ops = {
  .init = apic_init,
  .deinit = apic_deinit,
  .reset = NULL,
  .start = NULL,
  .stop = NULL,
};


struct vm_device * v3_create_apic() {
  PrintDebug("Creating APIC\n");

  struct apic_state * apic = (struct apic_state *)V3_Malloc(sizeof(struct apic_state));

  struct vm_device * device = v3_create_device("APIC", &dev_ops, apic);
  
  return device;
}
