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
       PrintError("CPU has no virtualizationExtensions\n");
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


struct v3_vm_info * v3_create_vm(void * cfg, void * priv_data) {
    struct v3_vm_info * vm = v3_config_guest(cfg);

    if (vm == NULL) {
	PrintError("Could not configure guest\n");
	return NULL;
    }

    vm->host_priv_data = priv_data;

    return vm;
}


static int start_core(void *p)
{
    struct guest_info * info = (struct guest_info *)p;


    PrintDebug("core %u: in start_core\n",info->cpu_id);
    
    // we assume here that the APs are in INIT mode
    // and only the BSP is in REAL
    // the per-architecture code will rely on this
    // assumption


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
	    PrintError("Attempting to enter a guest on an invalid CPU\n");
	    return -1;
    }
    // should not happen
    return 0;
}


static uint32_t get_next_core(unsigned int cpu_mask, uint32_t last_proc)
{
    uint32_t proc_to_use;

    PrintDebug("In get_next_core cpu_mask=0x%x last_proc=%u\n",cpu_mask,last_proc);

    proc_to_use=(last_proc+1) % 32; // only 32 procs
    // This will wrap around, and eventually we can use proc 0, 
    // since that's clearly available
    while (!((cpu_mask >> proc_to_use)&0x1)) {
	proc_to_use=(proc_to_use+1)%32;
    }
    return proc_to_use;
}

int v3_start_vm(struct v3_vm_info * vm, unsigned int cpu_mask) {
    uint32_t i;
    uint32_t last_proc;
    uint32_t proc_to_use;
    char tname[16];

    V3_Print("V3 --  Starting VM (%u cores)\n",vm->num_cores);

    // We assume that we are running on CPU 0 of the underlying system
    last_proc=0;

    // We will fork off cores 1..n first, then boot core zero
    
    // for the AP, we need to create threads
 
    for (i = 1; i < vm->num_cores; i++) {
	if (!os_hooks->start_thread_on_cpu) { 
	    PrintError("Host OS does not support start_thread_on_cpu - FAILING\n");
	    return -1;
	}

	proc_to_use=get_next_core(cpu_mask,last_proc);
	last_proc=proc_to_use;

	// vm->cores[i].cpu_id=i;
	// vm->cores[i].physical_cpu_id=proc_to_use;

	PrintDebug("Starting virtual core %u on logical core %u\n",i,proc_to_use);
	
	sprintf(tname,"core%u",i);

	PrintDebug("run: core=%u, func=0x%p, arg=0x%p, name=%s\n",
		   proc_to_use, start_core, &(vm->cores[i]), tname);

	// TODO: actually manage these threads instead of just launching them
	if (!(os_hooks->start_thread_on_cpu(proc_to_use,start_core,&(vm->cores[i]),tname))) { 
	    PrintError("Thread launch failed\n");
	    return -1;
	}
    }

    // vm->cores[0].cpu_id=0;
    // vm->cores[0].physical_cpu_id=0;

    // Finally launch the BSP on core 0
    sprintf(tname,"core%u",0);

#if CONFIG_LINUX
    if (vm->num_cores==1) { 
	start_core(&(vm->cores[0]));
	return -1;
    } else {
	if (!os_hooks->start_thread_on_cpu(0,start_core,&(vm->cores[0]),tname)) { 
	    PrintError("Thread launch failed\n");
	    return -1;
	}
    }
#else
    if (!os_hooks->start_thread_on_cpu(0,start_core,&(vm->cores[0]),tname)) { 
	PrintError("Thread launch failed\n");
	return -1;
    }
#endif

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
    cur_cycle = v3_get_host_time(&info->time_state);

    if (cur_cycle > (info->yield_start_cycle + info->vm_info->yield_cycle_period)) {

	/*
	  PrintDebug("Conditional Yield (cur_cyle=%p, start_cycle=%p, period=%p)\n", 
	  (void *)cur_cycle, (void *)info->yield_start_cycle, (void *)info->yield_cycle_period);
	*/
	V3_Yield();
	info->yield_start_cycle = v3_get_host_time(&info->time_state);
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
	info->yield_start_cycle = v3_get_host_time(&info->time_state);
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




void v3_interrupt_cpu(struct v3_vm_info * vm, int logical_cpu, int vector) {
    extern struct v3_os_hooks * os_hooks;

    if ((os_hooks) && (os_hooks)->interrupt_cpu) {
	(os_hooks)->interrupt_cpu(vm, logical_cpu, vector);
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
