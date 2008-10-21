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

#include <palacios/vmm.h>
#include <palacios/vmm_host_events.h>

#include <lwk/pmem.h>

struct guest_info * g_vm_guest = NULL;


// This is the function the interface code should call to deliver
// the interrupt to the vmm for handling
//extern int v3_deliver_interrupt(struct guest_info * vm, struct v3_interrupt *intr);


struct guest_info * irq_to_guest_map[256];





void Init_Stubs(struct guest_info * info) {
  memset(irq_to_guest_map, 0, sizeof(struct guest_info *) * 256);
  g_vm_guest = info;
}


void * kitten_pa_to_va(void *ptr)
{
  return __va(ptr);
}

void * kitten_va_to_pa(void *ptr)
{
  return __pa(ptr);
}

void * Allocate_VMM_Pages(int num_pages) 
{
  int rc;
  struct pmem_region result;
  
  rc=pmem_alloc_umem(num_pages*PAGE_SIZE,PAGE_SIZE,&result);
  
  if (rc) {
    return 0;
  } else {
    return result.start;
  }
}

void Free_VMM_Page(void * page) 
{
  int rc;
  struct pmem_region query;
  struct pmem_region result;

  pmem_region_unset_all(&query);

  query.start=page;
  query.end=page+PAGE_SIZE;

  rc=pmem_query(&query,&result);

  if (!rc) { 
    result.allocated=FALSE;
    pmem_update(&result);
  } else {
    // BAD
  }
}


void * VMM_Malloc(unsigned int size) {
  return Malloc((unsigned long) size);
}


void VMM_Free(void * addr) {
  Free(addr);
}



void send_key_to_vmm(unsigned char status, unsigned char scancode) {
  struct v3_keyboard_event evt;

  evt.status = status;
  evt.scan_code = scancode;

  if (g_vm_guest) {
    v3_deliver_keyboard_event(g_vm_guest, &evt);
  }
}


void send_mouse_to_vmm(unsigned char packet[3]) {
  struct v3_mouse_event evt;

  memcpy(evt.data, packet, 3);

  if (g_vm_guest) {
    v3_deliver_mouse_event(g_vm_guest, &evt);
  }
}

void send_tick_to_vmm(unsigned int period_us) {
  struct v3_timer_event evt;

  evt.period_us = period_us;

  if (g_vm_guest) {
    v3_deliver_timer_event(g_vm_guest, &evt);
  }
}


void translate_intr_handler(struct pt_regs *regs, unsigned int vector) 
{
  struct v3_interrupt intr;

  intr.irq = vector-32;
  intr.error = regs->orig_rax;
  intr.should_ack = 0;

  //  PrintBoth("translate_intr_handler: opaque=0x%x\n",mystate.opaque);

  v3_deliver_irq(irq_to_guest_map[intr.irq], &intr);

}



int kitten_hook_interrupt(struct guest_info * vm, unsigned int  irq)
{
  if (irq_to_guest_map[irq]) { 
    //PrintBoth("Attempt to hook interrupt that is already hooked\n");
    return -1;
  } else {
    //PrintBoth("Hooked interrupt 0x%x with opaque 0x%x\n", irq, vm);
    irq_to_guest_map[irq] = vm;
  }

  set_idtvec_handler(irq,translate_intr_handler);
  return 0;
}


int ack_irq(int irq) 
{
  lapic_ack_interrupt();
  return 0;
}

  


unsigned int get_cpu_khz() 
{
  return   cpu_info[0].arch.cur_cpu_khz;
}


