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


#include <palacios/vm_dev.h>
#include <palacios/vmm.h>



static struct vm_device * v3_allocate_device() {

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

struct vm_device * v3_create_device(char * name, struct vm_device_ops * ops, void * private_data) {
    struct vm_device * dev = v3_allocate_device();

    strncpy(dev->name, name, 32);
    dev->ops = ops;
    dev->private_data = private_data;

    return dev;
}

void v3_free_device(struct vm_device * dev) {
    V3_Free(dev);
}

