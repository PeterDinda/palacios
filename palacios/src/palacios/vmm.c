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

#include <palacios/vmm.h>
#include <palacios/vmm_intr.h>
#include <palacios/vmm_config.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_instrument.h>
#include <palacios/vmm_ctrl_regs.h>
#include <palacios/vmm_lowlevel.h>

#ifdef CONFIG_SVM
#include <palacios/svm.h>
#endif
#ifdef CONFIG_VMX
#include <palacios/vmx.h>
#endif


v3_cpu_arch_t v3_cpu_type;
struct v3_os_hooks * os_hooks = NULL;



static struct guest_info * allocate_guest() {
    void * info = V3_Malloc(sizeof(struct guest_info));
    memset(info, 0, sizeof(struct guest_info));
    return info;
}



void Init_V3(struct v3_os_hooks * hooks, struct v3_ctrl_ops * vmm_ops) {
    
    // Set global variables. 
    os_hooks = hooks;
    v3_cpu_type = V3_INVALID_CPU;

    // Register all the possible device types
    v3_init_devices();


#ifdef INSTRUMENT_VMM
    v3_init_instrumentation();
#endif

    vmm_ops->allocate_guest = &allocate_guest;

#ifdef CONFIG_SVM
    if (v3_is_svm_capable()) {
        PrintDebug("Machine is SVM Capable\n");
        v3_init_SVM(vmm_ops);

    } else 
#endif
#ifdef CONFIG_VMX
    if (v3_is_vmx_capable()) {
	PrintDebug("Machine is VMX Capable\n");
	v3_init_vmx(vmm_ops);
	
    } else 
#endif
    {
       PrintError("CPU has no virtualization Extensions\n");
    }
}



#ifdef __V3_32BIT__

v3_cpu_mode_t v3_get_host_cpu_mode() {
    uint32_t cr4_val;
    struct cr4_32 * cr4;

    __asm__ (
	     "movl %%cr4, %0; "
	     : "=r"(cr4_val) 
	     );

    
    cr4 = (struct cr4_32 *)&(cr4_val);

    if (cr4->pae == 1) {
	return PROTECTED_PAE;
    } else {
	return PROTECTED;
    }
}

#elif __V3_64BIT__

v3_cpu_mode_t v3_get_host_cpu_mode() {
    return LONG;
}

#endif 


#define V3_Yield(addr)					\
    do {						\
	extern struct v3_os_hooks * os_hooks;		\
	if ((os_hooks) && (os_hooks)->yield_cpu) {	\
	    (os_hooks)->yield_cpu();			\
	}						\
    } while (0)						\


void v3_yield_cond(struct guest_info * info) {
    uint64_t cur_cycle;
    rdtscll(cur_cycle);

    if (cur_cycle > (info->yield_start_cycle + info->yield_cycle_period)) {

	/*
	  PrintDebug("Conditional Yield (cur_cyle=%p, start_cycle=%p, period=%p)\n", 
	  (void *)cur_cycle, (void *)info->yield_start_cycle, (void *)info->yield_cycle_period);
	*/
	V3_Yield();
	rdtscll(info->yield_start_cycle);
    }
}

void v3_yield(struct guest_info * info) {
    V3_Yield();
    rdtscll(info->yield_start_cycle);
}
