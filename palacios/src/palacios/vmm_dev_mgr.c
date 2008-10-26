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

#include <palacios/vm_dev.h>
#include <palacios/vmm_dev_mgr.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm.h>
#include <palacios/vmm_decoder.h>


#ifndef DEBUG_DEV_MGR
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif



int v3_init_dev_mgr(struct guest_info * info) {
  struct vmm_dev_mgr * mgr = &(info->dev_mgr);
  INIT_LIST_HEAD(&(mgr->dev_list));
  mgr->num_devs = 0;

  INIT_LIST_HEAD(&(mgr->io_hooks));
  mgr->num_io_hooks = 0;

  return 0;
}


int v3_dev_mgr_deinit(struct guest_info * info) {
  struct vm_device * dev;
  struct vmm_dev_mgr * mgr = &(info->dev_mgr);
  struct vm_device * tmp;

  list_for_each_entry_safe(dev, tmp, &(mgr->dev_list), dev_link) {
    v3_unattach_device(dev);
    v3_free_device(dev);
  }

  return 0;
}




static int dev_mgr_add_device(struct vmm_dev_mgr * mgr, struct vm_device * dev) {
  list_add(&(dev->dev_link), &(mgr->dev_list));
  mgr->num_devs++;

  return 0;
}

static int dev_mgr_remove_device(struct vmm_dev_mgr * mgr, struct vm_device * dev) {
  list_del(&(dev->dev_link));
  mgr->num_devs--;

  return 0;
}



/* IO HOOKS */
static int dev_mgr_add_io_hook(struct vmm_dev_mgr * mgr, struct dev_io_hook * hook) {
  list_add(&(hook->mgr_list), &(mgr->io_hooks));
  mgr->num_io_hooks++;
  return 0;
}


static int dev_mgr_remove_io_hook(struct vmm_dev_mgr * mgr, struct dev_io_hook * hook) {
  list_del(&(hook->mgr_list));
  mgr->num_io_hooks--;

  return 0;
}


static int dev_add_io_hook(struct vm_device * dev, struct dev_io_hook * hook) {
  list_add(&(hook->dev_list), &(dev->io_hooks));
  dev->num_io_hooks++;
  return 0;
}


static int dev_remove_io_hook(struct vm_device * dev, struct dev_io_hook * hook) {
  list_del(&(hook->dev_list));
  dev->num_io_hooks--;

  return 0;
}





static struct dev_io_hook * dev_mgr_find_io_hook(struct vmm_dev_mgr * mgr, ushort_t port) {
  struct dev_io_hook * tmp = NULL;

  list_for_each_entry(tmp, &(mgr->io_hooks), mgr_list) {
    if (tmp->port == port) {
      return tmp;
    }
  }
  return NULL;
}


/*
static struct dev_io_hook * dev_find_io_hook(struct vm_device * dev, ushort_t port) {
  struct dev_io_hook * tmp = NULL;

  list_for_each_entry(tmp, &(dev->io_hooks), dev_list) {
    if (tmp->port == port) {
      return tmp;
    }
  }
  return NULL;
}
*/



int v3_dev_hook_io(struct vm_device   *dev,
		   ushort_t            port,
		   int (*read)(ushort_t port, void * dst, uint_t length, struct vm_device * dev),
		   int (*write)(ushort_t port, void * src, uint_t length, struct vm_device * dev)) {
  
  struct dev_io_hook *hook = (struct dev_io_hook *)V3_Malloc(sizeof(struct dev_io_hook));
  
  if (!hook) { 
    return -1;
  }


  if (v3_hook_io_port(dev->vm, port, 
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


int v3_dev_unhook_io(struct vm_device   *dev,
		  ushort_t            port) {

  struct vmm_dev_mgr * mgr = &(dev->vm->dev_mgr);
  struct dev_io_hook * hook = dev_mgr_find_io_hook(mgr, port);

  if (!hook) { 
    return -1;
  }

  dev_mgr_remove_io_hook(mgr, hook);
  dev_remove_io_hook(dev, hook);

  return v3_unhook_io_port(dev->vm, port);
}


int v3_attach_device(struct guest_info * vm, struct vm_device * dev) {
  struct vmm_dev_mgr *mgr= &(vm->dev_mgr);
  
  dev->vm = vm;
  dev_mgr_add_device(mgr, dev);
  dev->ops->init(dev);

  return 0;
}

int v3_unattach_device(struct vm_device * dev) {
  struct vmm_dev_mgr * mgr = &(dev->vm->dev_mgr);

  dev->ops->deinit(dev);
  dev_mgr_remove_device(mgr, dev);
  dev->vm = NULL;

  return 0;
}




#if 0
static int dev_mgr_hook_mem(struct guest_info    *vm,
			    struct vm_device   *device,
			    void               *start,
			    void               *end)
{

  struct dev_mem_hook * hook = (struct dev_mem_hook*)V3_Malloc(sizeof(struct dev_mem_hook));
  //  V3_Malloc(struct dev_mem_hook *, hook,sizeof(struct dev_mem_hook));

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


static int dev_mgr_unhook_mem(struct vm_device   *dev,
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
#endif


#ifdef DEBUG_DEV_MGR

void PrintDebugDevMgr(struct guest_info * info) {
  struct vmm_dev_mgr * mgr = &(info->dev_mgr);
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
    PrintDebug("\tPort: 0x%x (read=0x%p), (write=0x%p)\n", hook->port, 
	       (void *)(addr_t)(hook->read), 
	       (void *)(addr_t)(hook->write));
  }

  return;
}

#else 
void PrintDebugDevMgr(struct guest_info * info) {}
void PrintDebugDev(struct vm_device * dev) {}
void PrintDebugDevMgrIO(struct vmm_dev_mgr * mgr) {}
void PrintDebugDevIO(struct vm_device * dev) {}
#endif
