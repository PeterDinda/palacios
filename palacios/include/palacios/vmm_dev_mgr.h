#ifndef _VMM_DEV_MGR
#define _VMM_DEV_MGR

#include <palacios/vmm_types.h>
#include <palacios/vmm_list.h>
#include <palacios/vmm_string.h>

struct vm_device;
struct guest_info;

struct vmm_dev_mgr {
  uint_t num_devs;
  struct list_head dev_list;

  uint_t num_io_hooks;
  struct list_head io_hooks;
  
  uint_t num_mem_hooks;
  struct list_head mem_hooks;

};


int dev_mgr_init(struct vmm_dev_mgr *mgr);
int dev_mgr_deinit(struct vmm_dev_mgr * mgr);


// Registration of devices

//
// The following device manager functions should only be called
// when the guest is stopped
//

int v3_attach_device(struct guest_info *vm, struct vm_device * dev);
int v3_unattach_device(struct vm_device *dev);


void PrintDebugDevMgr(struct vmm_dev_mgr * mgr);

#ifdef __V3VEE__

struct dev_io_hook {
  ushort_t port;
  
  int (*read)(ushort_t port, void * dst, uint_t length, struct vm_device * dev);
  int (*write)(ushort_t port, void * src, uint_t length, struct vm_device * dev);

  struct vm_device * dev;

  // Do not touch anything below this  

  struct list_head dev_list;
  struct list_head mgr_list;
};

struct dev_mem_hook {
  void  *addr_start;
  void  *addr_end;

  struct vm_device * dev;

  // Do not touch anything below this
  struct list_head dev_list;
  struct list_head mgr_list;
};




void PrintDebugDev(struct vm_device * dev);
void PrintDebugDevIO(struct vm_device * dev);
void PrintDebugDevMgrIO(struct vmm_dev_mgr * mgr);

#endif // ! __V3VEE__

#endif
