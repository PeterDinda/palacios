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

#ifndef __VMM_SYMCALL_H__
#define __VMM_SYMCALL_H__

#ifdef __V3VEE__


#include <palacios/vmm_regs.h>



struct v3_sym_cpu_context {
    struct v3_gprs vm_regs;
    struct v3_segment cs;
    struct v3_segment ss;
    uint64_t gs_base;
    uint64_t fs_base;
    uint64_t rip;
    uint64_t flags;
    uint8_t cpl;
};

struct v3_symcall_state {
    struct {
	uint_t sym_call_active         : 1;
	uint_t sym_call_returned       : 1;
	uint_t sym_call_error          : 1;
    } __attribute__((packed));

    struct v3_sym_cpu_context old_ctx;

    int sym_call_errno;

    uint64_t sym_call_rip;
    uint64_t sym_call_cs;
    uint64_t sym_call_rsp;
    uint64_t sym_call_gs;
    uint64_t sym_call_fs;
};


typedef uint64_t sym_arg_t;

#define v3_sym_call0(info, call_num)		\
    v3_sym_call(info, call_num, 0, 0, 0, 0, 0)
#define v3_sym_call1(info, call_num, arg1)		\
    v3_sym_call(info, call_num, arg1, 0, 0, 0, 0)
#define v3_sym_call2(info, call_num, arg1, arg2)	\
    v3_sym_call(info, call_num, arg1, arg2, 0, 0, 0)
#define v3_sym_call3(info, call_num, arg1, arg2, arg3)	\
    v3_sym_call(info, call_num, arg1, arg2, arg3, 0, 0)
#define v3_sym_call4(info, call_num, arg1, arg2, arg3, arg4)	\
    v3_sym_call(info, call_num, arg1, arg2, arg3, arg4, 0)
#define v3_sym_call5(info, call_num, arg1, arg2, arg3, arg4, arg5)	\
    v3_sym_call(info, call_num, arg1, arg2, arg3, arg4, arg5)




/* Symcall numbers */
#define SYMCALL_TEST 1
#define SYMCALL_MEM_LOOKUP 10
/* ** */

int v3_sym_call(struct guest_info * info, 
		uint64_t call_num, sym_arg_t * arg0, 
		sym_arg_t * arg1, sym_arg_t * arg2,
		sym_arg_t * arg3, sym_arg_t * arg4);



int v3_init_symcall_vm(struct v3_vm_info * vm);

#endif

#endif
