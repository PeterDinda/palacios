#ifndef __VMM_DEV_H
#define __VMM_DEV_H

#include <palacios/vmm_types.h>

struct vm_guest;
struct vm_device_io_hook;
struct vm_device_mem_hook;


//
// This structure defines an abstract io and/or memory-mapped device
// It currently does not define the interaction with actual hardware
//



struct vm_device {
  int (*init_device)(struct vm_device *dev, struct vm_guest *vm);
  int (*deinit_device)(struct vm_device *dev);


  int (*reset_device)(struct vm_device *dev);

  int (*start_device)(struct vm_device *dev);
  int (*stop_device)(struct vm_device *dev);


  //
  // To understand how to register these callbacks
  // see vmm_dev_mgr.h
  //
  // Note that callbacks like these are only called
  // when the port/memory is hooked as EMULATED
  //


  //
  // If your device is I/O mapped, this function will
  // be called on an I/O read
  //

  int (*read_io_port)(ushort_t port_read,
		      void   *address, 
		      uint_t length,
		      struct vm_device *dev);

  //
  // If your device is I/O mapped, this function will
  // be called on an I/O write
  //

  int (*write_io_port)(ushort_t port_written,
		       void *address, 
		       uint_t length,
		       struct vm_device *dev);


  //
  // If your device is memory mapped, this function will
  // be called on an memory read
  //

  int (*read_mapped_memory)(void   *address_read,
			    void   *address, 
			    uint_t length,
			    struct vm_device *dev);

  //
  // If your device is memory mapped, this function will
  // be called on an memory read
  //

  int (*write_mapped_memory)(void   *address_written,
			     void   *address, 
			     uint_t length,
			     struct vm_device *dev);
  

  //int (*save_device)(struct vm_device *dev, struct *iostream);
  //int (*restore_device)(struct vm_device *dev, struct *iostream);

  struct guest_info  *vm;

  void *private_data;

  // Do not touch anything below this!

  struct vm_device   *next, *prev;

  struct vm_device_io_hook  *io_hooks;
  struct vm_device_mem_hook *mem_hooks;

};


#endif
