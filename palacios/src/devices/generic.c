#include <devices/generic.h>
#include <palacios/vmm.h>
#include <palacios/vmm_types.h>
#include <geekos/io.h>


#define GENERIC_DEBUG 1

#if GENERIC_DEBUG
#define GENERIC_DEBUG_PRINT(first, rest...) PrintDebug(first, ##rest) 
#else
#define GENERIC_DEBUG_PRINT(first, rest...)
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


  
    




int generic_reset_device(struct vm_device * dev)
{
  GENERIC_DEBUG_PRINT("generic: reset device\n");
 
  return 0;

}





int generic_start_device(struct vm_device *dev)
{
  GENERIC_DEBUG_PRINT("generic: start device\n");
  return 0;
}


int generic_stop_device(struct vm_device *dev)
{
  GENERIC_DEBUG_PRINT("generic: stop device\n");
  return 0;
}




int generic_write_port_passthrough(ushort_t port,
				   void * src, 
				   uint_t length,
				   struct vm_device * dev)
{
  uint_t i;

  GENERIC_DEBUG_PRINT("generic: writing 0x");

  for (i = 0; i < length; i++) { 
    GENERIC_DEBUG_PRINT("%x", ((uchar_t*)src)[i]);
  }
  
  GENERIC_DEBUG_PRINT(" to port 0x%x ... ", port);

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
  }

  GENERIC_DEBUG_PRINT(" done\n");
  
  return length;
}

int generic_read_port_passthrough(ushort_t port,
				  void * src, 
				  uint_t length,
				  struct vm_device * dev)
{
  uint_t i;

  GENERIC_DEBUG_PRINT("generic: reading 0x%x bytes from port 0x%x ...", length, port);

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
  }

  GENERIC_DEBUG_PRINT(" done ... read 0x");

  for (i = 0; i < length; i++) { 
    GENERIC_DEBUG_PRINT("%x", ((uchar_t*)src)[i]);
  }

  GENERIC_DEBUG_PRINT("\n");

  return length;
}

int generic_write_port_ignore(ushort_t port,
			      void * src, 
			      uint_t length,
			      struct vm_device * dev)
{
  uint_t i;

  GENERIC_DEBUG_PRINT("generic: writing 0x");

  for (i = 0; i < length; i++) { 
    GENERIC_DEBUG_PRINT("%x", ((uchar_t*)src)[i]);
  }
  
  GENERIC_DEBUG_PRINT(" to port 0x%x ... ", port);

  GENERIC_DEBUG_PRINT(" ignored\n");
  
  return length;
}

int generic_read_port_ignore(ushort_t port,
			     void * src, 
			     uint_t length,
			     struct vm_device * dev)
{

  GENERIC_DEBUG_PRINT("generic: reading 0x%x bytes from port 0x%x ...", length, port);

  memset((char*)src,0,length);
  GENERIC_DEBUG_PRINT(" ignored (return zeroed buffer)\n");

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

  GENERIC_DEBUG_PRINT("generic: init_device\n");

  // Would read state here

  generic_reset_device(dev);

  for (i = 0; i < state->num_port_ranges; i++) { 
    GENERIC_DEBUG_PRINT("generic: hooking ports 0x%x to 0x%x as %x\n", state->port_ranges[i][0], state->port_ranges[i][1], state->port_ranges[i][2]==GENERIC_PRINT_AND_PASSTHROUGH ? "print-and-passthrough" : "print-and-ignore");

#if PORT_HOOKS
    for (j = state->port_ranges[i][0]; j <= state->port_ranges[i][1]; j++) { 
      if (state->port_ranges[i][2]==GENERIC_PRINT_AND_PASSTHROUGH) { 
	if (dev_hook_io(dev, j, &generic_read_port_passthrough, &generic_write_port_passthrough)) { 
	  GENERIC_DEBUG_PRINT("generic: can't hook port 0x%x (already hooked?)\n", j);
	}
      } else if (state->port_ranges[i][2]==GENERIC_PRINT_AND_IGNORE) { 
	if (dev_hook_io(dev, j, &generic_read_port_ignore, &generic_write_port_ignore)) { 
	  GENERIC_DEBUG_PRINT("generic: can't hook port 0x%x (already hooked?)\n", j);
	}
      } 
    }
#else
    GENERIC_DEBUG_PRINT("generic: hooking ports not supported\n");
#endif

  }

