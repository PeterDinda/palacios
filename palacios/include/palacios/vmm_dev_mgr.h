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
#include <palacios/vmm_msr.h>
#include <palacios/vmm_config.h>
#include <palacios/vmm_ethernet.h>

struct v3_vm_info;

struct v3_device_ops;

typedef void * v3_dev_data_t;

struct vm_device;

struct vm_device {
    char name[32];
  
    void * private_data;

    struct v3_device_ops * ops;

    struct v3_vm_info * vm;

    struct list_head dev_link;

    uint_t num_res_hooks;
    struct list_head res_hooks;
};


struct vmm_dev_mgr {
    uint_t num_devs;
    struct list_head dev_list;
    struct hashtable * dev_table;

    struct list_head blk_list;
    struct hashtable * blk_table;

    struct list_head net_list;
    struct hashtable * net_table;

    struct list_head char_list;
    struct hashtable * char_table;

    struct list_head cons_list;
    struct hashtable * cons_table;

};

int v3_create_device(struct v3_vm_info * vm, const char * dev_name, v3_cfg_tree_t * cfg);

struct vm_device * v3_find_dev(struct v3_vm_info * info, const char * dev_name);


// Registration of devices

//
// The following device manager functions should only be called
// when the guest is stopped
//



int v3_init_dev_mgr(struct v3_vm_info * vm);
int v3_deinit_dev_mgr(struct v3_vm_info * vm);

int v3_free_vm_devices(struct v3_vm_info * vm);







int V3_init_devices();
int V3_deinit_devices();


#ifdef CONFIG_KEYED_STREAMS
#include <interfaces/vmm_keyed_stream.h>
#endif 

struct v3_device_ops {
    int (*free)(void * private_data);

#ifdef CONFIG_KEYED_STREAMS
    int (*checkpoint)(struct vm_device *dev, v3_keyed_stream_t stream);
    int (*restore)(struct vm_device *dev, v3_keyed_stream_t stream);
#endif
};






int v3_dev_hook_io(struct vm_device   * dev,
		   uint16_t            port,
		   int (*read)(struct guest_info * core, uint16_t port, void * dst, uint_t length, void * priv_data),
		   int (*write)(struct guest_info * core, uint16_t port, void * src, uint_t length, void * priv_data));

int v3_dev_unhook_io(struct vm_device   * dev,
		     uint16_t            port);


int v3_dev_hook_msr(struct vm_device * dev, 
		    uint32_t           msr,
		    int (*read)(struct guest_info * core, uint32_t msr, struct v3_msr * dst, void * priv_data), 
		    int (*write)(struct guest_info * core, uint32_t msr, struct v3_msr src, void * priv_data));

int v3_dev_unhook_msr(struct vm_device * dev, 
		      uint32_t msr);
	    



struct vm_device * v3_add_device(struct v3_vm_info * vm, char * name, 
				 struct v3_device_ops * ops, void * private_data);
int v3_remove_device(struct vm_device * dev);


int v3_attach_device(struct v3_vm_info * vm, struct vm_device * dev);
int v3_detach_device(struct vm_device * dev);
struct vm_device * v3_allocate_device(char * name, struct v3_device_ops * ops, void * private_data);


struct v3_device_info {
    char * name;
    int (*init)(struct v3_vm_info * info, v3_cfg_tree_t * cfg);
};


#define device_register(name, init_dev_fn)				\
    static char _v3_device_name[] = name;				\
    static struct v3_device_info _v3_device				\
    __attribute__((__used__))						\
	__attribute__((unused, __section__ ("_v3_devices"),		\
		       aligned(sizeof(addr_t))))			\
	= {_v3_device_name , init_dev_fn};




void v3_print_dev_mgr(struct v3_vm_info * vm);


