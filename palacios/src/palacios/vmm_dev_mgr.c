#include <palacios/vm_dev.h>
#include <palacios/vmm_dev_mgr.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm.h>
#include <palacios/vmm_irq.h>

extern struct vmm_os_hooks *os_hooks;

#ifndef NULL
#define NULL 0
#endif

int dev_mgr_init(struct vmm_dev_mgr * mgr) {

  INIT_LIST_HEAD(&(mgr->dev_list));
  mgr->num_devs = 0;

  INIT_LIST_HEAD(&(mgr->io_hooks));
  mgr->num_io_hooks = 0;
  return 0;
}


int dev_mgr_deinit(struct vmm_dev_mgr * mgr) {
  struct vm_device * dev;

  list_for_each_entry(dev, &(mgr->dev_list), dev_link) {
    unattach_device(dev);
    free_device(dev);
  }

  return 0;
}




int dev_mgr_add_device(struct vmm_dev_mgr * mgr, struct vm_device * dev) {
  list_add(&(dev->dev_link), &(mgr->dev_list));
  mgr->num_devs++;

  return 0;
}

int dev_mgr_remove_device(struct vmm_dev_mgr * mgr, struct vm_device * dev) {
  list_del(&(dev->dev_link));
  mgr->num_devs--;

  return 0;
}



/* IO HOOKS */
int dev_mgr_add_io_hook(struct vmm_dev_mgr * mgr, struct dev_io_hook * hook) {
  list_add(&(hook->mgr_list), &(mgr->io_hooks));
  mgr->num_io_hooks++;
  return 0;
}


int dev_mgr_remove_io_hook(struct vmm_dev_mgr * mgr, struct dev_io_hook * hook) {
  list_del(&(hook->mgr_list));
  mgr->num_io_hooks--;

  return 0;
}


int dev_add_io_hook(struct vm_device * dev, struct dev_io_hook * hook) {
  list_add(&(hook->dev_list), &(dev->io_hooks));
  dev->num_io_hooks++;
  return 0;
}


int dev_remove_io_hook(struct vm_device * dev, struct dev_io_hook * hook) {
  list_del(&(hook->dev_list));
  dev->num_io_hooks--;

  return 0;
}





struct dev_io_hook * dev_mgr_find_io_hook(struct vmm_dev_mgr * mgr, ushort_t port) {
  struct dev_io_hook * tmp;

  list_for_each_entry(tmp, &(mgr->io_hooks), mgr_list) {
    if (tmp->port == port) {
      return tmp;
    }
  }
  return NULL;
}

struct dev_io_hook * dev_find_io_hook(struct vm_device * dev, ushort_t port) {
  struct dev_io_hook * tmp;

  list_for_each_entry(tmp, &(dev->io_hooks), dev_list) {
    if (tmp->port == port) {
      return tmp;
    }
  }
  return NULL;
}




int dev_hook_io(struct vm_device   *dev,
		ushort_t            port,
		int (*read)(ushort_t port, void * dst, uint_t length, struct vm_device * dev),
		int (*write)(ushort_t port, void * src, uint_t length, struct vm_device * dev)) {

  struct dev_io_hook *hook = os_hooks->malloc(sizeof(struct dev_io_hook));
  
  if (!hook) { 
    return -1;
  }


  if (hook_io_port(&(dev->vm->io_map), port, 
		   (int (*)(ushort_t, void *, uint_t, void *))read, 
		   (int (*)(ushort_t, void *, uint_t, void *))write, 
		   (void *)dev) == 0) {

    hook->dev = dev;
    hook->port = port;
    hook->read = read;
    hook->write = write;
    
    dev_mgr_add_io_hook(&(dev->vm->dev_mgr), hook);
    dev_add_io_hook(dev, hook);
  } else {
    return -1;
  }

  return 0;
}


int dev_unhook_io(struct vm_device   *dev,
		  ushort_t            port) {

  struct vmm_dev_mgr * mgr = &(dev->vm->dev_mgr);
  struct dev_io_hook * hook = dev_mgr_find_io_hook(mgr, port);

  if (!hook) { 
    return -1;
  }

  dev_mgr_remove_io_hook(mgr, hook);
  dev_remove_io_hook(dev, hook);

  return unhook_io_port(&(dev->vm->io_map), port);
}




