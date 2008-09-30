/* (c) 2008, Peter Dinda <pdinda@northwestern.edu> */
/* (c) 2008, The V3VEE Project <http://www.v3vee.org> */


#include <devices/generic.h>
#include <palacios/vmm.h>
#include <palacios/vmm_types.h>
#include <palacios/vmm_list.h>



#ifndef DEBUG_GENERIC
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


#define PORT_HOOKS 1
#define MEM_HOOKS  0   // not yet implmented in device model
#define IRQ_HOOKS  0   // not yet implemented in device model


struct generic_internal {
  struct list_head port_range;
  uint_t num_port_ranges;
  struct list_head mem_range;
  uint_t num_mem_ranges;
  struct list_head irq_range;
  uint_t num_irq_ranges;
};


struct port_range {
  uint_t start;
  uint_t end;
  uint_t type;
  struct list_head range_list;
};

struct mem_range {
  void * start;
  void * end;
  uint_t type;
  struct list_head range_list;
};

struct irq_range {
  uint_t start;
  uint_t end;
  uint_t type;
  struct list_head range_list;
};



int generic_reset_device(struct vm_device * dev)
{
  PrintDebug("generic: reset device\n");
 
  return 0;

}





int generic_start_device(struct vm_device * dev)
{
  PrintDebug("generic: start device\n");
  return 0;
}


int generic_stop_device(struct vm_device * dev)
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


  switch (length) {
  case 1:

    v3_outb(port,((uchar_t*)src)[0]);
    break;
  case 2:
    v3_outw(port,((ushort_t*)src)[0]);
    break;
  case 4:
    v3_outdw(port,((uint_t*)src)[0]);

    break;
  default:
    for (i = 0; i < length; i++) { 
      v3_outb(port, ((uchar_t*)src)[i]);
    }
  } //switch length


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


    switch (length) {
    case 1:
      ((uchar_t*)src)[0] = v3_inb(port);
      break;
    case 2:
      ((ushort_t*)src)[0] = v3_inw(port);
      break;
    case 4:
      ((uint_t*)src)[0] = v3_indw(port);
      break;
    default:
      for (i = 0; i < length; i++) { 
	((uchar_t*)src)[i] = v3_inb(port);
      }
    }//switch length

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
  
  PrintDebug(" to port 0x%x ... ignored\n", port);
 
  return length;
}

int generic_read_port_ignore(ushort_t port,
			     void * src, 
			     uint_t length,
			     struct vm_device * dev)
{

  PrintDebug("generic: reading 0x%x bytes from port 0x%x ...", length, port);

  memset((char*)src, 0, length);
  PrintDebug(" ignored (return zeroed buffer)\n");

  return length;
}



int generic_interrupt(uint_t irq, struct vm_device * dev) {
  PrintDebug("generic: interrupt 0x%x - injecting into VM\n", irq);

  dev->vm->vm_ops.raise_irq(dev->vm, irq);

  return 0;
}


int generic_init_device(struct vm_device * dev) {
  struct generic_internal * state = (struct generic_internal *)(dev->private_data);

  PrintDebug("generic: init_device\n");
  generic_reset_device(dev);


  if (PORT_HOOKS) { // This is a runtime conditional on a #define
    struct port_range * tmp;

    list_for_each_entry(tmp, &(state->port_range), range_list) {
      uint_t i = 0;
      
      PrintDebug("generic: hooking ports 0x%x to 0x%x as %x\n", 
		 tmp->start, tmp->end, 
		 (tmp->type == GENERIC_PRINT_AND_PASSTHROUGH) ? "print-and-passthrough" : "print-and-ignore");
      
      for (i = tmp->start; i <= tmp->end; i++) { 
	if (tmp->type == GENERIC_PRINT_AND_PASSTHROUGH) { 
	  
	  if (dev_hook_io(dev, i, &generic_read_port_passthrough, &generic_write_port_passthrough)) { 
	    PrintDebug("generic: can't hook port 0x%x (already hooked?)\n", i);
	  }
	  
	} else if (tmp->type == GENERIC_PRINT_AND_IGNORE) { 
	  
	  if (dev_hook_io(dev, i, &generic_read_port_ignore, &generic_write_port_ignore)) { 
	    PrintDebug("generic: can't hook port 0x%x (already hooked?)\n", i);
	  }
	} 
      }

    }
  } else {
    PrintDebug("generic: hooking ports not supported\n");
  }



  if (MEM_HOOKS) { // This is a runtime conditional on a #define
    struct mem_range * tmp;

    list_for_each_entry(tmp, &(state->mem_range), range_list) {

      PrintDebug("generic: hooking addresses 0x%x to 0x%x\n", 
		 tmp->start, tmp->end); 
      
      
      if (dev_hook_mem(dev, tmp->start, tmp->end)) {
	PrintDebug("generic: Can't hook addresses 0x%x to 0x%x (already hooked?)\n",
		   tmp->start, tmp->end); 
      }
    }
  } else {
    PrintDebug("generic: hooking addresses not supported\n");
  }




  if (IRQ_HOOKS) { // This is a runtime conditional on a #define
    struct irq_range * tmp;
    
    list_for_each_entry(tmp, &(state->irq_range), range_list) {
      uint_t i;

      PrintDebug("generic: hooking irqs 0x%x to 0x%x\n",
		 tmp->start, tmp->end);
      
      for (i = tmp->start; i <= tmp->end; i++) { 
	if (dev_hook_irq(dev, i, &generic_interrupt)) { 
	  PrintDebug("generic: can't hook irq  0x%x (already hooked?)\n", i);
	}
      }

    }
  } else {
    PrintDebug("generic: hooking irqs not supported\n");
  }



  return 0;
}

