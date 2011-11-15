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
#include <palacios/vmm_extensions.h>

#ifdef V3_CONFIG_SVM
#include <palacios/svm.h>
#endif
#ifdef V3_CONFIG_VMX
#include <palacios/vmx.h>
#endif

#ifdef V3_CONFIG_CHECKPOINT
#include <palacios/vmm_checkpoint.h>
#endif


v3_cpu_arch_t v3_cpu_types[V3_CONFIG_MAX_CPUS];
struct v3_os_hooks * os_hooks = NULL;
int v3_dbg_enable = 0;




static void init_cpu(void * arg) {
    uint32_t cpu_id = (uint32_t)(addr_t)arg;

#ifdef V3_CONFIG_SVM
    if (v3_is_svm_capable()) {
        PrintDebug("Machine is SVM Capable\n");
        v3_init_svm_cpu(cpu_id);
	
    } else 
#endif
#ifdef V3_CONFIG_VMX
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
#ifdef V3_CONFIG_SVM
	case V3_SVM_CPU:
	case V3_SVM_REV3_CPU:
	    PrintDebug("Deinitializing SVM CPU %d\n", cpu_id);
	    v3_deinit_svm_cpu(cpu_id);
	    break;
#endif
#ifdef V3_CONFIG_VMX
	case V3_VMX_CPU:
	case V3_VMX_EPT_CPU:
	case V3_VMX_EPT_UG_CPU:
	    PrintDebug("Deinitializing VMX CPU %d\n", cpu_id);
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

    for (i = 0; i < V3_CONFIG_MAX_CPUS; i++) {
	v3_cpu_types[i] = V3_INVALID_CPU;
    }

    // Register all the possible device types
    V3_init_devices();

    // Register all shadow paging handlers
    V3_init_shdw_paging();

    // Register all extensions
    V3_init_extensions();


#ifdef V3_CONFIG_SYMMOD
    V3_init_symmod();
#endif

#ifdef V3_CONFIG_CHECKPOINT
    V3_init_checkpoint();
#endif




    if ((hooks) && (hooks->call_on_cpu)) {

	for (i = 0; i < num_cpus; i++) {

	    V3_Print("Initializing VMM extensions on cpu %d\n", i);
	    hooks->call_on_cpu(i, &init_cpu, (void *)(addr_t)i);
	}
    }


}


void Shutdown_V3() {
    int i;

    V3_deinit_devices();
    V3_deinit_shdw_paging();

    V3_deinit_extensions();

#ifdef V3_CONFIG_SYMMOD
    V3_deinit_symmod();
#endif

#ifdef V3_CONFIG_CHECKPOINT
    V3_deinit_checkpoint();
#endif


    if ((os_hooks) && (os_hooks->call_on_cpu)) {
	for (i = 0; i < V3_CONFIG_MAX_CPUS; i++) {
	    if (v3_cpu_types[i] != V3_INVALID_CPU) {
		V3_Call_On_CPU(i, deinit_cpu, (void *)(addr_t)i);
		//deinit_cpu((void *)(addr_t)i);
	    }
	}
    }

}


v3_cpu_arch_t v3_get_cpu_type(int cpu_id) {
    return v3_cpu_types[cpu_id];
}


struct v3_vm_info * v3_create_vm(void * cfg, void * priv_data, char * name) {
    struct v3_vm_info * vm = v3_config_guest(cfg, priv_data);

    if (vm == NULL) {
	PrintError("Could not configure guest\n");
	return NULL;
    }

    V3_Print("CORE 0 RIP=%p\n", (void *)(addr_t)(vm->cores[0].rip));

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


    PrintDebug("virtual core %u (on logical core %u): in start_core (RIP=%p)\n", 
	       core->vcpu_id, core->pcpu_id, (void *)(addr_t)core->rip);

