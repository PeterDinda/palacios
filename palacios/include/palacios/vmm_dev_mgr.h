#ifndef _VMM_DEV_MGR
#define _VMM_DEV_MGR

#include <palacios/vmm_types.h>
#include <palacios/vmm_device_types.h>
#include <palacios/vmm_list.h>
#include <palacios/vmm_string.h>


struct vm_device;
struct guest_info;

struct vm_dev_list {
  struct vm_device * head;
  uint_t num_devs;
};


struct dev_io_hook_list {
  struct dev_io_hook * head;
  uint_t num_hooks;
};


struct dev_mem_hook_list {
  struct dev_mem_hook * head;
  uint_t num_hooks;
};



struct vmm_dev_mgr {
  struct vm_dev_list dev_list;
  struct dev_io_hook_list io_hooks;
  struct dev_mem_hook_list mem_hooks;
};



struct dev_io_hook {
  ushort_t port;
  
  int (*read)(ushort_t port, void * dst, uint_t length, struct vm_device * dev);
  int (*write)(ushort_t port, void * src, uint_t length, struct vm_device * dev);

  struct vm_device * dev;

  // Do not touch anything below this  
  struct dev_io_hook *dev_next, *dev_prev;
  struct dev_io_hook *mgr_next, *mgr_prev;
};

struct dev_mem_hook {
  void             *addr_start;
  void             *addr_end;

  // Do not touch anything below this
  struct dev_mem_hook *dev_next, *dev_prev;
  struct dev_mem_hook *mgr_next, *mgr_prev;
};



// Registration of devices

//
// The following device manager functions should only be called
// when the guest is stopped
//

int dev_mgr_init(struct vmm_dev_mgr *mgr);
int dev_mgr_deinit(struct vmm_dev_mgr * mgr);



int attach_device(struct guest_info *vm, struct vm_device * dev);
int unattach_device(struct vm_device *dev);


int dev_mgr_add_device(struct vmm_dev_mgr * mgr, struct vm_device * dev);
int dev_mgr_remove_device(struct vmm_dev_mgr * mgr, struct vm_device * dev);


void PrintDebugDevMgr(struct vmm_dev_mgr * mgr);
void PrintDebugDev(struct vm_device * dev);
void PrintDebugDevIO(struct vm_device * dev);
void PrintDebugDevMgrIO(struct vmm_dev_mgr * mgr);

#endif
