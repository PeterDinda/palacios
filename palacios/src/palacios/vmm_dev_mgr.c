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

#include <palacios/vmm_dev_mgr.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm.h>
#include <palacios/vmm_decoder.h>


#ifndef CONFIG_DEBUG_DEV_MGR
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


static struct hashtable * master_dev_table = NULL;

static uint_t dev_hash_fn(addr_t key) {
    char * name = (char *)key;
    return v3_hash_buffer((uchar_t *)name, strlen(name));
}

static int dev_eq_fn(addr_t key1, addr_t key2) {
    char * name1 = (char *)key1;
    char * name2 = (char *)key2;
    
    return (strcmp(name1, name2) == 0);
}


int v3_init_devices() {
    extern struct v3_device_info __start__v3_devices[];
    extern struct v3_device_info __stop__v3_devices[];
    struct v3_device_info * tmp_dev =  __start__v3_devices;
    int i = 0;

#ifdef CONFIG_DEBUG_DEV_MGR
    {
	int num_devices = (__stop__v3_devices - __start__v3_devices) / sizeof(struct v3_device_info);
	PrintDebug("%d Virtual devices registered with Palacios\n", num_devices);
    }
#endif

    PrintDebug("Start addres=%p, Stop address=%p\n", __start__v3_devices, __stop__v3_devices);

    master_dev_table = v3_create_htable(0, dev_hash_fn, dev_eq_fn);



    while (tmp_dev != __stop__v3_devices) {
	PrintDebug("Device: %s\n", tmp_dev->name);

	if (v3_htable_search(master_dev_table, (addr_t)(tmp_dev->name))) {
	    PrintError("Multiple instance of device (%s)\n", tmp_dev->name);
	    return -1;
	}

	if (v3_htable_insert(master_dev_table, 
			     (addr_t)(tmp_dev->name), 
			     (addr_t)(tmp_dev->init)) == 0) {
	    PrintError("Could not add device %s to master list\n", tmp_dev->name);
	    return -1;
	}

	tmp_dev = &(__start__v3_devices[++i]);
    }


    return 0;
}


int v3_init_dev_mgr(struct guest_info * info) {
    struct vmm_dev_mgr * mgr = &(info->dev_mgr);

    INIT_LIST_HEAD(&(mgr->dev_list));
    mgr->num_devs = 0;

    mgr->dev_table = v3_create_htable(0, dev_hash_fn, dev_eq_fn);

    INIT_LIST_HEAD(&(mgr->blk_list));
    INIT_LIST_HEAD(&(mgr->net_list));
    INIT_LIST_HEAD(&(mgr->console_list));

    mgr->blk_table = v3_create_htable(0, dev_hash_fn, dev_eq_fn);
    mgr->net_table = v3_create_htable(0, dev_hash_fn, dev_eq_fn);
    mgr->console_table = v3_create_htable(0, dev_hash_fn, dev_eq_fn);
    
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



int v3_create_device(struct guest_info * info, const char * dev_name, v3_cfg_tree_t * cfg) {
    int (*dev_init)(struct guest_info * info, void * cfg_data);

    dev_init = (void *)v3_htable_search(master_dev_table, (addr_t)dev_name);

    if (dev_init == NULL) {
	PrintError("Could not find device %s in master device table\n", dev_name);
	return -1;
    }


    if (dev_init(info, cfg) == -1) {
	PrintError("Could not initialize Device %s\n", dev_name);
	return -1;
    }

    return 0;
}


void v3_free_device(struct vm_device * dev) {
    V3_Free(dev);
}



struct vm_device * v3_find_dev(struct guest_info * info, const char * dev_name) {
    struct vmm_dev_mgr * mgr = &(info->dev_mgr);

    if (!dev_name) {
	return NULL;
    }

    return (struct vm_device *)v3_htable_search(mgr->dev_table, (addr_t)dev_name);
}


/****************************************************************/
/* The remaining functions are called by the devices themselves */
/****************************************************************/

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



int v3_detach_device(struct vm_device * dev) {
    struct vmm_dev_mgr * mgr = &(dev->vm->dev_mgr);

    dev->ops->free(dev);

    list_del(&(dev->dev_link));
    mgr->num_devs--;

    dev->vm = NULL;

    return -1;
}


struct vm_device * v3_allocate_device(char * name, 
				      struct v3_device_ops * ops, 
				      void * private_data) {
    struct vm_device * dev = NULL;

    dev = (struct vm_device*)V3_Malloc(sizeof(struct vm_device));

    strncpy(dev->name, name, 32);
    dev->ops = ops;
    dev->private_data = private_data;

    dev->vm = NULL;

    return dev;
}


int v3_attach_device(struct guest_info * vm, struct vm_device * dev ) {
    struct vmm_dev_mgr * mgr = &(vm->dev_mgr);

    dev->vm = vm;

    list_add(&(dev->dev_link), &(mgr->dev_list));
    mgr->num_devs++;


    v3_htable_insert(mgr->dev_table, (addr_t)(dev->name), (addr_t)dev);

    return 0;
}



void v3_print_dev_mgr(struct guest_info * info) {
    struct vmm_dev_mgr * mgr = &(info->dev_mgr);
    struct vm_device * dev;

    V3_Print("%d devices registered with manager\n", mgr->num_devs);

    list_for_each_entry(dev, &(mgr->dev_list), dev_link) {
	V3_Print("Device: %s\n", dev->name);
    }

    return;
}




struct blk_frontend {
    int (*connect)(struct guest_info * info, 
		    void * frontend_data, 
		    struct v3_dev_blk_ops * ops, 
		    v3_cfg_tree_t * cfg, 
		    void * priv_data);
	

    struct list_head blk_node;

    void * priv_data;
};



int v3_dev_add_blk_frontend(struct guest_info * info, 
			    char * name, 
			    int (*connect)(struct guest_info * info, 
					    void * frontend_data, 
					    struct v3_dev_blk_ops * ops, 
					    v3_cfg_tree_t * cfg, 
					    void * priv_data), 
			    void * priv_data) {

    struct blk_frontend * frontend = NULL;

    frontend = (struct blk_frontend *)V3_Malloc(sizeof(struct blk_frontend));
    memset(frontend, 0, sizeof(struct blk_frontend));
    
    frontend->connect = connect;
    frontend->priv_data = priv_data;
	
    list_add(&(frontend->blk_node), &(info->dev_mgr.blk_list));
    v3_htable_insert(info->dev_mgr.blk_table, (addr_t)(name), (addr_t)frontend);

    return 0;
}

int v3_dev_connect_blk(struct guest_info * info, 
		       char * frontend_name, 
		       struct v3_dev_blk_ops * ops, 
		       v3_cfg_tree_t * cfg, 
		       void * private_data) {

    struct blk_frontend * frontend = NULL;

    frontend = (struct blk_frontend *)v3_htable_search(info->dev_mgr.blk_table,
						       (addr_t)frontend_name);
    
    if (frontend == NULL) {
	PrintError("Could not find frontend blk device %s\n", frontend_name);
	return 0;
    }

    if (frontend->connect(info, frontend->priv_data, ops, cfg, private_data) == -1) {
	PrintError("Error connecting to block frontend %s\n", frontend_name);
	return -1;
    }

    return 0;
}