    switch (v3_cpu_types[0]) {
#ifdef V3_CONFIG_SVM
	case V3_SVM_CPU:
	case V3_SVM_REV3_CPU:
	    return v3_start_svm_guest(core);
	    break;
#endif
#if V3_CONFIG_VMX
	case V3_VMX_CPU:
	case V3_VMX_EPT_CPU:
	case V3_VMX_EPT_UG_CPU:
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
#define MAX_CORES 32


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
	    if (v3_cpu_types[i] == V3_INVALID_CPU) {
		core_mask[major] &= ~(0x1 << minor);
	    } else {
		avail_cores++;
	    }
	}
    }


    if (vm->num_cores > avail_cores) {
	PrintError("Attempted to start a VM with too many cores (vm->num_cores = %d, avail_cores = %d, MAX=%d)\n", 
		   vm->num_cores, avail_cores, MAX_CORES);
	return -1;
    }

    vm->run_state = VM_RUNNING;

    // Spawn off threads for each core. 
    // We work backwards, so that core 0 is always started last.
    for (i = 0, vcore_id = vm->num_cores - 1; (i < MAX_CORES) && (vcore_id >= 0); i++) {
	int major = 0;
 	int minor = 0;
	struct guest_info * core = &(vm->cores[vcore_id]);
	char * specified_cpu = v3_cfg_val(core->core_cfg_data, "target_cpu");
	uint32_t core_idx = 0;

	if (specified_cpu != NULL) {
	    core_idx = atoi(specified_cpu);
	    
	    if ((core_idx < 0) || (core_idx >= MAX_CORES)) {
		PrintError("Target CPU out of bounds (%d) (MAX_CORES=%d)\n", core_idx, MAX_CORES);
	    }

	    i--; // We reset the logical core idx. Not strictly necessary I guess... 
	} else {
	    core_idx = i;
	}

	major = core_idx / 8;
	minor = core_idx % 8;

	if ((core_mask[major] & (0x1 << minor)) == 0) {
	    PrintError("Logical CPU %d not available for virtual core %d; not started\n",
		       core_idx, vcore_id);

	    if (specified_cpu != NULL) {
		PrintError("CPU was specified explicitly (%d). HARD ERROR\n", core_idx);
		v3_stop_vm(vm);
		return -1;
	    }

	    continue;
	}

	PrintDebug("Starting virtual core %u on logical core %u\n", 
		   vcore_id, core_idx);
	
	sprintf(core->exec_name, "%s-%u", vm->name, vcore_id);

	PrintDebug("run: core=%u, func=0x%p, arg=0x%p, name=%s\n",
		   core_idx, start_core, core, core->exec_name);

	core->core_run_state = CORE_STOPPED;  // core zero will turn itself on
	core->pcpu_id = core_idx;
	core->core_thread = V3_CREATE_THREAD_ON_CPU(core_idx, start_core, core, core->exec_name);

	if (core->core_thread == NULL) {
	    PrintError("Thread launch failed\n");
	    v3_stop_vm(vm);
	    return -1;
	}

	vcore_id--;
    }

    if (vcore_id >= 0) {
	PrintError("Error starting VM: Not enough available CPU cores\n");
	v3_stop_vm(vm);
	return -1;
    }


    return 0;

}


int v3_reset_vm_core(struct guest_info * core, addr_t rip) {
    
    switch (v3_cpu_types[core->pcpu_id]) {
#ifdef V3_CONFIG_SVM
	case V3_SVM_CPU:
	case V3_SVM_REV3_CPU:
	    PrintDebug("Resetting SVM Guest CPU %d\n", core->vcpu_id);
	    return v3_reset_svm_vm_core(core, rip);
#endif
#ifdef V3_CONFIG_VMX
	case V3_VMX_CPU:
	case V3_VMX_EPT_CPU:
	case V3_VMX_EPT_UG_CPU:
	    PrintDebug("Resetting VMX Guest CPU %d\n", core->vcpu_id);
	    return v3_reset_vmx_vm_core(core, rip);
#endif
	case V3_INVALID_CPU:
	default:
	    PrintError("CPU has no virtualization Extensions\n");
	    break;
    }

    return -1;
}



