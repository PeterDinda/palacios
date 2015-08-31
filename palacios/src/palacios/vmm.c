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
#include <palacios/vmm_mem.h>
#include <palacios/vmm_intr.h>
#include <palacios/vmm_config.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_ctrl_regs.h>
#include <palacios/vmm_lowlevel.h>
#include <palacios/vmm_sprintf.h>
#include <palacios/vmm_extensions.h>
#include <palacios/vmm_timeout.h>
#include <palacios/vmm_options.h>
#include <palacios/vmm_cpu_mapper.h>
#include <palacios/vmm_direct_paging.h>
#include <interfaces/vmm_numa.h>
#include <interfaces/vmm_file.h>

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
v3_cpu_arch_t v3_mach_type = V3_INVALID_CPU;

struct v3_os_hooks * os_hooks = NULL;
int v3_dbg_enable = 0;



static void init_cpu(void * arg) {
    uint32_t cpu_id = (uint32_t)(addr_t)arg;

    v3_init_fp();

#ifdef V3_CONFIG_SVM
    if (v3_is_svm_capable()) {
        PrintDebug(VM_NONE, VCORE_NONE, "Machine is SVM Capable\n");
        v3_init_svm_cpu(cpu_id);
	
    } else 
#endif
#ifdef V3_CONFIG_VMX
    if (v3_is_vmx_capable()) {
        PrintDebug(VM_NONE, VCORE_NONE, "Machine is VMX Capable\n");
	v3_init_vmx_cpu(cpu_id);

    } else 
#endif
    {
       PrintError(VM_NONE, VCORE_NONE, "CPU has no virtualization Extensions\n");
    }
}


static void deinit_cpu(void * arg) {
    uint32_t cpu_id = (uint32_t)(addr_t)arg;


    switch (v3_cpu_types[cpu_id]) {
#ifdef V3_CONFIG_SVM
	case V3_SVM_CPU:
	case V3_SVM_REV3_CPU:
	    PrintDebug(VM_NONE, VCORE_NONE, "Deinitializing SVM CPU %d\n", cpu_id);
	    v3_deinit_svm_cpu(cpu_id);
	    break;
#endif
#ifdef V3_CONFIG_VMX
	case V3_VMX_CPU:
	case V3_VMX_EPT_CPU:
	case V3_VMX_EPT_UG_CPU:
	    PrintDebug(VM_NONE, VCORE_NONE, "Deinitializing VMX CPU %d\n", cpu_id);
	    v3_deinit_vmx_cpu(cpu_id);
	    break;
#endif
	case V3_INVALID_CPU:
	default:
	    PrintError(VM_NONE, VCORE_NONE, "CPU has no virtualization Extensions\n");
	    break;
    }

    v3_deinit_fp();

}


static int in_long_mode()
{
  uint32_t high, low;

  v3_get_msr(0xc0000080,&high,&low); // EFER
  
  return ((low & 0x500)== 0x500);  // LMA and LME set
}
  

void Init_V3(struct v3_os_hooks * hooks, char * cpu_mask, int num_cpus, char *options) {
    int i = 0;
    int minor = 0;
    int major = 0;

    V3_Print(VM_NONE, VCORE_NONE, "V3 Print statement to fix a Kitten page fault bug\n");



#ifndef __V3_64BIT__
#error Palacios does not support compilation for a 32 bit host OS!!!!
#else
    if (!in_long_mode()) { 
      PrintError(VM_NONE,VCORE_NONE,"Palacios supports execution only in long mode (64 bit).\n");
      return;
    }
#endif

    // Set global variables. 
    os_hooks = hooks;

    if (num_cpus>V3_CONFIG_MAX_CPUS) { 
	PrintError(VM_NONE,VCORE_NONE, "Requesting as many as %d cpus, but Palacios is compiled for a maximum of %d.  Only the first %d cpus will be considered\n", num_cpus, V3_CONFIG_MAX_CPUS, V3_CONFIG_MAX_CPUS);
    }

    // Determine the global machine type
    v3_mach_type = V3_INVALID_CPU;

    for (i = 0; i < V3_CONFIG_MAX_CPUS; i++) {
	v3_cpu_types[i] = V3_INVALID_CPU;
    }

#ifdef V3_CONFIG_CACHEPART
    v3_init_cachepart();
#endif

    // Parse host-os defined options into an easily-accessed format.
    v3_parse_options(options);

#ifdef V3_CONFIG_MULTIBOOT
    v3_init_multiboot();
#endif

#ifdef V3_CONFIG_HVM
    v3_init_hvm();
#endif


    // Memory manager initialization
    v3_init_mem();

    // Register all the possible device types
    V3_init_devices();

    // Register all shadow paging handlers
    V3_init_shdw_paging();

#ifdef V3_CONFIG_SWAPPING
    v3_init_swapping();
#endif

    // Initialize the cpu_mapper framework (must be before extensions)
    V3_init_cpu_mapper();

    // Initialize the scheduler framework (must be before extensions)
    V3_init_scheduling();
 
    // Register all extensions
    V3_init_extensions();

    // Enabling cpu_mapper
    V3_enable_cpu_mapper();

    // Enabling scheduler
    V3_enable_scheduler();


#ifdef V3_CONFIG_SYMMOD
    V3_init_symmod();
#endif

#ifdef V3_CONFIG_CHECKPOINT
    V3_init_checkpoint();
#endif

    if ((hooks) && (hooks->call_on_cpu)) {

        for (i = 0; i < num_cpus && i < V3_CONFIG_MAX_CPUS; i++) {
            major = i / 8;
            minor = i % 8;

            if ((cpu_mask == NULL) || (*(cpu_mask + major) & (0x1 << minor))) {
                V3_Print(VM_NONE, VCORE_NONE, "Initializing VMM extensions on cpu %d\n", i);
                hooks->call_on_cpu(i, &init_cpu, (void *)(addr_t)i);

		if (v3_mach_type == V3_INVALID_CPU) {
		    v3_mach_type = v3_cpu_types[i];
		}   
            }
        }
    }
}



