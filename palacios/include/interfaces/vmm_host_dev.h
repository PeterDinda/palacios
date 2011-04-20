/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2011, Peter Dinda <pdinda@northwestern.edu> 
 * Copyright (c) 2011, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#ifndef __VMM_HOST_DEV_H__
#define __VMM_HOST_DEV_H__

#include <palacios/vmm.h>

/*

  The purpose of this interface is to make it possible to implement
  virtual devices in the host OS.   It is intended to be used by 
  passthrough device implementations, such as the generic device 
  and the PCI passthrough device.  

  One use of this interface, and the generic and PCI passthrough devices
  might be to build an interface with simulated devices in SST 
  under a Linux host.  That scenario would look like this:

Guest config:

  generic device:
    <device class="generic" id="mydev" impl="host_sst">
       ports, memory regions, interrupts set with PASSTHROUGH option 
    </device>

  PCI passthrough devive:
    <device class="pci_passthrough" id="mydev", impl="host_sst">
       vendor and device ids, etc
    </device>

impl="physical" or lack of an impl key would indicate that direct hardware
access is expected, which is how these devices currently operate. 


Host (Linux) side:

    There would be an implementation and registration of the hooks 
    defined and explained in this file

    The implementation might, for example, create an interface to 
    a user space process, for example like the console 
    (palacios-console.[ch] + v3_cons.c) or graphics console
    (palacios-graphics-console.[ch] + v3_vncserver.c) do
    and route the hook functions defined here through it. 
    Through this interface, the calls could be routed to an SST
    device module.   

*/


/* A host device is opaque to the palacios */
typedef void * v3_host_dev_t;
/* A guest device is opaque to the host */
typedef void * v3_guest_dev_t;


/* There is a notion of a bus class to which the device is attached */
typedef enum { V3_BUS_CLASS_DIRECT, V3_BUS_CLASS_PCI } v3_bus_class_t;

#ifdef __V3VEE__

struct v3_vm_info;

v3_host_dev_t v3_host_dev_open(char *impl, 
			       v3_bus_class_t bus,
			       v3_guest_dev_t gdev,
			       struct v3_vm_info *vm); 

int v3_host_dev_close(v3_host_dev_t hdev);
    
uint64_t v3_host_dev_read_io(v3_host_dev_t hostdev,  
			     uint16_t      port,
			     void          *dest,
			     uint64_t      len);

uint64_t v3_host_dev_write_io(v3_host_dev_t hostdev, 
			      uint16_t      port,
			      void          *src,
			      uint64_t      len);

uint64_t v3_host_dev_read_mem(v3_host_dev_t hostdev, 
			      addr_t        gpa,
			      void          *dest,
			      uint64_t      len);

uint64_t v3_host_dev_write_mem(v3_host_dev_t hostdev, 
			       addr_t        gpa,
			       void          *src,
			       uint64_t      len);

int v3_host_dev_ack_irq(v3_host_dev_t hostdev, uint8_t irq);

uint64_t v3_host_dev_read_config(v3_host_dev_t hostdev, 
				 uint64_t      offset,
				 void          *dest,
				 uint64_t      len);

uint64_t v3_host_dev_write_config(v3_host_dev_t hostdev, 
				  uint64_t      offset,
				  void          *src,
				  uint64_t      len);
 
#endif

struct v3_host_dev_hooks {
    
    // The host is given the implementation name, the type of bus
    // this device is attached to and an opaque pointer back to the
    // guest device.  It returns an opaque representation of 
    // the host device it has attached to, with zero indicating
    // failure.  The host_priv_data arguement supplies to the 
    // host the pointer that the VM was originally registered with
    v3_host_dev_t (*open)(char *impl, 
			  v3_bus_class_t bus,
			  v3_guest_dev_t gdev,
			  void *host_priv_data);

    int (*close)(v3_host_dev_t hdev);
    
    // Read/Write from/to an IO port. The read must either
    // completely succeed, returning len or completely
    // fail, returning != len
    // Callee gets the host dev id and the port in the guest
    uint64_t (*read_io)(v3_host_dev_t hostdev, 
			uint16_t      port,
			void          *dest,
			uint64_t      len);

    uint64_t (*write_io)(v3_host_dev_t hostdev, 
			 uint16_t      port,
			 void          *src,
			 uint64_t      len);
    
    // Read/Write from/to memory. The reads/writes must
    // completely succeed, returning len or completely
    // fail, returning != len
    // Callee gets the host dev id, and the guest physical address
    uint64_t (*read_mem)(v3_host_dev_t hostdev, 
			 void *        gpa,
			 void          *dest,
			 uint64_t      len);
    
    uint64_t (*write_mem)(v3_host_dev_t hostdev, 
			  void *        gpa,
			  void          *src,
			  uint64_t      len);
    
    //
    // Palacios or the guest device will call this
    // function when it has injected the irq
    // requested by the guest
    // 
    int (*ack_irq)(v3_host_dev_t hostdev, uint8_t irq);

    // Configuration space reads/writes for devices that
    // have them, such as PCI devices
    // As with other reads/writes, these must be fully successful
    // or fail
    //
    // Palacios maintains its own configuration for some
    // devices (e.g., pci_passthrough) and will take care of 
    // relevant hooking/unhooking, and maintain its own
    // config space info.   However, a read will return
    // the host device's config, while a write will affect
    // both the palacios-internal config and the hsot device's config
    //
    // for V3_BUS_CLASS_PCI they correspond to PCI config space (e.g., BARS, etc)
    // reads and writes
    //
    uint64_t (*read_config)(v3_host_dev_t hostdev, 
			    uint64_t      offset,
			    void          *dest,
			    uint64_t      len);
    
    uint64_t (*write_config)(v3_host_dev_t hostdev,
			     uint64_t      offset,
			     void          *src,
			     uint64_t      len);
 
};

/* This function is how the host will raise an irq to palacios
   for the device.   The IRQ argument will be ignored for devices
   whose irqs are managed by palacios */
int v3_host_dev_raise_irq(v3_host_dev_t hostdev,
			  v3_guest_dev_t guest_dev,
			  uint8_t irq);

/* These functions allow the host to read and write the guest
   memory by physical address, for example to implement DMA 
*/
uint64_t v3_host_dev_read_guest_mem(v3_host_dev_t  hostdev,
				    v3_guest_dev_t guest_dev,
				    void *         gpa,
				    void           *dest,
				    uint64_t       len);

uint64_t v3_host_dev_write_guest_mem(v3_host_dev_t  hostdev,
				     v3_guest_dev_t guest_dev,
				     void *         gpa,
				     void           *src,
				     uint64_t       len);
			      

extern void V3_Init_Host_Device_Support(struct v3_host_dev_hooks *hooks);

#endif
