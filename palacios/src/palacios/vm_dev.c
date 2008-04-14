#include <palacios/vm_dev.h>
#include <palacios/vmm.h>



struct vm_device * allocate_device() {

  struct vm_device * dev = NULL;
  VMMMalloc(struct vm_device *, dev, sizeof(struct vm_device));

  dev->ops = NULL;
  memset(dev->name, 0, 32);
  dev->vm = NULL;
  dev->private_data = NULL;

  dev->next = NULL;
  dev->prev = NULL;
  dev->io_hooks.head = NULL;
  dev->mem_hooks.head = NULL;
  dev->io_hooks.num_hooks = 0;
  dev->mem_hooks.num_hooks = 0;
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
  VMMFree(dev);
}
