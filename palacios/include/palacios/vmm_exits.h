/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2012, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2012, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __VMM_EXITS_H__
#define __VMM_EXITS_H__


#ifdef __V3VEE__

#include <palacios/vmm_types.h>



struct guest_info;
struct v3_vm_info;

typedef enum { V3_EXIT_RDTSC,
	       V3_EXIT_RDTSCP,
	       V3_EXIT_SWINTR,    
	       V3_EXIT_INVALID } v3_exit_type_t;




struct v3_exit_hook {

    int (*enable)(struct guest_info * core, v3_exit_type_t exit_type);
    int (*disable)(struct guest_info * core, v3_exit_type_t exit_type);

    struct {
	union {
	    uint32_t flags;
	    struct {
		uint32_t hooked      : 1;
		uint32_t registered  : 1;
		uint32_t rsvd        : 30;
	    } __attribute__((packed));
	} __attribute__((packed));
	
    } __attribute__((packed));



    int (*handler)(struct guest_info * core, v3_exit_type_t exit_type, 
		void * priv_data, void * exit_data);
    void * priv_data;

};


struct v3_exit_map {
    
    struct v3_exit_hook * exits; 
};




int v3_init_exit_hooks(struct v3_vm_info * vm);
int v3_deinit_exit_hooks(struct v3_vm_info * vm);

int v3_init_exit_hooks_core(struct guest_info * core);



int v3_register_exit(struct v3_vm_info * vm, v3_exit_type_t exit_type,
		     int (*enable)(struct guest_info * core, v3_exit_type_t exit_type),
		     int (*disable)(struct guest_info * core, v3_exit_type_t exit_type));
		     


int v3_dispatch_exit_hook(struct guest_info * core, v3_exit_type_t exit_type, void * exit_data);






int v3_hook_exit(struct v3_vm_info * vm, v3_exit_type_t exit_type,
		 int (*handler)(struct guest_info * core, v3_exit_type_t exit_type, 
			     void * priv_data, void * exit_data),
		 void * priv_data, 
		 struct guest_info * current_core);

int v3_unhook_exit(struct v3_vm_info * vm, v3_exit_type_t exit_type);
		   

#endif

#endif
