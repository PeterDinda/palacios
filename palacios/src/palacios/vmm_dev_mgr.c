
#include <palacios/vmm_dev.h>
#include <palacios/vmm_dev_mgr.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm.h>

extern struct vmm_os_hooks *os_hooks;

#ifndef NULL
#define NULL 0
#endif

int dev_mgr_init(struct vmm_dev_mgr *mgr, struct guest_info *vm)
{
  mgr->vm=vm;
  mgr->dev_list=NULL;
  mgr->num_devices=0;
  return 0;
}


int dev_mgr_deinit(struct vmm_dev_mgr *mgr)
{
  int rc;

  while (mgr->dev_list) { 
    rc=dev_mgr_detach_device(mgr->vm,mgr->dev_list);
    if (rc) { 
      // Bad bad bad
    }
  }
  return 0;
}

int dev_mgr_attach_device(struct guest_info *vm, struct vm_device *device)
{
  struct vmm_dev_mgr *mgr= &(vm->dev_mgr);
  
  if (device->io_hooks || device->mem_hooks) { 
    return -1;
  }

  device->next = mgr->dev_list;
  device->prev = 0;
  if (device->next) { 
    device->next->prev = device;
  }
  mgr->dev_list = device;
  
  device->vm=vm;

  return 0;
}

int dev_mgr_detach_device(struct guest_info *vm, struct vm_device *device)
{
  if (device->prev==0) { 
    vm->dev_mgr.dev_list = device->next;
  } else {
    device->prev->next = device->next;
  }
  if (device->next) { 
    device->next->prev=device->prev;
  }
  
  // avoid interrupts here

  device->deinit_device(device);

  device->vm=NULL;
  return 0;
}


#define INSERT_FRONT(listhead,item)         \
  do {                                      \
    if (!(listhead)) {                      \
      (listhead)=(item);                    \
      (item)->prev=NULL;                    \
      (item)->next=NULL;                    \
    }  else {                               \
      (item)->prev=NULL;                    \
      (item)->next=(listhead);              \
      if ((listhead)->next) {               \
  	(listhead)->next->prev=(item);      \
      }                                     \
      (listhead)=(item);                    \
    }                                       \
  } while (0)

#define DELETE(listhead,item)               \
  do {                                      \
    if ((item)->prev) {                     \
      (item)->prev->next=(item)->next;      \
    } else {                                \
      (listhead)=(item)->next;              \
    }                                       \
    if ((item)->next) {                     \
      (item)->next->prev=(item)->prev;      \
    }                                       \
  } while (0)
    

   

int dev_mgr_hook_io(struct guest_info    *vm,
		    struct vm_device   *device,
		    ushort_t            portno,
		    enum access_control control,
		    enum access_type    atype)
{
  struct vm_device_io_hook *hook = os_hooks->malloc(sizeof(struct vm_device_io_hook));
  
  if (!hook) { 
    return -1;
  }

  int (*read)(ushort_t, void *, uint_t, void *) = NULL;
  int (*write)(ushort_t, void *, uint_t, void *) = NULL;

  switch (control) { 
  case DEVICE_EMULATED:
    switch (atype) { 
    case DEVICE_READ:
      read = (int (*)(ushort_t, void *,uint_t, void *))  (device->read_io_port);
      break;
    case DEVICE_WRITE:
      write = (int (*)(ushort_t, void *, uint_t, void *)) (device->write_io_port);
      break;
    case DEVICE_READWRITE:
      read = (int (*)(ushort_t, void *, uint_t, void *)) (device->read_io_port);
      write = (int (*)(ushort_t, void *, uint_t, void *)) (device->write_io_port);
      break;
    }
    break;
  case DEVICE_PASSTHROUGH:
    read=write=NULL;
    break;
  }
    
  hook_io_port(&(vm->io_map), 
	       portno, 
	       read,
	       write,
	       device);

  hook->control=control;
  hook->atype=atype;
  hook->guest_port = portno;
  
  INSERT_FRONT(device->io_hooks,hook);
  
  return 0;
}


int dev_mgr_unhook_io(struct guest_info    *vm,
		      struct vm_device   *device,
		      ushort_t            portno)
{
  struct vm_device_io_hook *hook = device->io_hooks;

  while (hook) { 
    if (hook->guest_port==portno) { 
      DELETE(device->io_hooks,hook);
      break;
    }
  }

  if (!hook) { 
    // Very bad - unhooking something that doesn't exist!
    return -1;
  }

  return unhook_io_port(&(vm->io_map),
			portno);
}



int dev_mgr_hook_mem(struct guest_info    *vm,
		     struct vm_device   *device,
		     void               *guest_physical_address_start,
		     void               *guest_physical_address_end,
		     enum access_control control,
		     enum access_type    atype)
{

  struct vm_device_mem_hook *hook = os_hooks->malloc(sizeof(struct vm_device_mem_hook));
  
  if (!hook) { 
    return -1;
  }

  int (*read)(ushort_t, void *, uint_t, void *) = NULL;
  int (*write)(ushort_t, void *, uint_t, void *) = NULL;

  switch (control) { 
  case DEVICE_EMULATED:
    switch (atype) { 
    case DEVICE_READ:
      read = (int (*)(ushort_t, void *, uint_t, void *))(device->read_mapped_memory);
      break;
    case DEVICE_WRITE:
      write = (int (*)(ushort_t, void *, uint_t, void *))(device->write_mapped_memory);
      break;
    case DEVICE_READWRITE:
      read = (int (*)(ushort_t, void *, uint_t, void *))(device->read_mapped_memory);
      write = (int (*)(ushort_t, void *, uint_t, void *))(device->write_mapped_memory);
      break;
    }
    break;
  case DEVICE_PASSTHROUGH:
    read=write=NULL;
    break;
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

  hook->control=control;
  hook->atype=atype;
  hook->guest_physical_start = guest_physical_address_start;
  hook->guest_physical_end = guest_physical_address_end;

  
  INSERT_FRONT(device->mem_hooks,hook);

  return 0;
  
}


int dev_mgr_unhook_mem(struct guest_info    *vm,
		       struct vm_device   *device,
		       void               *guest_physical_start,
		       void               *guest_physical_end) 
{
  struct vm_device_mem_hook *hook = device->mem_hooks;

  while (hook) { 
    if (hook->guest_physical_start==guest_physical_start &&
	hook->guest_physical_end==guest_physical_end) {
      DELETE(device->mem_hooks,hook);
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


int dev_mgr_unhook_device(struct guest_info  *vm,
			  struct vm_device *device)
{
  struct vm_device_io_hook *iohook=device->io_hooks;
  struct vm_device_mem_hook *memhook=device->mem_hooks;

  while (iohook) { 
    if (dev_mgr_unhook_io(vm,device,iohook->guest_port)) { 
      return -1;
    }
  }

  while (memhook) { 
    if (dev_mgr_unhook_mem(vm,device,memhook->guest_physical_start, memhook->guest_physical_end)) {
      return -1;
    }
  }

  return 0;
}

