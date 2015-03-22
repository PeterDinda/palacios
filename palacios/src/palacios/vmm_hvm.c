/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2015, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author:  Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm_mem.h>
#include <palacios/vmm.h>
#include <palacios/vmm_util.h>
#include <palacios/vmm_emulator.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_debug.h>

#include <palacios/vmm_xml.h>

#include <stdio.h>
#include <stdlib.h>

/*

  MEM     = Total size of memory in the GPA (in MB)
  ROS_MEM = Total size of memory for the ROS (in MB) (<RAM)

  GPAs [0,ROS_MEM) are what the ROS sees
  GPAs [ROS_MEM, MEM) are HRT only
  GPAS [0,MEM) are accessible by the HRT

  CORES   = Total number of cores in VM
  ROS_CORES = Total numbber of cores for the ROS

  Cores [0,ROS_CORES) are what the ROS sees
  Cores [ROS_CORES,CORES) are HRT only
  Cores [0,CORES) are accessible by the HRT

  In a Pal file:

  <files> 
    <file id="hrtelf" filename="hrtelf.o" />
  </files>

  <mem ... >RAM</mem>   (MB)  Note these are  
  <cores count="CORES" ...>   backward compatible

  <hvm enable="y">
    <ros cores="ROS_CORES" mem="ROS_MEM" /> (MB)
    <hrt file_id="hrtelf" /hrt>
  </hvm>

*/

#ifndef V3_CONFIG_DEBUG_HVM
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

int v3_init_hvm()
{
    PrintDebug(VM_NONE,VCORE_NONE, "hvm: init\n");
    return 0;
}

int v3_deinit_hvm()
{
    PrintDebug(VM_NONE,VCORE_NONE, "hvm: deinit\n");
    return 0;
}


#define CEIL_DIV(x,y) (((x)/(y)) + !!((x)%(y)))

int v3_init_hvm_vm(struct v3_vm_info *vm, struct v3_xml *config)
{
    v3_cfg_tree_t *hvm_config;
    v3_cfg_tree_t *ros_config;
    v3_cfg_tree_t *hrt_config;
    char *enable;
    char *ros_cores;
    char *ros_mem;
    char *hrt_file_id;

    PrintDebug(vm, VCORE_NONE, "hvm: vm init\n");

    /* 
       Defaults - all ROS
    */
    memset(&vm->hvm_state,0,sizeof(struct v3_vm_hvm));
    vm->hvm_state.is_hvm=0;
    vm->hvm_state.first_hrt_core=vm->num_cores;
    vm->hvm_state.first_hrt_gpa=vm->mem_size;

    if (!config || !(hvm_config=v3_cfg_subtree(config,"hvm"))) {
	PrintDebug(vm,VCORE_NONE,"hvm: no HVM configuration found (all HW is ROS)\n");
	goto out_ok;
    }
    
    if (!(enable=v3_cfg_val(hvm_config,"enable")) || strcasecmp(enable,"y")) {
	PrintDebug(vm,VCORE_NONE,"hvm: HVM configuration disabled (all HW is ROS)\n");
	goto out_ok;
    }

    if (!(ros_config=v3_cfg_subtree(hvm_config,"ros"))) { 
	PrintError(vm,VCORE_NONE,"hvm: HVM configuration without ROS block...\n");
	return -1;
    }
 
    if (!(ros_cores=v3_cfg_val(ros_config,"cores"))) { 
	PrintError(vm,VCORE_NONE,"hvm: ROS block without cores...\n");
	return -1;
    }
   
    vm->hvm_state.first_hrt_core = ((uint32_t)atoi(ros_cores));
    
    if (!(ros_mem=v3_cfg_val(ros_config,"mem"))) { 
	PrintError(vm,VCORE_NONE,"hvm: ROS block without mem...\n");
	return -1;
    }

    vm->hvm_state.first_hrt_gpa = ((uint64_t)atoi(ros_mem))*1024*1024;
	
    if (!(hrt_config=v3_cfg_subtree(hvm_config,"hrt"))) { 
	PrintError(vm,VCORE_NONE,"hvm: HVM configuration without HRT block...\n");
	return -1;
    }
 
    if (!(hrt_file_id=v3_cfg_val(hrt_config,"file_id"))) { 
	PrintError(vm,VCORE_NONE,"hvm: HRT block without file_id...\n");
	return -1;
    }

    vm->hvm_state.hrt_file = v3_cfg_get_file(vm,hrt_file_id);
    
    if (!vm->hvm_state.hrt_file) { 
	PrintError(vm,VCORE_NONE,"hvm: HRT block contains bad file_id (%s)\n",hrt_file_id);
	return -1;
    }

    // XXX sanity check config here

    vm->hvm_state.is_hvm=1;

 out_ok:
    if (vm->hvm_state.is_hvm) {
	V3_Print(vm,VCORE_NONE,"hvm: [ROS: cores 0..%u, mem 0..%p] [HRT: cores %u..%u, mem %p..%p, file_id=%s (tag %s)]\n",
		 vm->hvm_state.first_hrt_core-1,
		 (void*) vm->hvm_state.first_hrt_gpa-1,
		 vm->hvm_state.first_hrt_core,
		 vm->num_cores-1,
		 (void*) vm->hvm_state.first_hrt_gpa,
		 (void*)vm->mem_size-1,
		 hrt_file_id,
		 vm->hvm_state.hrt_file->tag);
    } else {
	V3_Print(vm,VCORE_NONE,"hvm: This is a pure ROS VM\n");
    }
    return 0;
    
}

