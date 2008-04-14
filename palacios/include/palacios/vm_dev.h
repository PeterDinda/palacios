#ifndef __VM_DEV_H
#define __VM_DEV_H

#include <palacios/vmm_types.h>
#include <palacios/vmm_list.h>
#include <palacios/vmm_dev_mgr.h>

struct guest_info;


struct vm_device;


struct vm_device_ops {
  int (*init)(struct vm_device *dev);
  int (*deinit)(struct vm_device *dev);


  int (*reset)(struct vm_device *dev);

  int (*start)(struct vm_device *dev);
  int (*stop)(struct vm_device *dev);


  //int (*save)(struct vm_device *dev, struct *iostream);
  //int (*restore)(struct vm_device *dev, struct *iostream);
};



struct vm_device {
  char name[32];
  
  void *private_data;

  struct vm_device_ops * ops;

  struct guest_info * vm;

  struct vm_device   *next, *prev;


  struct dev_io_hook_list io_hooks;
  struct dev_mem_hook_list mem_hooks;
};



struct vm_device * allocate_device();
struct vm_device * create_device(char * name, struct vm_device_ops * ops, void * private_data);
void free_device(struct vm_device * dev);



int dev_hook_io(struct vm_device   *dev,
		ushort_t            port,
		int (*read)(ushort_t port, void * dst, uint_t length, struct vm_device * dev),
		int (*write)(ushort_t port, void * src, uint_t length, struct vm_device * dev));

int dev_unhook_io(struct vm_device   *dev,
		  ushort_t            port);

int dev_hook_mem(struct vm_device   *dev,
		 void               *start,
		 void               *end);

int dev_unhook_mem(struct vm_device   * dev,
		   void               * start,
		   void               * end);





#endif
