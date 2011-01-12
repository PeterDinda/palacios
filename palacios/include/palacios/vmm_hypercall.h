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


#ifndef __VMM_HYPERCALL_H__
#define __VMM_HYPERCALL_H__

#ifdef __V3VEE__
#include <palacios/vmm_types.h>
#include <palacios/vmm_rbtree.h>

typedef struct rb_root v3_hypercall_map_t;

struct guest_info;
struct v3_vm_info;



typedef enum {
    TEST_HCALL =           0x0001,
    SYMCALL_RET_HCALL =    0x0535,         // args in GPRs
    SYMCALL_ERR_HCALL =    0x0536,         // RBX: error code

    /* -- Symmod symbol table hypercall --
     * RBX: SymTable start 
     * RCX: SymTable size  
     * RDX: SymStrs start 
     * RSI: SymStrs size
     */
    SYMMOD_SYMS_HCALL =    0x0600,         
    
    MEM_OFFSET_HCALL =     0x1000,         // RBX: base addr(out)
    GUEST_INFO_HCALL =     0x3000,         // no args
    TELEMETRY_HCALL =      0x3001,         // no args
    BALLOON_START_HCALL =  0xba00,         // RAX: size
    BALLOON_QUERY_HCALL =  0xba01,         // RCX: req_pgs(out), RDX: alloc_pgs(out)
    OS_DEBUG_HCALL =       0xc0c0,          // RBX: msg_gpa, RCX: msg_len, RDX: buf_is_va (flag)
    TIME_CPUFREQ_HCALL   =  0xd000       //RCX: cpu freq (out)
} hcall_id_t;




void v3_init_hypercall_map(struct v3_vm_info * vm);
int v3_deinit_hypercall_map(struct v3_vm_info * vm);

int v3_register_hypercall(struct v3_vm_info * vm, hcall_id_t hypercall_id, 
			  int (*hypercall)(struct guest_info * info , hcall_id_t hcall_id, void * priv_data),
			  void * priv_data);
int v3_remove_hypercall(struct v3_vm_info * vm, hcall_id_t hypercall_id);

int v3_handle_hypercall(struct guest_info * info);




#endif

#endif
