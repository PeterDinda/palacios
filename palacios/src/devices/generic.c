
/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Peter Dinda <pdinda@northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 * Contributor: 2008, Jack Lange <jarusl@cs.northwestern.edu>
 *        
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#include <devices/generic.h>
#include <palacios/vmm.h>
#include <palacios/vmm_types.h>
#include <geekos/io.h>



#ifndef DEBUG_GENERIC
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


#define PORT_HOOKS 1
#define MEM_HOOKS  0   // not yet implmented in device model
#define IRQ_HOOKS  0   // not yet implemented in device model

struct generic_internal {
  generic_port_range_type    *port_ranges;
  uint_t                     num_port_ranges;
  generic_address_range_type *address_ranges;
  uint_t                     num_address_ranges;
  generic_irq_range_type     *irq_ranges;
  uint_t                     num_irq_ranges;
};


  
#ifdef RAMDISK_BOOT
#include <devices/ramdisk.h>
//ramdisk_state
static struct ramdisk_t *ramdisk_state;
#endif    




int generic_reset_device(struct vm_device * dev)
{
  PrintDebug("generic: reset device\n");
 
  return 0;

}





int generic_start_device(struct vm_device *dev)
{
  PrintDebug("generic: start device\n");
  return 0;
}


int generic_stop_device(struct vm_device *dev)
{
  PrintDebug("generic: stop device\n");
  return 0;
}




int generic_write_port_passthrough(ushort_t port,
				   void * src, 
				   uint_t length,
				   struct vm_device * dev)
{
  uint_t i;

  PrintDebug("generic: writing 0x");

  for (i = 0; i < length; i++) { 
    PrintDebug("%x", ((uchar_t*)src)[i]);
  }
  
  PrintDebug(" to port 0x%x ... ", port);

#ifdef RAMDISK_BOOT

  uint_t err;

  if (((port >= 0x170 && port <= 0x177) || port == 0x376 || port == 0x377)
      && (dev->vm->cpu_mode == REAL)) {

    
    err = ramdisk_state->eops.write_port(port, src, length, dev);

  }else{
#endif

  switch (length) {
  case 1:
    Out_Byte(port,((uchar_t*)src)[0]);
    break;
  case 2:
    Out_Word(port,((ushort_t*)src)[0]);
    break;
  case 4:
    Out_DWord(port,((uint_t*)src)[0]);
    break;
  default:
    for (i = 0; i < length; i++) { 
      Out_Byte(port, ((uchar_t*)src)[i]);
    }
  } //switch length
#ifdef RAMDISK_BOOT
  }//else not ramdisk
#endif
  PrintDebug(" done\n");
  
  return length;
}

int generic_read_port_passthrough(ushort_t port,
				  void * src, 
				  uint_t length,
				  struct vm_device * dev)
{
  uint_t i;

  PrintDebug("generic: reading 0x%x bytes from port 0x%x ...", length, port);

#ifdef RAMDISK_BOOT

  uint_t err;

  if (((port >= 0x170 && port <= 0x177) || port == 0x376 || port == 0x377)
      && (dev->vm->cpu_mode == REAL)) {

    err = ramdisk_state->eops.read_port(port, src, length, dev);

  }else{
#endif

    switch (length) {
    case 1:
      ((uchar_t*)src)[0] = In_Byte(port);
      break;
    case 2:
      ((ushort_t*)src)[0] = In_Word(port);
      break;
    case 4:
      ((uint_t*)src)[0] = In_DWord(port);
      break;
    default:
      for (i = 0; i < length; i++) { 
	((uchar_t*)src)[i] = In_Byte(port);
      }
    }//switch length
#ifdef RAMDISK_BOOT
  }//else not ramdisk
#endif

  PrintDebug(" done ... read 0x");

  for (i = 0; i < length; i++) { 
    PrintDebug("%x", ((uchar_t*)src)[i]);
  }

  PrintDebug("\n");

  return length;
}

int generic_write_port_ignore(ushort_t port,
			      void * src, 
			      uint_t length,
			      struct vm_device * dev)
{
  uint_t i;

  PrintDebug("generic: writing 0x");

  for (i = 0; i < length; i++) { 
    PrintDebug("%x", ((uchar_t*)src)[i]);
  }
  
  PrintDebug(" to port 0x%x ... ", port);

  PrintDebug(" ignored\n");
 
#ifdef RAMDISK_BOOT

  uint_t err;

  if (((port >= 0x3e8 && port <= 0x3ef) || 
       (port >= 0x2e8 && port <= 0x2ef))
      && (dev->vm->cpu_mode == REAL)) {

    err = ramdisk_state->eops.write_port_ignore(port, src, length, dev);
  }
#endif
 