void Shutdown_V3() {
    int i;

    // Reverse order of Init_V3

    // bring down CPUs

    if ((os_hooks) && (os_hooks->call_on_cpu)) {
	for (i = 0; i < V3_CONFIG_MAX_CPUS; i++) {
	    if (v3_cpu_types[i] != V3_INVALID_CPU) {
		V3_Call_On_CPU(i, deinit_cpu, (void *)(addr_t)i);
		//deinit_cpu((void *)(addr_t)i);
	    }
	}
    }

#ifdef V3_CONFIG_CHECKPOINT
    V3_deinit_checkpoint();
#endif

#ifdef V3_CONFIG_SYMMOD
    V3_deinit_symmod();
#endif

    V3_disable_scheduler();

    V3_disable_cpu_mapper();

    V3_deinit_extensions();

    V3_deinit_scheduling();
    
    V3_deinit_cpu_mapper();

#ifdef V3_CONFIG_SWAPPING
    v3_deinit_swapping();
#endif
    
    V3_deinit_shdw_paging();
    
    V3_deinit_devices();

    v3_deinit_mem();
    
#ifdef V3_CONFIG_HVM
    v3_deinit_hvm();
#endif

#ifdef V3_CONFIG_MULTIBOOT
    v3_deinit_multiboot();
#endif

    v3_deinit_options();
    
#ifdef V3_CONFIG_CACHEPART
    v3_deinit_cachepart();
#endif

}


v3_cpu_arch_t v3_get_cpu_type(int cpu_id) {
    return v3_cpu_types[cpu_id];
}

static int start_core(void * p)
{
    struct guest_info * core = (struct guest_info *)p;

    if (v3_scheduler_register_core(core) == -1){
        PrintError(core->vm_info, core,"Error initializing scheduling in core %d\n", core->vcpu_id);
    }

    PrintDebug(core->vm_info,core,"virtual core %u (on logical core %u): in start_core (RIP=%p)\n",
	       core->vcpu_id, core->pcpu_id, (void *)(addr_t)core->rip);

    switch (v3_mach_type) {
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
	    PrintError(core->vm_info, core, "Attempting to enter a guest on an invalid CPU\n");
	    return -1;
    }
    // should not happen
    return 0;
}

struct v3_vm_info * v3_create_vm(void * cfg, void * priv_data, char * name, unsigned int cpu_mask) {
    struct v3_vm_info * vm = v3_config_guest(cfg, priv_data);
    int vcore_id = 0;

    if (vm == NULL) {
	PrintError(VM_NONE, VCORE_NONE, "Could not configure guest\n");
	return NULL;
    }

    V3_Print(vm, VCORE_NONE, "CORE 0 RIP=%p\n", (void *)(addr_t)(vm->cores[0].rip));

    if (name == NULL) {
	name = "[V3_VM]";
    } else if (strlen(name) >= 128) {
        PrintError(vm, VCORE_NONE,"VM name is too long. Will be truncated to 128 chars.\n");
    }

    memset(vm->name, 0, 128);
    strncpy(vm->name, name, 127);

    if(v3_cpu_mapper_register_vm(vm) == -1) {

        PrintError(vm, VCORE_NONE,"Error registering VM with cpu_mapper\n");
    }

    /*
     * Register this VM with the palacios scheduler. It will ask for admission
     * prior to launch.
     */
    if(v3_scheduler_register_vm(vm) == -1) {
    
        PrintError(vm, VCORE_NONE,"Error registering VM with scheduler\n");
    }

    if (v3_cpu_mapper_admit_vm(vm,cpu_mask) != 0){
        PrintError(vm, VCORE_NONE,"Error admitting VM %s for mapping", vm->name);
    }

