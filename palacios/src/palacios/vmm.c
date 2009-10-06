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


v3_cpu_arch_t v3_cpu_types[CONFIG_MAX_CPUS];
struct v3_os_hooks * os_hooks = NULL;



static struct guest_info * allocate_guest() {
    void * info = V3_Malloc(sizeof(struct guest_info));
    memset(info, 0, sizeof(struct guest_info));
    return info;
}


struct vmm_init_arg {
    int cpu_id;
    struct v3_ctrl_ops * vmm_ops;
};

static void init_cpu(void * arg) {
    struct vmm_init_arg * vmm_arg = (struct vmm_init_arg *)arg;
    int cpu_id = vmm_arg->cpu_id;
    struct v3_ctrl_ops * vmm_ops = vmm_arg->vmm_ops;

#ifdef CONFIG_SVM
    if (v3_is_svm_capable()) {
        PrintDebug("Machine is SVM Capable\n");
        v3_init_svm_cpu(cpu_id);
	
	if (cpu_id == 0) {
	    v3_init_svm_hooks(vmm_ops);
	}
    } else 
#endif
#ifdef CONFIG_VMX
    if (v3_is_vmx_capable()) {
	PrintDebug("Machine is VMX Capable\n");
	v3_init_vmx_cpu(cpu_id);

	if (cpu_id == 0) {
	    v3_init_vmx_hooks(vmm_ops);
	}	
    } else 
#endif
    {
       PrintError("CPU has no virtualization Extensions\n");
    }
}



void Init_V3(struct v3_os_hooks * hooks, struct v3_ctrl_ops * vmm_ops, int num_cpus) {
    int i;
    struct vmm_init_arg arg;
    arg.vmm_ops = vmm_ops;    

    // Set global variables. 
    os_hooks = hooks;

    for (i = 0; i < CONFIG_MAX_CPUS; i++) {
	v3_cpu_types[i] = V3_INVALID_CPU;
    }

    // Register all the possible device types
    v3_init_devices();


#ifdef INSTRUMENT_VMM
    v3_init_instrumentation();
#endif

    vmm_ops->allocate_guest = &allocate_guest;


    if ((hooks) && (hooks->call_on_cpu)) {

	for (i = 0; i < num_cpus; i++) {
	    arg.cpu_id = i;

	    V3_Print("Initializing VMM extensions on cpu %d\n", i);
	    hooks->call_on_cpu(i, &init_cpu, &arg);
	}
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

/* 
 * unconditional cpu yield 
 * if the yielding thread is a guest context, the guest quantum is reset on resumption 
 * Non guest context threads should call this function with a NULL argument
 */
void v3_yield(struct guest_info * info) {
    V3_Yield();

    if (info) {
	rdtscll(info->yield_start_cycle);
    }
}



void v3_interrupt_cpu(struct guest_info * info, int logical_cpu) {
    extern struct v3_os_hooks * os_hooks;

    if ((os_hooks) && (os_hooks)->interrupt_cpu) {
	(os_hooks)->interrupt_cpu(info, logical_cpu);
    }
}



int v3_vm_enter(struct guest_info * info) {
    switch (v3_cpu_types[info->cpu_id]) {
#ifdef CONFIG_SVM
	case V3_SVM_CPU:
	case V3_SVM_REV3_CPU:
	    v3_svm_enter(info);
	    break;
#endif
#if CONFIG_VMX && 0
	case V3_VMX_CPU:
	case V3_VMX_EPT_CPU:
	    v3_vmx_enter(info);
	    break;
#endif
	default:
	    PrintError("Attemping to enter a guest on an invalid CPU\n");
	    return -1;
    }

    return 0;
}
