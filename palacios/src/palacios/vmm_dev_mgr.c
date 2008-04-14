#include <palacios/vm_dev.h>
#include <palacios/vmm_dev_mgr.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm.h>


extern struct vmm_os_hooks *os_hooks;

#ifndef NULL
#define NULL 0
#endif

int dev_mgr_init(struct vmm_dev_mgr * mgr) {
  mgr->dev_list.head = NULL;
  mgr->dev_list.num_devs = 0;
  return 0;
}


int dev_mgr_deinit(struct vmm_dev_mgr * mgr)
{
  struct vm_device * dev = mgr->dev_list.head;

  while (dev) { 
    unattach_device(dev);
    free_device(dev);
    dev = dev->next;
  }
  return 0;
}




int dev_mgr_add_device(struct vmm_dev_mgr * mgr, struct vm_device * dev) {
  dev->next = mgr->dev_list.head;
  dev->prev = 0;
  if (dev->next) { 
    dev->next->prev = dev;
  }
  mgr->dev_list.head = dev;

  mgr->dev_list.num_devs++;

  return 0;
}

int dev_mgr_remove_device(struct vmm_dev_mgr * mgr, struct vm_device * dev) {
  if (mgr->dev_list.head == dev) { 
    mgr->dev_list.head = dev->next;
  } else {
    dev->prev->next = dev->next;
  }
  if (dev->next) { 
    dev->next->prev = dev->prev;
  }
  
  mgr->dev_list.num_devs--;

  return 0;
}


int dev_mgr_add_io_hook(struct vmm_dev_mgr * mgr, struct dev_io_hook * hook) {
  hook->mgr_next = mgr->io_hooks.head;
  hook->mgr_prev = NULL;
  if (hook->mgr_next) {
    hook->mgr_next->mgr_prev = hook;
  }
  mgr->io_hooks.head = hook;

  mgr->io_hooks.num_hooks++;

  return 0;
}


int dev_mgr_remove_io_hook(struct vmm_dev_mgr * mgr, struct dev_io_hook * hook) {
  if (mgr->io_hooks.head == hook) {
    mgr->io_hooks.head = hook->mgr_next;
  } else {
    hook->mgr_prev->mgr_next = hook->mgr_next;
  }

  if (hook->mgr_next) {
    hook->mgr_next->mgr_prev = hook->mgr_prev;
  }
  
  mgr->io_hooks.num_hooks--;

  return 0;
}


int dev_add_io_hook(struct vm_device * dev, struct dev_io_hook * hook) {
  hook->dev_next = dev->io_hooks.head;
  hook->dev_prev = NULL;
  if (hook->dev_next) {
    hook->dev_next->dev_prev = hook;
  }
  dev->io_hooks.head = hook;

  dev->io_hooks.num_hooks++;

  return 0;
}


int dev_remove_io_hook(struct vm_device * dev, struct dev_io_hook * hook) {
  if (dev->io_hooks.head == hook) {
    dev->io_hooks.head = hook->dev_next;
  } else {
    hook->dev_prev->dev_next = hook->dev_next;
  }

  if (hook->dev_next) {
    hook->dev_next->dev_prev = hook->dev_prev;
  }
  
  dev->io_hooks.num_hooks--;

  return 0;
}


struct dev_io_hook * dev_mgr_find_io_hook(struct vmm_dev_mgr * mgr, ushort_t port) {
  struct dev_io_hook * tmp = mgr->io_hooks.head;

  while (tmp) {
    if (tmp->port == port) {
      break;
    }
    tmp = tmp->mgr_next;
  }

  return tmp;
}

struct dev_io_hook * dev_find_io_hook(struct vm_device * dev, ushort_t port) {
  struct dev_io_hook * tmp = dev->io_hooks.head;

  while (tmp) {
    if (tmp->port == port) {
      break;
    }
    tmp = tmp->dev_next;
  }

  return tmp;
}




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



int dev_mgr_hook_mem(struct guest_info    *vm,
		     struct vm_device   *device,
		     void               *start,
		     void               *end)
{

  struct dev_mem_hook *hook = os_hooks->malloc(sizeof(struct dev_mem_hook));
  
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


int dev_mgr_unhook_mem(struct guest_info    *vm,
		       struct vm_device   *device,
		       void               *start,
		       void               *end) 
{
  struct dev_mem_hook *hook = device->mem_hooks.head;

  while (hook) { 
    if (((hook->addr_start) == start) && (hook->addr_end == end)) {
      break;
    }
  }

  if (!hook) { 
    // Very bad - unhooking something that doesn't exist!
    return -1;
  }


  /* not implemented yet
  return unhook_mem_port(vm->mem_map,
			 guest_physical_start,
		         guest_physical_end) ;

  */
  return -1;
}




void PrintDebugDevMgr(struct vmm_dev_mgr * mgr) {
  struct vm_device * dev = mgr->dev_list.head;

  while (dev) {
    PrintDebugDev(dev);
    dev = dev->next;
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
  struct dev_io_hook * hook = dev->io_hooks.head;
  
  PrintDebug("IO Hooks (%d) for Device: %s\n", dev->io_hooks.num_hooks, dev->name);
  
  while (hook) {
    PrintDebug("\tPort: 0x%x (read=0x%x), (write=0x%x)\n", hook->port, hook->read, hook->write);
    hook = hook->dev_next;
  }

  return;
}