/* move a virtual core to different physical core */
int v3_move_vm_core(struct v3_vm_info * vm, int vcore_id, int target_cpu) {
    struct guest_info * core = NULL;

    if ((vcore_id < 0) || (vcore_id >= vm->num_cores)) {
	PrintError("Attempted to migrate invalid virtual core (%d)\n", vcore_id);
	return -1;
    }

    core = &(vm->cores[vcore_id]);

    if (target_cpu == core->pcpu_id) {
	PrintError("Attempted to migrate to local core (%d)\n", target_cpu);
	// well that was pointless
	return 0;
    }

    if (core->core_thread == NULL) {
	PrintError("Attempted to migrate a core without a valid thread context\n");
	return -1;
    }

    while (v3_raise_barrier(vm, NULL) == -1);

    V3_Print("Performing Migration from %d to %d\n", core->pcpu_id, target_cpu);

    // Double check that we weren't preemptively migrated
    if (target_cpu != core->pcpu_id) {    

	V3_Print("Moving Core\n");


#ifdef V3_CONFIG_VMX
	switch (v3_cpu_types[core->pcpu_id]) {
	    case V3_VMX_CPU:
	    case V3_VMX_EPT_CPU:
	    case V3_VMX_EPT_UG_CPU:
		PrintDebug("Flushing VMX Guest CPU %d\n", core->vcpu_id);
		V3_Call_On_CPU(core->pcpu_id, (void (*)(void *))v3_flush_vmx_vm_core, (void *)core);
		break;
	    default:
		break;
	}
#endif

	if (V3_MOVE_THREAD_TO_CPU(target_cpu, core->core_thread) != 0) {
	    PrintError("Failed to move Vcore %d to CPU %d\n", 
		       core->vcpu_id, target_cpu);
	    v3_lower_barrier(vm);
	    return -1;
	} 
	
	/* There will be a benign race window here:
	   core->pcpu_id will be set to the target core before its fully "migrated"
	   However the core will NEVER run on the old core again, its just in flight to the new core
	*/
	core->pcpu_id = target_cpu;

	V3_Print("core now at %d\n", core->pcpu_id);	
    }

    v3_lower_barrier(vm);

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

	v3_yield(NULL);
    }
    
    V3_Print("VM stopped. Returning\n");

    return 0;
}


int v3_pause_vm(struct v3_vm_info * vm) {

    if (vm->run_state != VM_RUNNING) {
	PrintError("Tried to pause a VM that was not running\n");
	return -1;
    }

    while (v3_raise_barrier(vm, NULL) == -1);

    vm->run_state = VM_PAUSED;

    return 0;
}


int v3_continue_vm(struct v3_vm_info * vm) {

    if (vm->run_state != VM_PAUSED) {
	PrintError("Tried to continue a VM that was not paused\n");
	return -1;
    }

    v3_lower_barrier(vm);

    vm->run_state = VM_RUNNING;

    return 0;
}

#ifdef V3_CONFIG_CHECKPOINT
#include <palacios/vmm_checkpoint.h>

int v3_save_vm(struct v3_vm_info * vm, char * store, char * url) {
    return v3_chkpt_save_vm(vm, store, url);
}


int v3_load_vm(struct v3_vm_info * vm, char * store, char * url) {
    return v3_chkpt_load_vm(vm, store, url);
}
#endif


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
	//PrintDebug("Conditional Yield (cur_cyle=%p, start_cycle=%p, period=%p)\n", 
	//           (void *)cur_cycle, (void *)info->yield_start_cycle, 
	//	   (void *)info->yield_cycle_period);
	
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



int v3_vm_enter(struct guest_info * info) {
    switch (v3_cpu_types[0]) {
#ifdef V3_CONFIG_SVM
	case V3_SVM_CPU:
	case V3_SVM_REV3_CPU:
	    return v3_svm_enter(info);
	    break;
#endif
#if V3_CONFIG_VMX
	case V3_VMX_CPU:
	case V3_VMX_EPT_CPU:
	case V3_VMX_EPT_UG_CPU:
	    return v3_vmx_enter(info);
	    break;
#endif
	default:
	    PrintError("Attemping to enter a guest on an invalid CPU\n");
	    return -1;
    }
}
