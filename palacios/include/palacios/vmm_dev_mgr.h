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

#ifndef _VMM_DEV_MGR
#define _VMM_DEV_MGR

#ifdef __V3VEE__

#include <palacios/vmm_types.h>
#include <palacios/vmm_list.h>
#include <palacios/vmm_string.h>
#include <palacios/vmm_hashtable.h>


struct guest_info;

struct v3_device_ops;


struct vm_device {
    char name[32];
  
    void * private_data;

    struct v3_device_ops * ops;

    struct guest_info * vm;

    struct list_head dev_link;


    uint_t num_io_hooks;
    struct list_head io_hooks;
};


struct vmm_dev_mgr {
    uint_t num_devs;
    struct list_head dev_list;

    struct hashtable * dev_table;
};




int v3_create_device(struct guest_info * info, const char * dev_name, void * cfg_data);
void v3_free_device(struct vm_device * dev);


struct vm_device * v3_find_dev(struct guest_info * info, const char * dev_name);


// Registration of devices

//
// The following device manager functions should only be called
// when the guest is stopped
//



int v3_init_dev_mgr(struct guest_info * info);
int v3_dev_mgr_deinit(struct guest_info * info);




int v3_init_devices();


struct v3_device_ops {
    int (*free)(struct vm_device *dev);


    int (*reset)(struct vm_device *dev);

    int (*start)(struct vm_device *dev);
    int (*stop)(struct vm_device *dev);


    //int (*save)(struct vm_device *dev, struct *iostream);
    //int (*restore)(struct vm_device *dev, struct *iostream);
};






int v3_dev_hook_io(struct vm_device   *dev,
		   ushort_t            port,
		   int (*read)(ushort_t port, void * dst, uint_t length, struct vm_device * dev),
		   int (*write)(ushort_t port, void * src, uint_t length, struct vm_device * dev));

int v3_dev_unhook_io(struct vm_device   *dev,
		     ushort_t            port);


int v3_attach_device(struct guest_info * vm, struct vm_device * dev);
int v3_detach_device(struct vm_device * dev);

struct vm_device * v3_allocate_device(char * name, struct v3_device_ops * ops, void * private_data);


struct v3_device_info {
    char * name;
    int (*init)(struct guest_info * info, void * cfg_data);
};


#define device_register(name, init_dev_fn)				\
    static char _v3_device_name[] = name;				\
    static struct v3_device_info _v3_device				\
    __attribute__((__used__))						\
	__attribute__((unused, __section__ ("_v3_devices"),		\
		       aligned(sizeof(addr_t))))			\
	= {_v3_device_name , init_dev_fn};




void PrintDebugDevMgr(struct guest_info * info);
void PrintDebugDev(struct vm_device * dev);





#endif // ! __V3VEE__

#endif