    for (vcore_id = 0; vcore_id < vm->num_cores; vcore_id++) {

        struct guest_info * core = &(vm->cores[vcore_id]);

	PrintDebug(vm, VCORE_NONE, "Creating virtual core %u on logical core %u\n",
		   vcore_id, core->pcpu_id);

	sprintf(core->exec_name, "%s-%d", vm->name, vcore_id);

        PrintDebug(vm, VCORE_NONE, "run: core=%u, func=0x%p, arg=0x%p, name=%s\n",
		   core->pcpu_id, start_core, core, core->exec_name);


	// Resource controls for cores can be independent, but
	// currently are not, hence this copy.
	core->resource_control = vm->resource_control;

	core->core_thread = V3_CREATE_THREAD_ON_CPU(core->pcpu_id, start_core, core, core->exec_name, &core->resource_control);

	if (core->core_thread == NULL) {
	    PrintError(vm, VCORE_NONE, "Thread creation failed\n");
	    v3_stop_vm(vm);
	    return NULL;
	}

    }
    return vm;
}

int v3_start_vm(struct v3_vm_info * vm, unsigned int cpu_mask) {

    uint32_t i;
    uint8_t * core_mask = (uint8_t *)&cpu_mask; // This is to make future expansion easier
    uint32_t avail_cores = 0;
    int vcore_id = 0;

    if (!vm) { 
        PrintError(VM_NONE, VCORE_NONE, "Asked to start nonexistent VM\n");
	return -1;
    }

    if (vm->run_state != VM_STOPPED) {
        PrintError(vm, VCORE_NONE, "VM has already been launched (state=%d)\n", (int)vm->run_state);
        return -1;
    }

#if V3_CONFIG_MULTIBOOT
    if (v3_setup_multiboot_vm_for_boot(vm)) { 
	PrintError(vm, VCORE_NONE, "Multiboot setup for boot failed\n");
	return -1;
    }
#endif
#if V3_CONFIG_HVM
    if (v3_setup_hvm_vm_for_boot(vm)) { 
	PrintError(vm, VCORE_NONE, "HVM setup for boot failed\n");
	return -1;
    }
#endif

    /// CHECK IF WE ARE MULTICORE ENABLED....

    V3_Print(vm, VCORE_NONE, "V3 --  Starting VM (%u cores)\n", vm->num_cores);
    V3_Print(vm, VCORE_NONE, "CORE 0 RIP=%p\n", (void *)(addr_t)(vm->cores[0].rip));


    // Check that enough cores are present in the mask to handle vcores
    for (i = 0; i < V3_CONFIG_MAX_CPUS; i++) {
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

    vm->avail_cores = avail_cores;

    if (v3_scheduler_admit_vm(vm) != 0){
       PrintError(vm, VCORE_NONE,"Error admitting VM %s for scheduling", vm->name);
    }

    vm->run_state = VM_RUNNING;

    for (vcore_id = 0; vcore_id < vm->num_cores; vcore_id++) {

        struct guest_info * core = &(vm->cores[vcore_id]);

	PrintDebug(vm, VCORE_NONE, "Starting virtual core %u on logical core %u\n", 
		   vcore_id, core->pcpu_id);

	if (core->core_run_state==CORE_INVALID) {
	  // launch of a fresh VM
	  core->core_run_state = CORE_STOPPED;
	  // core zero will turn itself on
	} else {
	  // this is a resume - use whatever its current run_state is
	}

	V3_START_THREAD(core->core_thread);

    }

    return 0;

}


int v3_reset_vm_core(struct guest_info * core, addr_t rip) {
    
    switch (v3_cpu_types[core->pcpu_id]) {
#ifdef V3_CONFIG_SVM
	case V3_SVM_CPU:
	case V3_SVM_REV3_CPU:
	    PrintDebug(core->vm_info, core, "Resetting SVM Guest CPU %d\n", core->vcpu_id);
	    return v3_reset_svm_vm_core(core, rip);
#endif
#ifdef V3_CONFIG_VMX
	case V3_VMX_CPU:
	case V3_VMX_EPT_CPU:
	case V3_VMX_EPT_UG_CPU:
	    PrintDebug(core->vm_info, core, "Resetting VMX Guest CPU %d\n", core->vcpu_id);
	    return v3_reset_vmx_vm_core(core, rip);
#endif
	case V3_INVALID_CPU:
	default:
	    PrintError(core->vm_info, core, "CPU has no virtualization Extensions\n");
	    break;
    }

    return -1;
}


// resets the whole VM (non-HVM) or the ROS (HVM) 
int v3_reset_vm(struct v3_vm_info *vm)
{
#ifdef V3_CONFIG_HVM
    if (vm->hvm_state.is_hvm) { 
	return v3_reset_vm_extended(vm,V3_VM_RESET_ROS,0);
    } else {
	return v3_reset_vm_extended(vm,V3_VM_RESET_ALL,0);
    }
#else
    return v3_reset_vm_extended(vm,V3_VM_RESET_ALL,0);
#endif
}

int v3_reset_vm_extended(struct v3_vm_info *vm, v3_vm_reset_type t, void *data)
{
    uint32_t start, end, i;
    uint32_t newcount;

    if (vm->run_state != VM_RUNNING) { 
	PrintError(vm,VCORE_NONE,"Attempt to reset VM in state %d (must be in running state)\n",vm->run_state);
	return -1;
    }
	

    switch (t) { 
	case V3_VM_RESET_ALL:
#ifdef V3_CONFIG_HVM
	    if (vm->hvm_state.is_hvm) { 
		PrintError(vm,VCORE_NONE,"Attempt to do ALL reset of HVM (not allowed)\n");
		return -1;
	    }
#endif
	    start=0; end=vm->num_cores-1;
	    break;
#ifdef V3_CONFIG_HVM
	case V3_VM_RESET_HRT:
	case V3_VM_RESET_ROS:
	    if (vm->hvm_state.is_hvm) { 
		if (t==V3_VM_RESET_HRT) { 
		    start = vm->hvm_state.first_hrt_core;
		    end = vm->num_cores-1;
		} else {
		    start = 0;
		    end = vm->hvm_state.first_hrt_core-1;
		}
	    } else {
		PrintError(vm,VCORE_NONE,"This is not an HVM and so HVM-specific resets do not apply\n");
		return -1;
	    }
#endif
	    break;
	case V3_VM_RESET_CORE_RANGE:
	    if (data) { 
		start = ((uint32_t*)data)[0];
		end = ((uint32_t*)data)[1];
	    } else {
		return -1;
	    }
	    break;
	default:
	    PrintError(vm,VCORE_NONE,"Unsupported reset type %d for this VM\n",t);
	    return -1;
	    break;
    }

    PrintDebug(vm,VCORE_NONE,"Resetting cores %d through %d\n",start,end);

    newcount = end-start+1;
    
    for (i=start;i<=end;i++) { 
	if (!(vm->cores[i].core_run_state == CORE_RUNNING || vm->cores[i].core_run_state == CORE_STOPPED)) {
	    PrintError(vm,VCORE_NONE,"Cannot reset VM as core %u is in state %d (must be running or stopped)\n",i,vm->cores[i].core_run_state);
	    return -1;
	}
    }


    // This had better be the only thread using the barrier at this point...
    v3_init_counting_barrier(&vm->reset_barrier,newcount);

    // OK, I am the reseter, tell the relevant cores what to do
    // each will atomically decrement the reset countdown and then
    // spin waiting for it to hit zero.

    for (i=start;i<=end;i++) { 
	vm->cores[i].core_run_state = CORE_RESETTING;
	// force exit of core
	v3_interrupt_cpu(vm, vm->cores[i].pcpu_id, 0);
    }
    
    // we don't wait for reset to finish
    // because reset could have been initiated by a core

    return 0;
}


/* move a virtual core to different physical core */
int v3_move_vm_core(struct v3_vm_info * vm, int vcore_id, int target_cpu) {
    struct guest_info * core = NULL;

    if (!vm) { 
        PrintError(VM_NONE, VCORE_NONE, "Asked to move core of nonexistent VM\n");
	return -1;
    }

    if ((vcore_id < 0) || (vcore_id >= vm->num_cores)) {
        PrintError(vm, VCORE_NONE, "Attempted to migrate invalid virtual core (%d)\n", vcore_id);
	return -1;
    }

    core = &(vm->cores[vcore_id]);

    if (target_cpu == core->pcpu_id) {
	PrintError(vm,  core, "Attempted to migrate to local core (%d)\n", target_cpu);
	// well that was pointless
	return 0;
    }

    if (core->core_thread == NULL) {
	PrintError(vm, core, "Attempted to migrate a core without a valid thread context\n");
	return -1;
    }

    while (v3_raise_barrier(vm, NULL) == -1);

    V3_Print(vm, core, "Performing Migration from %d to %d\n", core->pcpu_id, target_cpu);

    // Double check that we weren't preemptively migrated
    if (target_cpu != core->pcpu_id) {    

	V3_Print(vm, core, "Moving Core\n");

	if(v3_cpu_mapper_admit_core(vm, vcore_id, target_cpu) == -1){
		PrintError(vm, core, "Core %d can not be admitted in cpu %d\n",vcore_id, target_cpu);
		return -1;
	}


#ifdef V3_CONFIG_VMX
	switch (v3_cpu_types[core->pcpu_id]) {
	    case V3_VMX_CPU:
	    case V3_VMX_EPT_CPU:
	    case V3_VMX_EPT_UG_CPU:
		PrintDebug(vm, core, "Flushing VMX Guest CPU %d\n", core->vcpu_id);
		V3_Call_On_CPU(core->pcpu_id, (void (*)(void *))v3_flush_vmx_vm_core, (void *)core);
		break;
	    default:
		break;
	}
#endif

	if (V3_MOVE_THREAD_TO_CPU(target_cpu, core->core_thread) != 0) {
	    PrintError(vm, core, "Failed to move Vcore %d to CPU %d\n", 
		       core->vcpu_id, target_cpu);
	    v3_lower_barrier(vm);
	    return -1;
	} 
	
	/* There will be a benign race window here:
	   core->pcpu_id will be set to the target core before its fully "migrated"
	   However the core will NEVER run on the old core again, its just in flight to the new core
	*/
	core->pcpu_id = target_cpu;

	V3_Print(vm, core, "core now at %d\n", core->pcpu_id);	
    }

    v3_lower_barrier(vm);

    return 0;
}

/* move a memory region to memory with affinity for a specific physical core */
int v3_move_vm_mem(struct v3_vm_info * vm, void *gpa, int target_cpu) {
    int old_node;
    int new_node;
    struct v3_mem_region *reg;
    void *new_hpa;
    int num_pages;
    void *old_hpa;
    int i;

    if (!vm) { 
        PrintError(VM_NONE, VCORE_NONE, "Asked to move memory of nonexistent VM\n");
	return -1;
    }

    old_node = v3_numa_gpa_to_node(vm,(addr_t)gpa);

    if (old_node<0) { 
	PrintError(vm, VCORE_NONE, "Cannot determine current node of gpa %p\n",gpa);
	return -1;
    }

    new_node = v3_numa_cpu_to_node(target_cpu);

    if (new_node<0) { 
	PrintError(vm, VCORE_NONE, "Cannot determine current node of cpu %d\n",target_cpu);
	return -1;
    }

    if (new_node==old_node) { 
	PrintDebug(vm, VCORE_NONE, "Affinity is already established - ignoring request\n");
	return 0;
    }

    // We are now going to change the universe, so 
    // we'll barrier everyone first

    while (v3_raise_barrier(vm, NULL) == -1);

    // get region
    
    reg = v3_get_mem_region(vm, V3_MEM_CORE_ANY, (addr_t) gpa);

    if (!reg) { 
	PrintError(vm, VCORE_NONE, "Attempt to migrate non-existent memory\n");
	goto out_fail;
    }
    
    if (!(reg->flags.base) || !(reg->flags.alloced)) { 
	PrintError(vm, VCORE_NONE, "Attempt to migrate invalid region: base=%d alloced=%d\n", reg->flags.base, reg->flags.alloced);
	goto out_fail;
    }

    // we now have the allocated base region corresponding to  - and not a copy
    // we will rewrite this region after moving its contents
    
    // first, let's double check that we are in fact changing the numa_id...

    if (reg->numa_id==new_node) { 
	PrintDebug(vm, VCORE_NONE, "Affinity for this base region is already established - ignoring...\n");
	goto out_success;
    }

    // region uses exclusive addressing [guest_start,guest_end)
    num_pages = (reg->guest_end-reg->guest_start)/PAGE_SIZE;

    new_hpa = V3_AllocPagesExtended(num_pages,
				    PAGE_SIZE_4KB,
				    new_node,
				    vm->resource_control.pg_filter_func,
				    vm->resource_control.pg_filter_state); 

    if (!new_hpa) { 
	PrintError(vm, VCORE_NONE, "Cannot allocate memory for new base region...\n");
	goto out_fail;
    }

    // Note, assumes virtual contiguity in the host OS... 
    memcpy(V3_VAddr((void*)new_hpa), V3_VAddr((void*)(reg->host_addr)), num_pages*PAGE_SIZE);

    old_hpa = (void*)(reg->host_addr);
    old_node = (int)(reg->numa_id);

    reg->host_addr = (addr_t)new_hpa;
    reg->numa_id = v3_numa_hpa_to_node((addr_t)new_hpa);

    // flush all page tables / kill all humans 

    for (i=0;i<vm->num_cores;i++) { 
	if (vm->cores[i].shdw_pg_mode==SHADOW_PAGING) { 
	    v3_invalidate_shadow_pts(&(vm->cores[i]));
	} else if (vm->cores[i].shdw_pg_mode==NESTED_PAGING) { 
	    // nested invalidator uses inclusive addressing [start,end], not [start,end)
	  v3_invalidate_nested_addr_range(&(vm->cores[i]),reg->guest_start,reg->guest_end-1,NULL,NULL);
	} else {
	    PrintError(vm,VCORE_NONE, "Cannot determine how to invalidate paging structures! Reverting to previous region.\n");
	    // We'll restore things...
	    reg->host_addr = (addr_t) old_hpa;
	    reg->numa_id = old_node;
	    V3_FreePages(new_hpa,num_pages);
	    goto out_fail;
	}
    }
    
    // Now the old region can go away...
    V3_FreePages(old_hpa,num_pages);
    
    PrintDebug(vm,VCORE_NONE,"Migration of memory complete - new region is %p to %p\n",
	       (void*)(reg->host_addr),(void*)(reg->host_addr+num_pages*PAGE_SIZE-1));
    
 out_success:
    v3_lower_barrier(vm);
    return 0;
    
    
 out_fail:
    v3_lower_barrier(vm);
    return -1;
}

int v3_stop_vm(struct v3_vm_info * vm) {

    struct guest_info * running_core;

    if (!vm) { 
        PrintError(VM_NONE, VCORE_NONE, "Asked to stop nonexistent VM\n");
	return -1;
    }

    if ((vm->run_state != VM_RUNNING) && 
	(vm->run_state != VM_SIMULATING)) {
        PrintError(vm, VCORE_NONE,"Tried to stop VM in invalid runstate (%d)\n", vm->run_state);
	return -1;
    }

    vm->run_state = VM_STOPPED;

    // Sanity check to catch any weird execution states
    if (v3_wait_for_barrier(vm, NULL) == 0) {
	v3_lower_barrier(vm);
    }
    
    // XXX force exit all cores via a cross call/IPI XXX

    while (1) {
	int i = 0;
	int still_running = 0;

	for (i = 0; i < vm->num_cores; i++) {
	    if (vm->cores[i].core_run_state != CORE_STOPPED) {
                running_core = &vm->cores[i];
		still_running = 1;
	    }
	}

	if (still_running == 0) {
 	    break;
	}

        v3_scheduler_stop_core(running_core);
    }
    
    V3_Print(vm, VCORE_NONE,"VM stopped. Returning\n");

    return 0;
}


int v3_pause_vm(struct v3_vm_info * vm) {

    if (!vm) { 
        PrintError(VM_NONE, VCORE_NONE, "Asked to pause nonexistent VM\n");
	return -1;
    }

    if (vm->run_state != VM_RUNNING) {
	PrintError(vm, VCORE_NONE,"Tried to pause a VM that was not running\n");
	return -1;
    }

    while (v3_raise_barrier(vm, NULL) == -1);

    vm->run_state = VM_PAUSED;

    return 0;
}


int v3_continue_vm(struct v3_vm_info * vm) {

    if (!vm) { 
        PrintError(VM_NONE, VCORE_NONE, "Asked to continue nonexistent VM\n");
	return -1;
    }

    if (vm->run_state != VM_PAUSED) {
	PrintError(vm, VCORE_NONE,"Tried to continue a VM that was not paused\n");
	return -1;
    }

    vm->run_state = VM_RUNNING;

    v3_lower_barrier(vm);

    return 0;
}



static int sim_callback(struct guest_info * core, void * private_data) {
    struct v3_bitmap * timeout_map = private_data;

    v3_bitmap_set(timeout_map, core->vcpu_id);
    
    V3_Print(core->vm_info, core, "Simulation callback activated (guest_rip=%p)\n", (void *)core->rip);

    while (v3_bitmap_check(timeout_map, core->vcpu_id) == 1) {
        // We spin here if there is noone to yield to
	v3_yield(NULL,-1);
    }

    return 0;
}




int v3_simulate_vm(struct v3_vm_info * vm, unsigned int msecs) {
    struct v3_bitmap timeout_map;
    int i = 0;
    int all_blocked = 0;
    uint64_t cycles = 0;
    uint64_t cpu_khz = V3_CPU_KHZ();

    if (!vm) { 
        PrintError(VM_NONE, VCORE_NONE, "Asked to simulate nonexistent VM\n");
	return -1;
    }

    if (vm->run_state != VM_PAUSED) {
	PrintError(vm, VCORE_NONE,"VM must be paused before simulation begins\n");
	return -1;
    }

    /* AT this point VM is paused */
    
    // initialize bitmap
    v3_bitmap_init(&timeout_map, vm->num_cores);




    // calculate cycles from msecs...
    // IMPORTANT: Floating point not allowed.
    cycles = (msecs * cpu_khz);
    


    V3_Print(vm, VCORE_NONE,"Simulating %u msecs (%llu cycles) [CPU_KHZ=%llu]\n", msecs, cycles, cpu_khz);

    // set timeout
    
    for (i = 0; i < vm->num_cores; i++) {
	if (v3_add_core_timeout(&(vm->cores[i]), cycles, sim_callback, &timeout_map) == -1) {
	    PrintError(vm, VCORE_NONE,"Could not register simulation timeout for core %d\n", i);
	    return -1;
	}
    }

    V3_Print(vm, VCORE_NONE,"timeouts set on all cores\n ");

    
    // Run the simulation
//    vm->run_state = VM_SIMULATING;
    vm->run_state = VM_RUNNING;
    v3_lower_barrier(vm);


    V3_Print(vm, VCORE_NONE,"Barrier lowered: We are now Simulating!!\n");

    // block until simulation is complete    
    while (all_blocked == 0) {
	all_blocked = 1;

	for (i = 0; i < vm->num_cores; i++) {
	    if (v3_bitmap_check(&timeout_map, i)  == 0) {
		all_blocked = 0;
	    }
	}

	if (all_blocked == 1) {
	    break;
	}
	
	// Intentionally spin if there is no one to yield to
	v3_yield(NULL,-1);
    }


    V3_Print(vm, VCORE_NONE,"Simulation is complete\n");

    // Simulation is complete
    // Reset back to PAUSED state

    v3_raise_barrier_nowait(vm, NULL);
    vm->run_state = VM_PAUSED;
    
    v3_bitmap_reset(&timeout_map);

    v3_wait_for_barrier(vm, NULL);

    return 0;

}

int v3_get_state_vm(struct v3_vm_info        *vm, 
		    struct v3_vm_base_state  *base,
		    struct v3_vm_core_state  *core,
		    struct v3_vm_mem_state   *mem)
{
    uint32_t i;
    uint32_t numcores;
    uint32_t numregions; 
    extern uint64_t v3_mem_block_size;
    void     *cur_gpa;

    if (!vm || !base || !core || !mem)  { 
        PrintError(VM_NONE, VCORE_NONE, "Invalid request to v3_get_state_vm\n");
	return -1;
    }

    numcores = core->num_vcores > vm->num_cores ? vm->num_cores : core->num_vcores;
    numregions = mem->num_regions > vm->mem_map.num_base_regions ? vm->mem_map.num_base_regions : mem->num_regions;

    switch (vm->run_state) { 
	case VM_INVALID: base->state = V3_VM_INVALID; break;
	case VM_RUNNING: base->state = V3_VM_RUNNING; break;
	case VM_STOPPED: base->state = V3_VM_STOPPED; break;
	case VM_PAUSED: base->state = V3_VM_PAUSED; break;
	case VM_ERROR: base->state = V3_VM_ERROR; break;
	case VM_SIMULATING: base->state = V3_VM_SIMULATING; break;
	case VM_RESETTING: base->state = V3_VM_RESETTING; break;
	default: base->state = V3_VM_UNKNOWN; break;
    }

    base->vm_type = V3_VM_GENERAL;

#ifdef V3_CONFIG_HVM
    if (vm->hvm_state.is_hvm) { 
	base->vm_type = V3_VM_HVM;
    } 
#endif

    for (i=0;i<numcores;i++) {
	switch (vm->cores[i].core_run_state) {
	    case CORE_INVALID: core->vcore[i].state = V3_VCORE_INVALID; break;
	    case CORE_RUNNING: core->vcore[i].state = V3_VCORE_RUNNING; break;
	    case CORE_STOPPED: core->vcore[i].state = V3_VCORE_STOPPED; break;
	    case CORE_RESETTING: core->vcore[i].state = V3_VCORE_RESETTING; break;
	    default: core->vcore[i].state = V3_VCORE_UNKNOWN; break;
	}
	switch (vm->cores[i].cpu_mode) {
	    case REAL: core->vcore[i].cpu_mode = V3_VCORE_CPU_REAL; break;
	    case PROTECTED: core->vcore[i].cpu_mode = V3_VCORE_CPU_PROTECTED; break;
	    case PROTECTED_PAE: core->vcore[i].cpu_mode = V3_VCORE_CPU_PROTECTED_PAE; break;
	    case LONG: core->vcore[i].cpu_mode = V3_VCORE_CPU_LONG; break;
	    case LONG_32_COMPAT: core->vcore[i].cpu_mode = V3_VCORE_CPU_LONG_32_COMPAT; break;
	    case LONG_16_COMPAT: core->vcore[i].cpu_mode = V3_VCORE_CPU_LONG_16_COMPAT; break;
	    default: core->vcore[i].cpu_mode = V3_VCORE_CPU_UNKNOWN; break;
	}
	switch (vm->cores[i].shdw_pg_mode) { 
	    case SHADOW_PAGING: core->vcore[i].mem_state = V3_VCORE_MEM_STATE_SHADOW; break;
	    case NESTED_PAGING: core->vcore[i].mem_state = V3_VCORE_MEM_STATE_NESTED; break;
	    default: core->vcore[i].mem_state = V3_VCORE_MEM_STATE_UNKNOWN; break;
	}
	switch (vm->cores[i].mem_mode) { 
	    case PHYSICAL_MEM: core->vcore[i].mem_mode = V3_VCORE_MEM_MODE_PHYSICAL; break;
	    case VIRTUAL_MEM: core->vcore[i].mem_mode=V3_VCORE_MEM_MODE_VIRTUAL; break;
	    default: core->vcore[i].mem_mode=V3_VCORE_MEM_MODE_UNKNOWN; break;
	}

	core->vcore[i].vcore_type = V3_VCORE_GENERAL;

#ifdef V3_CONFIG_HVM
	if (vm->hvm_state.is_hvm) { 
	    if (v3_is_hvm_ros_core(&vm->cores[i])) { 
		core->vcore[i].vcore_type = V3_VCORE_ROS;
	    } else {
		core->vcore[i].vcore_type = V3_VCORE_HRT;
	    }
	}
#endif
	
	core->vcore[i].pcore=vm->cores[i].pcpu_id;
	core->vcore[i].last_rip=(void*)(vm->cores[i].rip);
	core->vcore[i].num_exits=vm->cores[i].num_exits;
    }
    
    core->num_vcores=numcores;

    cur_gpa=0;

    for (i=0;i<numregions;i++) {
	mem->region[i].guest_paddr = cur_gpa;
	mem->region[i].host_paddr =  (void*)(vm->mem_map.base_regions[i].host_addr);
	mem->region[i].size = v3_mem_block_size;
#ifdef V3_CONFIG_SWAPPING
	mem->region[i].swapped = vm->mem_map.base_regions[i].flags.swapped;
	mem->region[i].pinned = vm->mem_map.base_regions[i].flags.pinned;
#else
	mem->region[i].swapped = 0;
	mem->region[i].pinned = 0;
#endif

	cur_gpa += mem->region[i].size;
    }

    mem->num_regions=numregions;


    mem->mem_size=vm->mem_size;
    mem->ros_mem_size=vm->mem_size;

#ifdef V3_CONFIG_HVM
    if (vm->hvm_state.is_hvm) { 
	mem->ros_mem_size=v3_get_hvm_ros_memsize(vm);
    }
#endif

    return 0;
}

int v3_get_state_sizes_vm(struct v3_vm_info        *vm, 
			  unsigned long long       *num_vcores, 
			  unsigned long long       *num_regions)
{
    if (!vm || !num_vcores || !num_regions) {
        PrintError(VM_NONE, VCORE_NONE, "Invalid request to v3_get_state_sizes\n");
	return -1;
    }

    *num_vcores = vm->num_cores;
    *num_regions = vm->mem_map.num_base_regions;
    
    return 0;
}


#ifdef V3_CONFIG_CHECKPOINT
#include <palacios/vmm_checkpoint.h>

int v3_save_vm(struct v3_vm_info * vm, char * store, char * url, v3_chkpt_options_t opts) {
    if (!vm || !store || !url) {
	PrintError(VM_NONE,VCORE_NONE, "Incorrect arguemnts for v3_save_vm\n");
	return -1;
    }
    return v3_chkpt_save_vm(vm, store, url, opts);
}


int v3_load_vm(struct v3_vm_info * vm, char * store, char * url, v3_chkpt_options_t opts) {
    if (!vm || !store || !url) {
	PrintError(VM_NONE,VCORE_NONE, "Incorrect arguemnts for v3_load_vm\n");
	return -1;
    }
    return v3_chkpt_load_vm(vm, store, url, opts);
}

#ifdef V3_CONFIG_LIVE_MIGRATION
int v3_send_vm(struct v3_vm_info * vm, char * store, char * url, v3_chkpt_options_t opts) {
    if (!vm || !store || !url) {
	PrintError(VM_NONE,VCORE_NONE, "Incorrect arguemnts for v3_send_vm\n");
	return -1;
    }
    return v3_chkpt_send_vm(vm, store, url, opts);
}


int v3_receive_vm(struct v3_vm_info * vm, char * store, char * url, v3_chkpt_options_t opts) {
    if (!vm || !store || !url) {
	PrintError(VM_NONE,VCORE_NONE, "Incorrect arguemnts for v3_receive_vm\n");
	return -1;
    }
    return v3_chkpt_receive_vm(vm, store, url, opts);
}
#endif

#endif


int v3_free_vm(struct v3_vm_info * vm) {
    int i = 0;
    // deinitialize guest (free memory, etc...)

    if (!vm) { 
        PrintError(VM_NONE, VCORE_NONE, "Asked to free nonexistent VM\n");
	return -1;
    }

    if ((vm->run_state != VM_STOPPED) &&
	(vm->run_state != VM_ERROR)) {
	PrintError(vm, VCORE_NONE,"Tried to Free VM in invalid runstate (%d)\n", vm->run_state);
	return -1;
    }

    v3_free_vm_devices(vm);

    // free cores
    for (i = 0; i < vm->num_cores; i++) {
        v3_scheduler_free_core(&(vm->cores[i]));
	v3_free_core(&(vm->cores[i]));
    }

    // free vm
    v3_scheduler_free_vm(vm);
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

void v3_print_cond(const char * fmt, ...) {
    if (v3_dbg_enable == 1) {
	char buf[2048];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, 2048, fmt, ap);
	va_end(ap);

	V3_Print(VM_NONE, VCORE_NONE,"%s", buf);
    }    
}



void v3_interrupt_cpu(struct v3_vm_info * vm, int logical_cpu, int vector) {
    extern struct v3_os_hooks * os_hooks;

    if ((os_hooks) && (os_hooks)->interrupt_cpu) {
	(os_hooks)->interrupt_cpu(vm, logical_cpu, vector);
    }
}



int v3_vm_enter(struct guest_info * info) {
    switch (v3_mach_type) {
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
	    PrintError(info->vm_info, info, "Attemping to enter a guest on an invalid CPU\n");
	    return -1;
    }
}


void    *v3_get_host_vm(struct v3_vm_info *x)
{
  if (x) { 
    return x->host_priv_data;
  } else {
    return 0;
  }
}

int v3_get_vcore(struct guest_info *x)
{
  if (x) {
    return x->vcpu_id;
  } else {
    return -1;
  }
}
