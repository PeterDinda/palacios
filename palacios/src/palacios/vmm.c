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
#include <palacios/vmm_sprintf.h>

#ifdef CONFIG_SVM
#include <palacios/svm.h>
#endif
#ifdef CONFIG_VMX
#include <palacios/vmx.h>
#endif

#ifdef CONFIG_VNET
#include <palacios/vmm_vnet.h>
#endif


v3_cpu_arch_t v3_cpu_types[CONFIG_MAX_CPUS];
struct v3_os_hooks * os_hooks = NULL;

int v3_dbg_enable = 0;



static void init_cpu(void * arg) {
    uint32_t cpu_id = (uint32_t)(addr_t)arg;

#ifdef CONFIG_SVM
    if (v3_is_svm_capable()) {
        PrintDebug("Machine is SVM Capable\n");
        v3_init_svm_cpu(cpu_id);
	
    } else 
#endif
#ifdef CONFIG_VMX
    if (v3_is_vmx_capable()) {
	PrintDebug("Machine is VMX Capable\n");
	v3_init_vmx_cpu(cpu_id);

    } else 
#endif
    {
       PrintError("CPU has no virtualization Extensions\n");
    }
}



void Init_V3(struct v3_os_hooks * hooks, int num_cpus) {
    int i;

    V3_Print("V3 Print statement to fix a Kitten page fault bug\n");

    // Set global variables. 
    os_hooks = hooks;

    for (i = 0; i < CONFIG_MAX_CPUS; i++) {
	v3_cpu_types[i] = V3_INVALID_CPU;
    }

    // Register all the possible device types
    v3_init_devices();

    // Register all shadow paging handlers
    V3_init_shdw_paging();


#ifdef CONFIG_SYMMOD
    V3_init_symmod();
#endif

#ifdef CONFIG_INSTRUMENT_VMM
    v3_init_instrumentation();
#endif


#ifdef CONFIG_VNET
    V3_init_vnet();
#endif

    if ((hooks) && (hooks->call_on_cpu)) {

	for (i = 0; i < num_cpus; i++) {

	    V3_Print("Initializing VMM extensions on cpu %d\n", i);
	    hooks->call_on_cpu(i, &init_cpu, (void *)(addr_t)i);
	}
    }
}


v3_cpu_arch_t v3_get_cpu_type(int cpu_id) {
    return v3_cpu_types[cpu_id];
}


struct v3_vm_info * v3_create_vm(void * cfg) {
    struct v3_vm_info * vm = v3_config_guest(cfg);

    if (vm == NULL) {
	PrintError("Could not configure guest\n");
	return NULL;
    }

    return vm;
}


int v3_start_vm(struct v3_vm_info * vm, unsigned int cpu_mask) {
    int i = 0;
    V3_Print("V3 --  Starting VM\n");


    for (i = 0; i < vm->num_cores; i++) {
	struct guest_info * info = &(vm->cores[i]);

	/* GRUESOM HACK... */
	//	vm->cpu_id = v3_get_cpu_id();

	switch (v3_cpu_types[info->cpu_id]) {
#ifdef CONFIG_SVM
	    case V3_SVM_CPU:
	    case V3_SVM_REV3_CPU:
		return v3_start_svm_guest(info);
		break;
#endif
#if CONFIG_VMX
	    case V3_VMX_CPU:
	    case V3_VMX_EPT_CPU:
		return v3_start_vmx_guest(info);
		break;
#endif
	    default:
		PrintError("Attemping to enter a guest on an invalid CPU\n");
		return -1;
	}
    }

    return 0;
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

    if (cur_cycle > (info->yield_start_cycle + info->vm_info->yield_cycle_period)) {

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




void v3_print_cond(const char * fmt, ...) {
    if (v3_dbg_enable == 1) {
	char buf[2048];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, 2048, fmt, ap);
	va_end(ap);

	V3_Print("%s", buf);
    }    
}




void v3_interrupt_cpu(struct v3_vm_info * vm, int logical_cpu) {
    extern struct v3_os_hooks * os_hooks;

    if ((os_hooks) && (os_hooks)->interrupt_cpu) {
	(os_hooks)->interrupt_cpu(vm, logical_cpu);
    }
}



unsigned int v3_get_cpu_id() {
    extern struct v3_os_hooks * os_hooks;
    unsigned int ret = (unsigned int)-1;

    if ((os_hooks) && (os_hooks)->get_cpu) {
	ret = os_hooks->get_cpu();
    }

    return ret;
}



int v3_vm_enter(struct guest_info * info) {
    switch (v3_cpu_types[info->cpu_id]) {
#ifdef CONFIG_SVM
	case V3_SVM_CPU:
	case V3_SVM_REV3_CPU:
	    return v3_svm_enter(info);
	    break;
#endif
#if CONFIG_VMX
	case V3_VMX_CPU:
	case V3_VMX_EPT_CPU:
	    return v3_vmx_enter(info);
	    break;
#endif
	default:
	    PrintError("Attemping to enter a guest on an invalid CPU\n");
	    return -1;
    }
}