  for (i = 0; i < state->num_address_ranges; i++) { 
    GENERIC_DEBUG_PRINT("generic: hooking addresses 0x%x to 0x%x\n",state->address_ranges[i][0],state->address_ranges[i][1]); 

#if MEM_HOOKS
    if (dev_hook_mem(dev, state->address_ranges[i][0], state->address_ranges[i][1])) {
      GENERIC_DEBUG_PRINT("generic: Can't hook addresses 0x%x to 0x%x (already hooked?)\n",
		  state->address_ranges[i][0], state->address_ranges[i][1]); 
    }
#else
    GENERIC_DEBUG_PRINT("generic: hooking addresses not supported\n");
#endif

  }

  for (i = 0; i < state->num_irq_ranges; i++) { 
    GENERIC_DEBUG_PRINT("generic: hooking irqs 0x%x to 0x%x\n",state->irq_ranges[i][0],state->irq_ranges[i][1]);

#if IRQ_HOOKS
    for (j = state->irq_ranges[i][0]; j <= state->irq_ranges[i][1]; j++) { 
      if (dev_hook_irq(dev, j, &generic_interrupt)) { 
	GENERIC_DEBUG_PRINT("generic: can't hook irq  0x%x (already hooked?)\n", j);
      }
    }
#else
    GENERIC_DEBUG_PRINT("generic: hooking irqs not supported\n");
#endif

  }

  return 0;
}

int generic_deinit_device(struct vm_device *dev)
{
  struct generic_internal *state = (struct generic_internal *)(dev->private_data);
  uint_t i, j;

  GENERIC_DEBUG_PRINT("generic: deinit_device\n");

  for (i = 0; i < state->num_irq_ranges; i++) { 
    GENERIC_DEBUG_PRINT("generic: unhooking irqs 0x%x to 0x%x\n", state->irq_ranges[i][0], state->irq_ranges[i][1]);

#if IRQ_HOOKS
    for (j = state->irq_ranges[i][0]; j <= state->irq_ranges[i][1]; j++) { 
      if (dev_unhook_irq(dev, j)) {
	GENERIC_DEBUG_PRINT("generic: can't unhook irq 0x%x (already unhooked?)\n",j);
      }
    }
#else
    GENERIC_DEBUG_PRINT("generic: unhooking irqs not supported\n");
#endif

  }

  for (i = 0; i < state->num_address_ranges; i++) { 
    GENERIC_DEBUG_PRINT("generic: unhooking addresses 0x%x to 0x%x\n",state->address_ranges[i][0],state->address_ranges[i][1]); 

#if MEM_HOOKS
    if (dev_unhook_mem(dev, state->address_ranges[i][0], state->address_ranges[i][1])) {
      GENERIC_DEBUG_PRINT("generic: Can't unhook addresses 0x%x to 0x%x (already unhooked?)\n",
		  state->address_ranges[i][0], state->address_ranges[i][1]); 
    }
#else
    GENERIC_DEBUG_PRINT("generic: unhooking addresses not supported\n");
#endif

  }

  for (i = 0; i < state->num_port_ranges; i++) { 
    GENERIC_DEBUG_PRINT("generic: unhooking ports 0x%x to 0x%x\n",state->port_ranges[i][0],state->port_ranges[i][1]);

#if PORT_HOOKS
    for (j = state->port_ranges[i][0]; j <= state->port_ranges[i][1]; j++) { 
      if (dev_unhook_io(dev, j)) {
	GENERIC_DEBUG_PRINT("generic: can't unhook port 0x%x (already unhooked?)\n", j);
      }
    }
#else
    GENERIC_DEBUG_PRINT("generic: unhooking ports not supported\n");
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
				 uint_t                     num_port_ranges,
				 generic_address_range_type address_ranges[],
				 uint_t                     num_address_ranges,
				 generic_irq_range_type     irq_ranges[],
				 uint_t                     num_irq_ranges)
{
  struct generic_internal * generic_state = (struct generic_internal *)V3_Malloc(sizeof(struct generic_internal));


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


  struct vm_device *device = create_device("GENERIC", &dev_ops, generic_state);

  return device;
}
