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
#include <geekos/serial.h>
#include <geekos/debug.h>
#include <palacios/vmm.h>
#include <palacios/vmm_host_events.h>


struct guest_info * g_vm_guest = NULL;


// This is the function the interface code should call to deliver
// the interrupt to the vmm for handling
//extern int v3_deliver_interrupt(struct guest_info * vm, struct v3_interrupt *intr);


struct guest_info * irq_to_guest_map[256];




static inline void VM_Out_Byte(ushort_t port, uchar_t value)
{
    __asm__ __volatile__ (
	"outb %b0, %w1"
	:
	: "a" (value), "Nd" (port)
    );
}

/*
 * Read a byte from an I/O port.
 */
static inline uchar_t VM_In_Byte(ushort_t port)
{
    uchar_t value;

    __asm__ __volatile__ (
	"inb %w1, %b0"
	: "=a" (value)
	: "Nd" (port)
    );

    return value;
}



void Init_Stubs(struct guest_info * info) {
  memset(irq_to_guest_map, 0, sizeof(struct guest_info *) * 256);
  g_vm_guest = info;
}



void * Identity(void *addr) { return addr; };

void * Allocate_VMM_Pages(int num_pages) {
  void * start_page = Alloc_Page();
  //SerialPrint("Starting by Allocating Page: %x (%d of %d)\n",start_page, 1, num_pages); 
  int i = 1;

  while (i < num_pages) {
    void * tmp_page = Alloc_Page();
    //SerialPrint("Allocating Page: %x (%d of %d)\n",tmp_page, i+1, num_pages); 
    
    if (tmp_page != start_page + (PAGE_SIZE * i)) {
      //we have to start over...;
      while (i >= 0) {
	Free_Page(start_page + (PAGE_SIZE * i));
	i--;
      }
      start_page = Alloc_Page();
      //SerialPrint("Starting over by Allocating Page: %x (%d of %d)\n",start_page, 1, num_pages);
      i = 1;
      continue;
    }
    i++;
  }

  return start_page;
}

void Free_VMM_Page(void * page) {
  Free_Page(page);
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


void translate_intr_handler(struct Interrupt_State *state) {
  struct v3_interrupt intr;

  intr.irq = state->intNum - 32;
  intr.error = state->errorCode;
  intr.should_ack = 0;

  //  PrintBoth("translate_intr_handler: opaque=0x%x\n",mystate.opaque);

  v3_deliver_irq(irq_to_guest_map[intr.irq], &intr);

  End_IRQ(state);

}



int geekos_hook_interrupt(struct guest_info * vm, unsigned int  irq)
{
  if (irq_to_guest_map[irq]) { 
    PrintBoth("Attempt to hook interrupt that is already hooked\n");
    return -1;
  } else {
    PrintBoth("Hooked interrupt 0x%x with opaque 0x%x\n", irq, vm);
    irq_to_guest_map[irq] = vm;
  }

  Disable_IRQ(irq);
  Install_IRQ(irq, translate_intr_handler);
  Enable_IRQ(irq);
  return 0;
}


int ack_irq(int irq) {
  End_IRQ_num(irq);
  return 0;
}

  


unsigned int get_cpu_khz() {
  extern uint_t cpu_khz_freq;

  unsigned long  print_khz = (unsigned long)(cpu_khz_freq & 0xffffffff);
  
  PrintBoth("Detected %lu.%lu MHz CPU\n", print_khz / 1000, print_khz % 1000);

  return cpu_khz_freq;
}


