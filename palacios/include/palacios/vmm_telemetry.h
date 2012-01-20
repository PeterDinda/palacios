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

#ifndef __VMM_TELEMETRY_H__
#define __VMM_TELEMETRY_H__

#ifdef __V3VEE__

#ifdef V3_CONFIG_TELEMETRY

#include <palacios/vmm_rbtree.h>
#include <palacios/vmm_list.h>

struct guest_info;
struct v3_vm_info;

struct v3_telemetry_state {
    uint32_t invoke_cnt;
    uint64_t granularity;

    uint64_t prev_tsc;

    struct list_head cb_list;
};


struct v3_core_telemetry {
    uint_t exit_cnt;
    struct rb_root exit_root;

    uint64_t vmm_start_tsc;

    struct v3_telemetry_state * vm_telem;

};


void v3_init_telemetry(struct v3_vm_info * vm);
void v3_init_core_telemetry(struct guest_info * info);
void v3_deinit_telemetry(struct v3_vm_info * vm);
void v3_deinit_core_telemetry(struct guest_info * core);

void v3_telemetry_start_exit(struct guest_info * info);
void v3_telemetry_end_exit(struct guest_info * info, uint_t exit_code);

void v3_print_core_telemetry(struct guest_info * core);
void v3_print_global_telemetry(struct v3_vm_info * vm);
void v3_print_telemetry(struct v3_vm_info * vm, struct guest_info * core);


void v3_add_telemetry_cb(struct v3_vm_info * vm, 
			 void (*telemetry_fn)(struct v3_vm_info * vm, void * private_data, char * hdr),
			 void * private_data);

#endif

#endif

#endif