int v3_deinit_hvm_vm(struct v3_vm_info *vm)
{
    PrintDebug(vm, VCORE_NONE, "hvm: HVM deinit\n");

    return 0;
}

int v3_init_hvm_core(struct guest_info *core)
{
    memset(&core->hvm_state,0,sizeof(core->hvm_state));
    if (core->vm_info->hvm_state.is_hvm) { 
	if (core->vcpu_id >= core->vm_info->hvm_state.first_hrt_core) { 
	    core->hvm_state.is_hrt=1;
	}
    }
    return 0;
}

int v3_deinit_hvm_core(struct guest_info *core)
{
    return 0;
}


uint64_t v3_get_hvm_ros_memsize(struct v3_vm_info *vm)
{
    if (vm->hvm_state.is_hvm) { 
	return vm->hvm_state.first_hrt_gpa;
    } else {
	return vm->mem_size;
    }
}
uint64_t v3_get_hvm_hrt_memsize(struct v3_vm_info *vm)
{
    if (vm->hvm_state.is_hvm) { 
	return vm->mem_size - vm->hvm_state.first_hrt_gpa;
    } else {
	return 0;
    }
}

uint32_t v3_get_hvm_ros_cores(struct v3_vm_info *vm)
{
    if (vm->hvm_state.is_hvm) { 
	return vm->hvm_state.first_hrt_core;
    } else {
	return vm->num_cores;
    }
}

uint32_t v3_get_hvm_hrt_cores(struct v3_vm_info *vm)
{
    if (vm->hvm_state.is_hvm) { 
	return vm->num_cores - vm->hvm_state.first_hrt_core;
    } else {
	return 0;
    }
}


int v3_is_hvm_ros_mem_gpa(struct v3_vm_info *vm, addr_t gpa)
{
    if (vm->hvm_state.is_hvm) { 
	return gpa>=0 && gpa<vm->hvm_state.first_hrt_gpa;
    } else {
	return 1;
    }
}

int v3_is_hvm_hrt_mem_gpa(struct v3_vm_info *vm, addr_t gpa)
{
    if (vm->hvm_state.is_hvm) { 
	return gpa>=vm->hvm_state.first_hrt_gpa && gpa<vm->mem_size;
    } else {
	return 0;
    }
}

int v3_is_hvm_hrt_core(struct guest_info *core)
{
    return core->hvm_state.is_hrt;
}

int v3_is_hvm_ros_core(struct guest_info *core)
{
    return !core->hvm_state.is_hrt;
}