  return length;
}

int generic_read_port_ignore(ushort_t port,
			     void * src, 
			     uint_t length,
			     struct vm_device * dev)
{

  PrintDebug("generic: reading 0x%x bytes from port 0x%x ...", length, port);

  memset((char*)src,0,length);
  PrintDebug(" ignored (return zeroed buffer)\n");

#ifdef RAMDISK_BOOT

  uint_t err;

  if (((port >= 0x3e8 && port <= 0x3ef) || 
       (port >= 0x2e8 && port <= 0x2ef))
      && (dev->vm->cpu_mode == REAL)) {

    err = ramdisk_state->eops.read_port_ignore(port, src, length, dev);
  }
#endif

  return length;
}



int generic_interrupt(uint_t irq,
		      struct vm_device * dev) 
{
  PrintDebug("generic: interrupt 0x%x - injecting into VM\n", irq);

  dev->vm->vm_ops.raise_irq(dev->vm, irq);

  return 0;

}


int generic_init_device(struct vm_device * dev) 
{
  struct generic_internal *state = (struct generic_internal *)(dev->private_data);
  uint_t i, j;

  PrintDebug("generic: init_device\n");

  // Would read state here

  generic_reset_device(dev);

  for (i = 0; i < state->num_port_ranges; i++) { 
    PrintDebug("generic: hooking ports 0x%x to 0x%x as %x\n", state->port_ranges[i][0], state->port_ranges[i][1], state->port_ranges[i][2]==GENERIC_PRINT_AND_PASSTHROUGH ? "print-and-passthrough" : "print-and-ignore");

#if PORT_HOOKS
    for (j = state->port_ranges[i][0]; j <= state->port_ranges[i][1]; j++) { 
      if (state->port_ranges[i][2]==GENERIC_PRINT_AND_PASSTHROUGH) { 
	if (dev_hook_io(dev, j, &generic_read_port_passthrough, &generic_write_port_passthrough)) { 
	  PrintDebug("generic: can't hook port 0x%x (already hooked?)\n", j);
	}
      } else if (state->port_ranges[i][2]==GENERIC_PRINT_AND_IGNORE) { 
	if (dev_hook_io(dev, j, &generic_read_port_ignore, &generic_write_port_ignore)) { 
	  PrintDebug("generic: can't hook port 0x%x (already hooked?)\n", j);
	}
      } 
    }
#else
    PrintDebug("generic: hooking ports not supported\n");
#endif

  }

  for (i = 0; i < state->num_address_ranges; i++) { 
    PrintDebug("generic: hooking addresses 0x%x to 0x%x\n",state->address_ranges[i][0],state->address_ranges[i][1]); 

#if MEM_HOOKS
    if (dev_hook_mem(dev, state->address_ranges[i][0], state->address_ranges[i][1])) {
      PrintDebug("generic: Can't hook addresses 0x%x to 0x%x (already hooked?)\n",
		  state->address_ranges[i][0], state->address_ranges[i][1]); 
    }
#else
    PrintDebug("generic: hooking addresses not supported\n");
#endif

  }

  for (i = 0; i < state->num_irq_ranges; i++) { 
    PrintDebug("generic: hooking irqs 0x%x to 0x%x\n",state->irq_ranges[i][0],state->irq_ranges[i][1]);

#if IRQ_HOOKS
    for (j = state->irq_ranges[i][0]; j <= state->irq_ranges[i][1]; j++) { 
      if (dev_hook_irq(dev, j, &generic_interrupt)) { 
	PrintDebug("generic: can't hook irq  0x%x (already hooked?)\n", j);
      }
    }
#else
    PrintDebug("generic: hooking irqs not supported\n");
#endif

  }

#ifdef RAMDISK_BOOT

  ramdisk_state->cops.init(ramdisk_state, dev);

#endif

  return 0;
}

