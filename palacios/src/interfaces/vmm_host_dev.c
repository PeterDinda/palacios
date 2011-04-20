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


#include <interfaces/vmm_host_dev.h>
#include <palacios/vmm.h>
#include <palacios/vmm_debug.h>
#include <palacios/vmm_types.h>
#include <palacios/vm_guest.h>
#include <palacios/vm_guest_mem.h>

struct v3_host_dev_hooks * host_dev_hooks = 0;

v3_host_dev_t v3_host_dev_open(char *impl,
			       v3_bus_class_t bus,
			       v3_guest_dev_t gdev,
			       struct v3_vm_info *vm)
{					       
    V3_ASSERT(host_dev_hooks != NULL);
    V3_ASSERT(host_dev_hooks->open != NULL);

    return host_dev_hooks->open(impl,bus,gdev,vm->host_priv_data);
}

int v3_host_dev_close(v3_host_dev_t hdev) 
{
    V3_ASSERT(host_dev_hooks);
    V3_ASSERT(host_dev_hooks->close);

    return host_dev_hooks->close(hdev);
}

uint64_t v3_host_dev_read_io(v3_host_dev_t hdev,
			     uint16_t port,
			     void *dst,
			     uint64_t len)
{
    V3_ASSERT(host_dev_hooks != NULL);
    V3_ASSERT(host_dev_hooks->read_io != NULL);
    
    return host_dev_hooks->read_io(hdev,port,dst,len);
}

uint64_t v3_host_dev_write_io(v3_host_dev_t hdev,
			      uint16_t port,
			      void *src,
			      uint64_t len)
{
    V3_ASSERT(host_dev_hooks != NULL);
    V3_ASSERT(host_dev_hooks->write_io != NULL);
    
    return host_dev_hooks->write_io(hdev,port,src,len);
}

uint64_t v3_host_dev_read_mem(v3_host_dev_t hdev,
			      addr_t gpa,
			      void *dst,
			      uint64_t len)
{
    V3_ASSERT(host_dev_hooks != NULL);
    V3_ASSERT(host_dev_hooks->read_mem != NULL);
    
    return host_dev_hooks->read_mem(hdev,(void*)gpa,dst,len);
}

uint64_t v3_host_dev_write_mem(v3_host_dev_t hdev,
			      addr_t gpa,
			      void *src,
			      uint64_t len)
{
    V3_ASSERT(host_dev_hooks != NULL);
    V3_ASSERT(host_dev_hooks->write_mem != NULL);
    
    return host_dev_hooks->write_mem(hdev,(void*)gpa,src,len);
}

uint64_t v3_host_dev_read_config(v3_host_dev_t hdev,
				 uint64_t offset,
				 void *dst,
				 uint64_t len)
{
    V3_ASSERT(host_dev_hooks != NULL);
    V3_ASSERT(host_dev_hooks->read_config);

    return host_dev_hooks->read_config(hdev,offset,dst,len);
}

uint64_t v3_host_dev_write_config(v3_host_dev_t hdev,
				  uint64_t offset,
				  void *src,
				  uint64_t len)
{
    V3_ASSERT(host_dev_hooks != NULL);
    V3_ASSERT(host_dev_hooks->write_config);
    
    return host_dev_hooks->write_config(hdev,offset,src,len);

}


int v3_host_dev_ack_irq(v3_host_dev_t hdev, uint8_t irq)
{
    V3_ASSERT(host_dev_hooks != NULL);
    V3_ASSERT(host_dev_hooks->ack_irq);

    return host_dev_hooks->ack_irq(hdev,irq);
}


int v3_host_dev_raise_irq(v3_host_dev_t hostdev,
			  v3_guest_dev_t guest_dev,
			  uint8_t irq)
{
    // Make this smarter later...

    struct vm_device *dev = (struct vm_device *) guest_dev;
    
    if (dev && dev->vm) { 
	return v3_raise_irq(dev->vm,irq);
    } else {
	return -1;
    }
}


uint64_t v3_host_dev_read_guest_mem(v3_host_dev_t  hostdev,
				    v3_guest_dev_t guest_dev,
				    void *         gpa,
				    void           *dst,
				    uint64_t       len)
{
    struct vm_device *dev = (struct vm_device *) guest_dev;

    if (!dev) { 
	return 0;
    } else {
	struct v3_vm_info *vm = dev->vm;
	
	if (!vm) { 
	    return 0;
	} else {
	    return v3_read_gpa_memory(&(vm->cores[0]), (addr_t)gpa, len, dst);
	}
    }
}

uint64_t v3_host_dev_write_guest_mem(v3_host_dev_t  hostdev,
				     v3_guest_dev_t guest_dev,
				     void *         gpa,
				     void           *src,
				     uint64_t       len)
{
    struct vm_device *dev = (struct vm_device *) guest_dev;

    if (!dev) { 
	return 0;
    } else {
	struct v3_vm_info *vm = dev->vm;
	
	if (!vm) { 
	    return 0;
	} else {
	    return v3_write_gpa_memory(&(vm->cores[0]), (addr_t)gpa, len, src);
	}
    }
}



void V3_Init_Host_Device_Support(struct v3_host_dev_hooks * hooks) {
    host_dev_hooks = hooks;
    PrintDebug("V3 host device interface inited\n");

    return;
}