/* IRQ HOOKS */
/*
int dev_mgr_add_irq_hook(struct vmm_dev_mgr * mgr, struct dev_irq_hook * hook) {
  list_add(&(hook->mgr_list), &(mgr->irq_hooks));
  mgr->num_irq_hooks++;
  return 0;
}


int dev_mgr_remove_irq_hook(struct vmm_dev_mgr * mgr, struct dev_irq_hook * hook) {
  list_del(&(hook->mgr_list));
  mgr->num_irq_hooks--;

  return 0;
}


int dev_add_irq_hook(struct vm_device * dev, struct dev_irq_hook * hook) {
  list_add(&(hook->dev_list), &(dev->irq_hooks));
  dev->num_irq_hooks++;
  return 0;
}


int dev_remove_irq_hook(struct vm_device * dev, struct dev_irq_hook * hook) {
  list_del(&(hook->dev_list));
  dev->num_irq_hooks--;

  return 0;
}





struct dev_irq_hook * dev_mgr_find_irq_hook(struct vmm_dev_mgr * mgr, uint_t irq) {
  struct dev_irq_hook * tmp;

  list_for_each_entry(tmp, &(mgr->irq_hooks), mgr_list) {
    if (tmp->irq == irq) {
      return tmp;
    }
  }
  return NULL;
}

struct dev_irq_hook * dev_find_irq_hook(struct vm_device * dev, uint_t irq) {
  struct dev_irq_hook * tmp;

  list_for_each_entry(tmp, &(dev->irq_hooks), dev_list) {
    if (tmp->irq == irq) {
      return tmp;
    }
  }
  return NULL;
}




int dev_hook_irq(struct vm_device   *dev,
		 uint_t irq,
		 int (*handler)(uint_t irq, struct vm_device * dev)) {

  struct dev_irq_hook *hook = os_hooks->malloc(sizeof(struct dev_irq_hook));
  
  if (!hook) { 
    return -1;
  }


  if (hook_irq(&(dev->vm->irq_map), irq, 
	       (int (*)(uint_t, void *))handler, 
	       (void *)dev) == 0) {

    hook->dev = dev;
    hook->irq = irq;
    hook->handler = handler;
    
    dev_mgr_add_irq_hook(&(dev->vm->dev_mgr), hook);
    dev_add_irq_hook(dev, hook);
  } else {
    return -1;
  }

  return 0;
}


int dev_unhook_irq(struct vm_device * dev,
		   uint_t irq) {

  struct vmm_dev_mgr * mgr = &(dev->vm->dev_mgr);
  struct dev_irq_hook * hook = dev_mgr_find_irq_hook(mgr, irq);

  if (!hook) { 
    return -1;
  }

  dev_mgr_remove_irq_hook(mgr, hook);
  dev_remove_irq_hook(dev, hook);

  return unhook_irq(&(dev->vm->irq_map), irq);
}


*/





int attach_device(struct guest_info * vm, struct vm_device * dev) {
  struct vmm_dev_mgr *mgr= &(vm->dev_mgr);
  
  dev->vm = vm;
  dev_mgr_add_device(mgr, dev);
  dev->ops->init(dev);

  return 0;
}

int unattach_device(struct vm_device * dev) {
  struct vmm_dev_mgr * mgr = &(dev->vm->dev_mgr);

  dev->ops->deinit(dev);
  dev_mgr_remove_device(mgr, dev);
  dev->vm = NULL;

  return 0;
}





int dev_mgr_hook_mem(struct guest_info    *vm,
		     struct vm_device   *device,
		     void               *start,
		     void               *end)
{

  struct dev_mem_hook *hook;
  V3_Malloc(struct dev_mem_hook *, hook,sizeof(struct dev_mem_hook));
  
  if (!hook) { 
    return -1;
  }


    

  /* not implemented yet
  hook_memory(vm->mem_map, 
	      guest_physical_address_start, 
	      guest_physical_address_end, 
	      read,
	      write,
	      device);

  */

  return -1;   // remove when hook_memory works


  hook->addr_start = start;
  hook->addr_end = end;

  return 0;
  
}


int dev_mgr_unhook_mem(struct vm_device   *dev,
		       addr_t start,
		       addr_t end)  {
  /*
  struct vmm_dev_mgr * mgr = &(dev->vm->dev_mgr);
  struct dev_mem_hook *hook = dev_mgr_find_mem_hook(mgr, start, end);
  
  if (!hook) { 
    // Very bad - unhooking something that doesn't exist!
    return -1;
  }
  */

  /* not implemented yet
  return unhook_mem_port(vm->mem_map,
			 guest_physical_start,
		         guest_physical_end) ;

  */
  return -1;
}




void PrintDebugDevMgr(struct vmm_dev_mgr * mgr) {
  struct vm_device * dev;
  PrintDebug("%d devices registered with manager\n", mgr->num_devs);

  list_for_each_entry(dev, &(mgr->dev_list), dev_link) {
    PrintDebugDev(dev);
    PrintDebug("next..\n");
  }

  return;
}


void PrintDebugDev(struct vm_device * dev) {
  
  PrintDebug("Device: %s\n", dev->name);
  PrintDebugDevIO(dev);
}

void PrintDebugDevMgrIO(struct vmm_dev_mgr * mgr) {

}

void PrintDebugDevIO(struct vm_device * dev) {
  struct dev_io_hook * hook;

  PrintDebug("IO Hooks(%d)  for Device: %s\n", dev->num_io_hooks,  dev->name);

  list_for_each_entry(hook, &(dev->io_hooks), dev_list) {
    PrintDebug("\tPort: 0x%x (read=0x%x), (write=0x%x)\n", hook->port, hook->read, hook->write);
  }

  return;
}
