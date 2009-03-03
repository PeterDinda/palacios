/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __VM_DEV_H
#define __VM_DEV_H

#ifdef __V3VEE__

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




struct vm_device * v3_create_device(char * name, struct vm_device_ops * ops, void * private_data);
void v3_free_device(struct vm_device * dev);



int v3_dev_hook_io(struct vm_device   *dev,
		   ushort_t            port,
		   int (*read)(ushort_t port, void * dst, uint_t length, struct vm_device * dev),
		   int (*write)(ushort_t port, void * src, uint_t length, struct vm_device * dev));

int v3_dev_unhook_io(struct vm_device   *dev,
		     ushort_t            port);

int v3_dev_hook_mem(struct vm_device   *dev,
		    void               *start,
		    void               *end);

int v3_dev_unhook_mem(struct vm_device   * dev,
		      void               * start,
		      void               * end);


int v3_dev_hook_irq(struct vm_device * dev,
		    uint_t irq, 
		    int (*handler)(uint_t irq, struct vm_device * dev));
int v3_dev_unhook_irq(struct vm_device * dev, uint_t irq);



#endif // ! __V3VEE__

#endif