int generic_deinit_device(struct vm_device * dev) {
  struct generic_internal * state = (struct generic_internal *)(dev->private_data);


  PrintDebug("generic: deinit_device\n");


  if (IRQ_HOOKS) { // This is a runtime conditional on a #define
    struct irq_range * tmp;
    struct irq_range * cur;
    
    list_for_each_entry_safe(cur, tmp, &(state->irq_range), range_list) {
      uint_t i;

      PrintDebug("generic: unhooking irqs 0x%x to 0x%x\n", 
		 cur->start, cur->end);
      

      for (i = cur->start; i <= cur->end; i++) { 
	if (dev_unhook_irq(dev, i)) {
	  PrintDebug("generic: can't unhook irq 0x%x (already unhooked?)\n", i);
	}
      }

      V3_Free(cur);
    }
  } else {
    PrintDebug("generic: unhooking irqs not supported\n");
  }


  if (MEM_HOOKS) {
    struct mem_range * tmp;
    struct mem_range * cur;
    
    list_for_each_entry_safe(cur, tmp, &(state->mem_range), range_list) {

      PrintDebug("generic: unhooking addresses 0x%x to 0x%x\n",
		 cur->start, cur->end); 

      if (dev_unhook_mem(dev, cur->start, cur->end)) {
	PrintDebug("generic: Can't unhook addresses 0x%x to 0x%x (already unhooked?)\n",
		   cur->start, cur->end); 
      }

      V3_Free(cur);
    }
  } else {
    PrintDebug("generic: unhooking addresses not supported\n");
  }
  

  if (PORT_HOOKS) {
    struct port_range * tmp;
    struct port_range * cur;
    
    list_for_each_entry_safe(cur, tmp, &(state->mem_range), range_list) {
      uint_t i;

      PrintDebug("generic: unhooking ports 0x%x to 0x%x\n",
		   cur->start, cur->end);
		
      for (i = cur->start; i <= cur->end; i++) {
	if (dev_unhook_io(dev, i)) {
	  PrintDebug("generic: can't unhook port 0x%x (already unhooked?)\n", i);
	}
      }

      V3_Free(cur);
    }
  } else {
    PrintDebug("generic: unhooking ports not supported\n");
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




int v3_generic_add_port_range(struct vm_device * dev, uint_t start, uint_t end, uint_t type) {

  if (PORT_HOOKS) {
    struct generic_internal * state = (struct generic_internal *)(dev->private_data);

    struct port_range * range = (struct port_range *)V3_Malloc(sizeof(struct port_range));
    range->start = start;
    range->end = end;
    range->type = type;
    
    list_add(&(state->port_range), &(range->range_list));
    state->num_port_ranges++;
  } else {
    PrintDebug("generic: hooking IO ports not supported\n");
    return -1;
  }

  return 0;
}

int v3_generic_add_mem_range(struct vm_device * dev, void * start, void * end, uint_t type) {

  if (MEM_HOOKS) {
    struct generic_internal * state = (struct generic_internal *)(dev->private_data);
    
    struct mem_range * range = (struct mem_range *)V3_Malloc(sizeof(struct mem_range));
    range->start = start;
    range->end = end;
    range->type = type;
    
    list_add(&(state->port_range), &(range->range_list));
    state->num_mem_ranges++;
  } else {
    PrintDebug("generic: hooking memory not supported\n");
    return -1;
  }

  return 0;
}


int v3_generic_add_irq_range(struct vm_device * dev, uint_t start, uint_t end, uint_t type) {

  if (IRQ_HOOKS) {
    struct generic_internal * state = (struct generic_internal *)(dev->private_data);
    
    struct irq_range * range = (struct irq_range *)V3_Malloc(sizeof(struct irq_range));
    range->start = start;
    range->end = end;
    range->type = type;
    
    list_add(&(state->port_range), &(range->range_list));
    state->num_irq_ranges++;
  } else {
    PrintDebug("generic: hooking IRQs not supported\n");
    return -1;
  }

  return 0;
}



struct vm_device *create_generic() {
  struct generic_internal * generic_state = (struct generic_internal *)V3_Malloc(sizeof(struct generic_internal));
  
  generic_state->num_port_ranges = 0;
  generic_state->num_mem_ranges = 0;
  generic_state->num_irq_ranges = 0;

  INIT_LIST_HEAD(&(generic_state->port_range));
  INIT_LIST_HEAD(&(generic_state->mem_range));
  INIT_LIST_HEAD(&(generic_state->irq_range));
    
  struct vm_device * device = create_device("GENERIC", &dev_ops, generic_state);

  return device;
}
