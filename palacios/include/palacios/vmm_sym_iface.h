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


#ifndef __VMM_SYM_IFACE_H__
#define __VMM_SYM_IFACE_H__

#ifdef __V3VEE__

#include <palacios/vm_guest.h>



struct v3_sym_interface {
    uint64_t magic;


    union {
	uint32_t feature_flags;
	struct {
	    uint_t pci_map_valid          : 1;
	    uint32_t sym_call_enabled       : 1;
	} __attribute__((packed));
    } __attribute__((packed));

    union { 
	uint32_t state_flags;
	struct {
	    uint32_t sym_call_active        : 1;
	} __attribute__((packed));
    } __attribute__((packed));

    uint64_t current_proc;
    uint64_t proc_list;
    
    uint8_t pci_pt_map[(4 * 256) / 8]; // we're hardcoding this: (4 busses, 256 max devs)




} __attribute__((packed));




struct v3_sym_context {
    struct v3_gprs vm_regs;
    struct v3_segment cs;
    struct v3_segment ss;
    uint64_t gs_base;
    uint64_t fs_base;
    uint64_t rip;
    uint8_t cpl;
};



struct v3_sym_state {
    
    struct v3_sym_interface * sym_page;
    addr_t sym_page_pa;

    uint64_t guest_pg_addr;

    struct {
	uint_t active              : 1;
	uint_t call_pending        : 1;
	uint_t call_active         : 1;
    } __attribute__((packed));

    struct v3_sym_context old_ctx;
    uint64_t args[6];

    uint64_t sym_call_rip;
    uint64_t sym_call_cs;
    uint64_t sym_call_rsp;
    uint64_t sym_call_gs;
    uint64_t sym_call_fs;
    uint64_t sym_call_ret_fn;

    int (*notifier)(struct guest_info * info, void * private_data);

    void * private_data;

};

int v3_init_sym_iface(struct guest_info * info);



#define v3_sym_call0(info, call_num, cb, priv)		\
    v3_sym_call(info, call_num, 0, 0, 0, 0, 0, cb, priv)
#define v3_sym_call1(info, call_num, arg1, cb, priv)		\
    v3_sym_call(info, call_num, arg1, 0, 0, 0, 0, cb, priv)
#define v3_sym_call2(info, call_num, arg1, arg2, cb, priv)	\
    v3_sym_call(info, call_num, arg1, arg2, 0, 0, 0, cb, priv)
#define v3_sym_call3(info, call_num, arg1, arg2, arg3, cb, priv)	\
    v3_sym_call(info, call_num, arg1, arg2, arg3, 0, 0, cb, priv)
#define v3_sym_call4(info, call_num, arg1, arg2, arg3, arg4, cb, priv)	\
    v3_sym_call(info, call_num, arg1, arg2, arg3, arg4, 0, cb, priv)
#define v3_sym_call5(info, call_num, arg1, arg2, arg3, arg4, arg5, cb, priv)	\
    v3_sym_call(info, call_num, arg1, arg2, arg3, arg4, arg5, cb, priv)




int v3_sym_map_pci_passthrough(struct guest_info * info, uint_t bus, uint_t dev, uint_t fn);
int v3_sym_unmap_pci_passthrough(struct guest_info * info, uint_t bus, uint_t dev, uint_t fn);


/* Symcall numbers */
#define SYMCALL_TEST 1
#define SYMCALL_MEM_LOOKUP 10
/* ** */

int v3_sym_call(struct guest_info * info, 
		uint64_t call_num, uint64_t arg0, 
		uint64_t arg1, uint64_t arg2,
		uint64_t arg3, uint64_t arg4, 
		int (*notifier)(struct guest_info * info, void * private_data),
		void * private_data);

int v3_activate_sym_call(struct guest_info * info);

#endif

#endif