int generic_deinit_device(struct vm_device *dev)
{
  struct generic_internal *state = (struct generic_internal *)(dev->private_data);
  uint_t i, j;

  PrintDebug("generic: deinit_device\n");

#ifdef RAMDISK_BOOT

  ramdisk_state->cops.close(ramdisk_state);
  
#endif

  for (i = 0; i < state->num_irq_ranges; i++) { 
    PrintDebug("generic: unhooking irqs 0x%x to 0x%x\n", state->irq_ranges[i][0], state->irq_ranges[i][1]);

#if IRQ_HOOKS
    for (j = state->irq_ranges[i][0]; j <= state->irq_ranges[i][1]; j++) { 
      if (dev_unhook_irq(dev, j)) {
	PrintDebug("generic: can't unhook irq 0x%x (already unhooked?)\n",j);
      }
    }
#else
    PrintDebug("generic: unhooking irqs not supported\n");
#endif

  }

  for (i = 0; i < state->num_address_ranges; i++) { 
    PrintDebug("generic: unhooking addresses 0x%x to 0x%x\n",state->address_ranges[i][0],state->address_ranges[i][1]); 

#if MEM_HOOKS
    if (dev_unhook_mem(dev, state->address_ranges[i][0], state->address_ranges[i][1])) {
      PrintDebug("generic: Can't unhook addresses 0x%x to 0x%x (already unhooked?)\n",
		  state->address_ranges[i][0], state->address_ranges[i][1]); 
    }
#else
    PrintDebug("generic: unhooking addresses not supported\n");
#endif

  }

  for (i = 0; i < state->num_port_ranges; i++) { 
    PrintDebug("generic: unhooking ports 0x%x to 0x%x\n",state->port_ranges[i][0],state->port_ranges[i][1]);

#if PORT_HOOKS
    for (j = state->port_ranges[i][0]; j <= state->port_ranges[i][1]; j++) { 
      if (dev_unhook_io(dev, j)) {
	PrintDebug("generic: can't unhook port 0x%x (already unhooked?)\n", j);
      }
    }
#else
    PrintDebug("generic: unhooking ports not supported\n");
#endif

  }

  generic_reset_device(dev);
  return 0;
}





static struct vm_device_ops dev_ops = { 
  .init = generic_init_device, 
  .deinit = generic_deinit_device,
  .reset = generic_reset_device,
  .start = generic_start_device,
  .stop = generic_stop_device,
};




struct vm_device *create_generic(generic_port_range_type    port_ranges[], 
				 generic_address_range_type address_ranges[],
				 generic_irq_range_type     irq_ranges[])
{
  struct generic_internal * generic_state = (struct generic_internal *)V3_Malloc(sizeof(struct generic_internal));
  int i;
  uint_t num_port_ranges, num_address_ranges, num_irq_ranges;

  num_port_ranges=0;
  if (port_ranges!=NULL) { 
    i=0;
    while (port_ranges[i]!=NULL && 
	   !(port_ranges[i][0]==0 && port_ranges[i][1]==0 && port_ranges[i][2]==0)) 
      { num_port_ranges++; i++; }
  }

  
  num_address_ranges=0;
  if (address_ranges!=NULL) { 
    i=0;
    while (address_ranges[i]!=NULL  && 
	   !(address_ranges[i][0]==0 && address_ranges[i][1]==0 && address_ranges[i][2]==0)) 
      { num_address_ranges++; i++; }
  }

  num_irq_ranges=0;
  if (irq_ranges!=NULL) { 
    i=0;
    while (irq_ranges[i]!=NULL && 
	   !(irq_ranges[i][0]==0 && irq_ranges[i][1]==0 && irq_ranges[i][2]==0) ) 
      { num_irq_ranges++; i++; }
  }
    

  generic_state->num_port_ranges = num_port_ranges;

  if (num_port_ranges > 0) { 
    generic_state->port_ranges = V3_Malloc(sizeof(generic_address_range_type) * num_port_ranges);
    memcpy(generic_state->port_ranges, port_ranges, sizeof(generic_port_range_type) * num_port_ranges);
  } else {
    generic_state->port_ranges = NULL;
  }


  generic_state->num_address_ranges = num_address_ranges;

  if (num_address_ranges > 0) { 
    generic_state->address_ranges = V3_Malloc(sizeof(generic_address_range_type) * num_address_ranges);
    memcpy(generic_state->address_ranges, address_ranges, sizeof(generic_address_range_type) * num_address_ranges);
  } else {
    generic_state->address_ranges = NULL;
  }


  generic_state->num_irq_ranges = num_irq_ranges;

  if (num_irq_ranges > 0) { 
    generic_state->irq_ranges = V3_Malloc(sizeof(generic_address_range_type) * num_irq_ranges);
    memcpy(generic_state->irq_ranges, irq_ranges, sizeof(generic_irq_range_type) * num_port_ranges);
  } else {
    generic_state->irq_ranges = NULL;
  }

#ifdef RAMDISK_BOOT

  ramdisk_state = create_ramdisk();
  V3_ASSERT(ramdisk_state != NULL);

#endif

  struct vm_device *device = create_device("GENERIC", &dev_ops, generic_state);

  return device;
}
