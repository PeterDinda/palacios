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
#include <palacios/vmm_dev_mgr.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm.h>
#include <palacios/vmm_decoder.h>


#ifndef DEBUG_DEV_MGR
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


//DEFINE_HASHTABLE_INSERT(insert_dev, const char *, struct vm_device *);
//DEFINE_HASHTABLE_SEARCH(find_dev, const char *, struct vm_device *);
//DEFINE_HASHTABLE_REMOVE(remove_dev, const char *, struct vm_device *, 0);

int v3_init_dev_mgr(struct guest_info * info) {
    struct vmm_dev_mgr * mgr = &(info->dev_mgr);

    INIT_LIST_HEAD(&(mgr->dev_list));
    mgr->num_devs = 0;

    //   mgr->dev_table = v3_create_htable(0, , );

    return 0;
}


int v3_dev_mgr_deinit(struct guest_info * info) {
    struct vm_device * dev;
    struct vmm_dev_mgr * mgr = &(info->dev_mgr);
    struct vm_device * tmp;

    list_for_each_entry_safe(dev, tmp, &(mgr->dev_list), dev_link) {
	v3_detach_device(dev);
	v3_free_device(dev);
    }

    return 0;
}



int v3_attach_device(struct guest_info * vm, struct vm_device * dev) {
    struct vmm_dev_mgr *mgr= &(vm->dev_mgr);
  
    dev->vm = vm;

    list_add(&(dev->dev_link), &(mgr->dev_list));
    mgr->num_devs++;

    dev->ops->init(dev);

    return 0;
}

int v3_detach_device(struct vm_device * dev) {
    struct vmm_dev_mgr * mgr = &(dev->vm->dev_mgr);

    dev->ops->deinit(dev);

    list_del(&(dev->dev_link));
    mgr->num_devs--;

    dev->vm = NULL;

    return 0;
}





/* IO HOOKS */
int v3_dev_hook_io(struct vm_device * dev, uint16_t port,
		   int (*read)(uint16_t port, void * dst, uint_t length, struct vm_device * dev),
		   int (*write)(uint16_t port, void * src, uint_t length, struct vm_device * dev)) {
    return v3_hook_io_port(dev->vm, port, 
			   (int (*)(ushort_t, void *, uint_t, void *))read, 
			   (int (*)(ushort_t, void *, uint_t, void *))write, 
			   (void *)dev);
}


int v3_dev_unhook_io(struct vm_device * dev, uint16_t port) {
    return v3_unhook_io_port(dev->vm, port);
}





#ifdef DEBUG_DEV_MGR

void PrintDebugDevMgr(struct guest_info * info) {
    struct vmm_dev_mgr * mgr = &(info->dev_mgr);
    struct vm_device * dev;

    PrintDebug("%d devices registered with manager\n", mgr->num_devs);

    list_for_each_entry(dev, &(mgr->dev_list), dev_link) {
	PrintDebugDev(dev);
	PrintDebug("next..\n");
    }

    return;
}


void PrintDebugDev(struct vm_device * dev) {
    PrintDebug("Device: %s\n", dev->name);
}




#else 
void PrintDebugDevMgr(struct guest_info * info) {}
void PrintDebugDev(struct vm_device * dev) {}
#endif
