/* Northwestern University */
/* (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> */

#include <palacios/vm_dev.h>
#include <palacios/vmm.h>



struct vm_device * allocate_device() {

  struct vm_device * dev = NULL;
  dev = (struct vm_device*)V3_Malloc(sizeof(struct vm_device));

  V3_ASSERT(dev != NULL);

  dev->ops = NULL;
  memset(dev->name, 0, 32);
  dev->vm = NULL;
  dev->private_data = NULL;


  INIT_LIST_HEAD(&(dev->io_hooks));
  dev->num_io_hooks = 0;

  INIT_LIST_HEAD(&(dev->mem_hooks));
  dev->num_mem_hooks = 0;
  
  INIT_LIST_HEAD(&(dev->irq_hooks));
  dev->num_irq_hooks = 0;

  return dev;
}

struct vm_device * create_device(char * name, struct vm_device_ops * ops, void * private_data) {
  struct vm_device * dev = allocate_device();

  strncpy(dev->name, name, 32);
  dev->ops = ops;
  dev->private_data = private_data;

  return dev;
}

void free_device(struct vm_device * dev) {
  V3_Free(dev);
}

