#ifndef _vmm_dev_mgr
#define _vmm_dev_mgr

#include <palacios/vmm_types.h>
#include <palacios/vmm_device_types.h>

struct vm_device;
struct guest_info;


struct vmm_dev_mgr {
  struct guest_info   *vm;
  struct vm_device  *dev_list;
  uint_t             num_devices;
};


enum access_control {DEVICE_EMULATED, DEVICE_PASSTHROUGH} ;
enum access_type {DEVICE_READ, DEVICE_WRITE, DEVICE_READWRITE} ;

struct vm_device_io_hook {
  enum access_control control;
  enum access_type    atype;

  ushort_t            guest_port;

  // Do not touch anything below this
  
  struct vm_device_io_hook *next, *prev;
};

struct vm_device_mem_hook {
  enum access_control control;
  enum access_type    atype;

  void             *guest_physical_start;
  void             *guest_physical_end;

  // Do not touch anything below this
  
  struct vm_device_mem_hook *next, *prev;

};



// Registration of devices

//
// The following device manager functions should only be called
// when the guest is stopped
//

int dev_mgr_init(struct vmm_dev_mgr *mgr, struct guest_info *vm);
int dev_mgr_deinit(struct vmm_dev_mgr *mgr);

int dev_mgr_attach_device(struct guest_info *vm, 
			  struct vm_device *device);
int dev_mgr_detach_device(struct guest_info *vm, 
			  struct vm_device *device);


int dev_mgr_hook_io(struct guest_info    *vm,
		    struct vm_device   *device,
		    ushort_t            portno,
		    enum access_control control,
		    enum access_type    atype);

int dev_mgr_unhook_io(struct guest_info    *vm,
		      struct vm_device   *device,
		      ushort_t            portno);

int dev_mgr_hook_mem(struct guest_info    *vm,
		     struct vm_device   *device,
		     void               *guest_physical_address_start,
		     void               *guest_physical_address_end,
		     enum access_control control,
		     enum access_type    atype);

int dev_mgr_unhook_mem(struct guest_info    *vm,
		       struct vm_device   *device,
		       void               *guest_physical_address_start,
		       void               *guest_physical_address_end);

//
// Strictly a helper - the device is resposible for unhooking on disconnect
//

int dev_mgr_unhook_device(struct guest_info  *vm,
			  struct vm_device *device);


#endif
