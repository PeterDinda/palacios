/* Northwestern University */
/* (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> */

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

  struct list_head dev_link;


  uint_t num_io_hooks;
  struct list_head io_hooks;
  uint_t num_mem_hooks;
  struct list_head mem_hooks;
  uint_t num_irq_hooks;
  struct list_head irq_hooks;

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


int dev_hook_irq(struct vm_device * dev,
		 uint_t irq, 
		 int (*handler)(uint_t irq, struct vm_device * dev));
int dev_unhook_irq(struct vm_device * dev, uint_t irq);


#endif