struct v3_dev_blk_ops {
    uint64_t (*get_capacity)(void * private_data);
    // Reads always operate on 2048 byte blocks
    int (*read)(uint8_t * buf, uint64_t lba, uint64_t num_bytes, void * private_data);
    int (*write)(uint8_t * buf, uint64_t lba, uint64_t num_bytes, void * private_data);
};

struct v3_dev_net_ops {
    /* Backend implemented functions */
    int (*send)(uint8_t * buf, uint32_t count, void * private_data);

    /* Frontend implemented functions */
    int (*recv)(uint8_t * buf, uint32_t count, void * frnt_data);
    void (*poll)(struct v3_vm_info * vm, int budget, void * frnt_data);

    /* This is ugly... */
    void * frontend_data; 
    char fnt_mac[ETH_ALEN];
};

struct v3_dev_console_ops {
    int (*update_screen)(uint_t x, uint_t y, uint_t length, uint8_t * fb_data, void * private_data);
    int (*update_cursor)(uint_t x, uint_t y, void * private_data);
    int (*scroll)(int rows, void * private_data);
    int (*set_text_resolution)(int cols, int rows, void * private_data);

    /* frontend implemented functions */
    int (*get_screen)(uint_t x, uint_t y, uint_t length, void * frontend_data);
    void * push_fn_arg;
};

struct v3_dev_char_ops {
    /* Backend implemented functions */
    int (*write)(uint8_t * buf, uint64_t len, void * private_data);
    //  int (*read)(uint8_t * buf, uint64_t len, void * private_data);

    /* Frontend Implemented functions */
    int (*push)(struct v3_vm_info * vm, uint8_t * buf, uint64_t len, void * private_data);
};


int v3_dev_add_blk_frontend(struct v3_vm_info * vm, 
			    char * name, 
			    int (*connect)(struct v3_vm_info * vm, 
					    void * frontend_data, 
					    struct v3_dev_blk_ops * ops, 
					    v3_cfg_tree_t * cfg, 
					    void * private_data), 
			    void * priv_data);

int v3_dev_connect_blk(struct v3_vm_info * vm, 
		       char * frontend_name, 
		       struct v3_dev_blk_ops * ops, 
		       v3_cfg_tree_t * cfg, 
		       void * private_data);

int v3_dev_add_net_frontend(struct v3_vm_info * vm, 
			    char * name, 
			    int (*connect)(struct v3_vm_info * vm, 
					    void * frontend_data, 
					    struct v3_dev_net_ops * ops, 
					    v3_cfg_tree_t * cfg, 
					    void * private_data), 
			    void * priv_data);

int v3_dev_connect_net(struct v3_vm_info * vm, 
		       char * frontend_name, 
		       struct v3_dev_net_ops * ops, 
		       v3_cfg_tree_t * cfg, 
		       void * private_data);




int v3_dev_add_console_frontend(struct v3_vm_info * vm, 
				char * name, 
				int (*connect)(struct v3_vm_info * vm, 
					       void * frontend_data, 
					       struct v3_dev_console_ops * ops, 
					       v3_cfg_tree_t * cfg, 
					       void * private_data), 
				void * priv_data);

int v3_dev_connect_console(struct v3_vm_info * vm, 
			   char * frontend_name, 
			   struct v3_dev_console_ops * ops, 
			   v3_cfg_tree_t * cfg, 
			   void * private_data);



int v3_dev_add_char_frontend(struct v3_vm_info * vm, 
			     char * name, 
			     int (*connect)(struct v3_vm_info * vm, 
					    void * frontend_data, 
					    struct v3_dev_char_ops * ops, 
					    v3_cfg_tree_t * cfg, 
					    void * private_data,
					    void ** push_fn_arg), 
			     void * priv_data);

int v3_dev_connect_char(struct v3_vm_info * vm, 
			char * frontend_name, 
			struct v3_dev_char_ops * ops, 
			v3_cfg_tree_t * cfg, 
			void * private_data, 
			void ** push_fn_arg);


#endif // ! __V3VEE__

#endif
