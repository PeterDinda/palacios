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


static void deinit_cpu(void * arg) {
    uint32_t cpu_id = (uint32_t)(addr_t)arg;


    switch (v3_cpu_types[cpu_id]) {
 #ifdef CONFIG_SVM
	case V3_VMX_CPU:
	case V3_VMX_EPT_CPU:
	    PrintDebug("Machine is SVM Capable\n");
	    v3_deinit_svm_cpu(cpu_id);
	    break;
#endif
#ifdef CONFIG_VMX
	case V3_SVM_CPU:
	case V3_SVM_REV3_CPU:
	    PrintDebug("Machine is VMX Capable\n");
	    v3_deinit_vmx_cpu(cpu_id);
	    break;
#endif
	case V3_INVALID_CPU:
	default:
	    PrintError("CPU has no virtualization Extensions\n");
	    break;
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
    V3_init_devices();

    // Register all shadow paging handlers
    V3_init_shdw_paging();


#ifdef CONFIG_SYMMOD
    V3_init_symmod();
#endif


#ifdef CONFIG_VNET
    v3_init_vnet();
#endif


#ifdef CONFIG_MULTITHREAD_OS
    if ((hooks) && (hooks->call_on_cpu)) {

	for (i = 0; i < num_cpus; i++) {

	    V3_Print("Initializing VMM extensions on cpu %d\n", i);
	    hooks->call_on_cpu(i, &init_cpu, (void *)(addr_t)i);
	}
    }
#else 
    init_cpu(0);
#endif

}


void Shutdown_V3() {
    int i;

    V3_deinit_devices();
    V3_deinit_shdw_paging();

#ifdef CONFIG_SYMMOD
    V3_deinit_symmod();
#endif


#ifdef CONFIG_VNET
    v3_deinit_vnet();
#endif

#ifdef CONFIG_MULTITHREAD_OS
    if ((os_hooks) && (os_hooks->call_on_cpu)) {
	for (i = 0; i < CONFIG_MAX_CPUS; i++) {
	    if (v3_cpu_types[i] != V3_INVALID_CPU) {
		deinit_cpu((void *)(addr_t)i);
	    }
	}
    }
#else 
    deinit_cpu(0);
#endif

}


v3_cpu_arch_t v3_get_cpu_type(int cpu_id) {
    return v3_cpu_types[cpu_id];
}


struct v3_vm_info * v3_create_vm(void * cfg, void * priv_data, char * name) {
    struct v3_vm_info * vm = v3_config_guest(cfg, priv_data);

    V3_Print("CORE 0 RIP=%p\n", (void *)(addr_t)(vm->cores[0].rip));


    if (vm == NULL) {
	PrintError("Could not configure guest\n");
	return NULL;
    }

    if (name == NULL) {
	name = "[V3_VM]";
    } else if (strlen(name) >= 128) {
	PrintError("VM name is too long. Will be truncated to 128 chars.\n");
    }

    memset(vm->name, 0, 128);
    strncpy(vm->name, name, 127);

    return vm;
}


static int start_core(void * p)
{
    struct guest_info * core = (struct guest_info *)p;


    PrintDebug("virtual core %u: in start_core (RIP=%p)\n", 
	       core->cpu_id, (void *)(addr_t)core->rip);

    switch (v3_cpu_types[0]) {
#ifdef CONFIG_SVM
	case V3_SVM_CPU:
	case V3_SVM_REV3_CPU:
	    return v3_start_svm_guest(core);
	    break;
#endif
#if CONFIG_VMX
	case V3_VMX_CPU:
	case V3_VMX_EPT_CPU:
	    return v3_start_vmx_guest(core);
	    break;
#endif
	default:
	    PrintError("Attempting to enter a guest on an invalid CPU\n");
	    return -1;
    }
    // should not happen
    return 0;
}


// For the moment very ugly. Eventually we will shift the cpu_mask to an arbitrary sized type...
#ifdef CONFIG_MULTITHREAD_OS
#define MAX_CORES 32
#else
#define MAX_CORES 1
#endif


int v3_start_vm(struct v3_vm_info * vm, unsigned int cpu_mask) {
    uint32_t i;
    uint8_t * core_mask = (uint8_t *)&cpu_mask; // This is to make future expansion easier
    uint32_t avail_cores = 0;
    int vcore_id = 0;

    /// CHECK IF WE ARE MULTICORE ENABLED....

    V3_Print("V3 --  Starting VM (%u cores)\n", vm->num_cores);
    V3_Print("CORE 0 RIP=%p\n", (void *)(addr_t)(vm->cores[0].rip));


    // Check that enough cores are present in the mask to handle vcores
    for (i = 0; i < MAX_CORES; i++) {
	int major = i / 8;
	int minor = i % 8;
	
	if (core_mask[major] & (0x1 << minor)) {
	    avail_cores++;
	}
    }


    if (vm->num_cores > avail_cores) {
	PrintError("Attempted to start a VM with too many cores (vm->num_cores = %d, avail_cores = %d, MAX=%d)\n", vm->num_cores, avail_cores, MAX_CORES);
	return -1;
    }

#ifdef CONFIG_MULTITHREAD_OS
    // spawn off new threads, for other cores
    for (i = 0, vcore_id = 1; (i < MAX_CORES) && (vcore_id < vm->num_cores); i++) {
	int major = i / 8;
	int minor = i % 8;
	void * core_thread = NULL;
	struct guest_info * core = &(vm->cores[vcore_id]);

	/* This assumes that the core 0 thread has been mapped to physical core 0 */
	if (i == V3_Get_CPU()) {
	    // We skip the local CPU because it is reserved for vcore 0
	    continue;
	}

	if ((core_mask[major] & (0x1 << minor)) == 0) {
	    PrintError("Logical CPU %d not available for virtual core %d; not started\n",
		       i, vcore_id);
	    continue;
	} 

	PrintDebug("Starting virtual core %u on logical core %u\n", 
		   vcore_id, i);
	
	sprintf(core->exec_name, "%s-%u", vm->name, vcore_id);

	PrintDebug("run: core=%u, func=0x%p, arg=0x%p, name=%s\n",
		   i, start_core, core, core->exec_name);

	// TODO: actually manage these threads instead of just launching them
	core_thread = V3_CREATE_THREAD_ON_CPU(i, start_core, core, core->exec_name);

	if (core_thread == NULL) {
	    PrintError("Thread launch failed\n");
	    return -1;
	}

	vcore_id++;
    }
#endif

    sprintf(vm->cores[0].exec_name, "%s", vm->name);

    if (start_core(&(vm->cores[0])) != 0) {
	PrintError("Error starting VM core 0\n");
	return -1;
    }


    return 0;

}




int v3_stop_vm(struct v3_vm_info * vm) {

    vm->run_state = VM_STOPPED;

    // force exit all cores via a cross call/IPI

    while (1) {
	int i = 0;
	int still_running = 0;

	for (i = 0; i < vm->num_cores; i++) {
	    if (vm->cores[i].core_run_state != CORE_STOPPED) {
		still_running = 1;
	    }
	}

	if (still_running == 0) {
 	    break;
	}

 	V3_Print("Yielding\n");

	v3_yield(NULL);
    }
    
    V3_Print("VM stopped. Returning\n");

    return 0;
}


int v3_free_vm(struct v3_vm_info * vm) {
    int i = 0;
    // deinitialize guest (free memory, etc...)

    v3_free_vm_devices(vm);

    // free cores
    for (i = 0; i < vm->num_cores; i++) {
	v3_free_core(&(vm->cores[i]));
    }

    // free vm
    v3_free_vm_internal(vm);

    v3_free_config(vm);

    V3_Free(vm);

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


#ifdef CONFIG_MULTITHREAD_OS

void v3_interrupt_cpu(struct v3_vm_info * vm, int logical_cpu, int vector) {
    extern struct v3_os_hooks * os_hooks;

    if ((os_hooks) && (os_hooks)->interrupt_cpu) {
	(os_hooks)->interrupt_cpu(vm, logical_cpu, vector);
    }
}
#endif



int v3_vm_enter(struct guest_info * info) {
    switch (v3_cpu_types[0]) {
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
