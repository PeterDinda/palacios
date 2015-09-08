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
#include <palacios/vmm_hypercall.h>

#include <palacios/vmm_xml.h>

#include <palacios/vm_guest_mem.h>

#include <palacios/vmm_debug.h>


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

  <hvm enable="y" >
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

// ignore requests from when we are in the wrong state
#define ENFORCE_STATE_MACHINE 1

// invoke the HRT using a page fault instead of
// the SWINTR mechanism
#define USE_UPCALL_MAGIC_PF  1
#define UPCALL_MAGIC_ADDRESS 0x0000800df00df00dULL
#define UPCALL_MAGIC_ERROR   0xf00df00d

/*
  64 bit only hypercall:

  rax = hypercall number
  rbx = 0x646464...
  then args are:  rcx, rdx, rsi, rdi r8, r9, r10, r11
  rcx = 1st arg
*/
static int hvm_hcall_handler(struct guest_info * core , hcall_id_t hcall_id, void * priv_data)
{
    uint64_t c;
    uint64_t bitness = core->vm_regs.rbx;
    uint64_t a1 = core->vm_regs.rcx;
    uint64_t a2 = core->vm_regs.rdx;
    struct v3_vm_hvm *h = &core->vm_info->hvm_state;


    if (bitness!=0x6464646464646464) { 
	PrintError(core->vm_info,core,"hvm: unable to handle non-64 bit hypercall\n");
	core->vm_regs.rax = -1;
	return 0;
    }

    switch (a1) {
	case 0x0:   // null
	    
	    rdtscll(c);
	    
	    V3_Print(core->vm_info,core, "hvm: received hypercall %x  rax=%llx rbx=%llx rcx=%llx at cycle count %llu (%llu cycles since last boot start) num_exits=%llu since initial boot\n",
		     hcall_id, core->vm_regs.rax, core->vm_regs.rbx, core->vm_regs.rcx, c, core->hvm_state.last_boot_start, core->num_exits);
	    //v3_print_core_telemetry(core);
	    //    v3_print_guest_state(core);
	    core->vm_regs.rax = 0;
	    break;
	    
	case 0x1: // reset ros
	    PrintDebug(core->vm_info,core, "hvm: reset ROS\n");
	    if (v3_reset_vm_extended(core->vm_info,V3_VM_RESET_ROS,0)) { 
		PrintError(core->vm_info,core, "hvm: reset of ROS failed\n");
		core->vm_regs.rax = -1;
	    } else {
		core->vm_regs.rax = 0;
	    }
	    break;

	case 0x2: // reset hrt
	    PrintDebug(core->vm_info,core, "hvm: reset HRT\n");
	    if (v3_reset_vm_extended(core->vm_info,V3_VM_RESET_HRT,0)) { 
		PrintError(core->vm_info,core, "hvm: reset of HRT failed\n");
		core->vm_regs.rax = -1;
	    } else {
		core->vm_regs.rax = 0;
	    }
	    break;

	case 0x3: // reset both
	    PrintDebug(core->vm_info,core, "hvm: reset ROS+HRT\n");
	    if (v3_reset_vm_extended(core->vm_info,V3_VM_RESET_ALL,0)) { 
		PrintError(core->vm_info,core, "hvm: reset of HRT failed\n");
		core->vm_regs.rax = -1;
	    } else {
		core->vm_regs.rax = 0;
	    }
	    break;
	    
	case 0xf: // get HRT state
	    core->vm_regs.rax = h->trans_state;
	    if (v3_write_gva_memory(core, a2, sizeof(h->ros_event), (uint8_t*) &h->ros_event)!=sizeof(h->ros_event)) { 
		PrintError(core->vm_info, core, "hvm: cannot write back ROS event state to %p - continuing\n",(void*)a2);
	    }
	    //PrintDebug(core->vm_info,core,"hvm: get HRT transaction state 0x%llx\n",core->vm_regs.rax);
	    break;

	case 0x10:
	    PrintDebug(core->vm_info, core, "hvm: ROS event request\n");
	    if (h->ros_event.event_type!=ROS_NONE) { 
		PrintError(core->vm_info, core, "hvm: ROS event is already in progress\n");
		core->vm_regs.rax = -1;
	    } else {
		if (v3_read_gva_memory(core, a2, sizeof(h->ros_event), (uint8_t*)&h->ros_event)!=sizeof(h->ros_event)) { 
		    PrintError(core->vm_info, core, "hvm: cannot read ROS event from %p\n",(void*)a2);
		    core->vm_regs.rax = -1;
		} else {
		    core->vm_regs.rax = 0;
		}
	    }

	    break;

	case 0x1f:
	    PrintDebug(core->vm_info, core, "hvm: completion of ROS event (rc=0x%llx)\n",a2);
	    h->ros_event.event_type=ROS_NONE;
	    h->ros_event.last_ros_event_result = a2;
	    break;

	case 0x20: // invoke function (ROS->HRT)
	case 0x21: // invoke parallel function (ROS->HRT)
	    if (v3_is_hvm_hrt_core(core)) { 
		PrintError(core->vm_info,core,"hvm: %s function invocation not supported from HRT core\n", a1==0x20 ? "" : "parallel");
		core->vm_regs.rax = -1;
	    } else {
		if (ENFORCE_STATE_MACHINE && h->trans_state!=HRT_IDLE) { 
		    PrintError(core->vm_info,core, "hvm: cannot invoke %s function %p in state %d\n",a1==0x20 ? "" : "parallel", (void*)a2,h->trans_state);
		    core->vm_regs.rax = -1;
		} else {
		    uint64_t *page = (uint64_t *) h->comm_page_hva;
		    uint64_t first, last, cur;

		    PrintDebug(core->vm_info,core, "hvm: %s invoke function %p\n",a1==0x20 ? "" : "parallel",(void*)a2);
		    page[0] = a1;
		    page[1] = a2;

		    if (a1==0x20) { 
			first=last=h->first_hrt_core;
		    } else {
			first=h->first_hrt_core;
			last=core->vm_info->num_cores-1;
		    }

		    core->vm_regs.rax = 0;

		    h->trans_count = last-first+1;

		    for (cur=first;cur<=last;cur++) { 

#if USE_UPCALL_MAGIC_PF
			PrintDebug(core->vm_info,core,"hvm: injecting magic #PF into core %llu\n",cur);
			core->vm_info->cores[cur].ctrl_regs.cr2 = UPCALL_MAGIC_ADDRESS;
			if (v3_raise_exception_with_error(&core->vm_info->cores[cur],
							  PF_EXCEPTION, 
							  UPCALL_MAGIC_ERROR)) { 
			    PrintError(core->vm_info,core, "hvm: cannot inject HRT #PF to core %llu\n",cur);
			    core->vm_regs.rax = -1;
			    break;
			}
#else
			PrintDebug(core->vm_info,core,"hvm: injecting SW intr 0x%u into core %llu\n",h->hrt_int_vector,cur);
			if (v3_raise_swintr(&core->vm_info->cores[cur],h->hrt_int_vector)) { 
			    PrintError(core->vm_info,core, "hvm: cannot inject HRT interrupt to core %llu\n",cur);
			    core->vm_regs.rax = -1;
			    break;
			}
#endif
			// Force core to exit now
			v3_interrupt_cpu(core->vm_info,core->vm_info->cores[cur].pcpu_id,0);
			  
		    }
		    if (core->vm_regs.rax==0) { 
			if (a1==0x20) { 
			    h->trans_state = HRT_CALL;
			} else {
			    h->trans_state = HRT_PARCALL;
			}
		    }  else {
			PrintError(core->vm_info,core,"hvm: in inconsistent state due to HRT call failure\n");
			h->trans_state = HRT_IDLE;
			h->trans_count = 0;
		    }
		}
	    }
	    break;


	case 0x28: // setup for synchronous operation (ROS->HRT)
	case 0x29: // teardown for synchronous operation (ROS->HRT)
	    if (v3_is_hvm_hrt_core(core)) { 
		PrintError(core->vm_info,core,"hvm: %ssynchronization invocation not supported from HRT core\n",a1==0x29 ? "de" : "");
		core->vm_regs.rax = -1;
	    } else {
		if (ENFORCE_STATE_MACHINE && 
		    ((a1==0x28 && h->trans_state!=HRT_IDLE) || (a1==0x29 && h->trans_state!=HRT_SYNC))) { 
		    PrintError(core->vm_info,core, "hvm: cannot invoke %ssynchronization in state %d\n",a1==0x29 ? "de" : "", h->trans_state);
		    core->vm_regs.rax = -1;
		} else {
		    uint64_t *page = (uint64_t *) h->comm_page_hva;
		    uint64_t first, last, cur;

		    PrintDebug(core->vm_info,core, "hvm: invoke %ssynchronization on address %p\n",a1==0x29 ? "de" : "",(void*)a2);
		    page[0] = a1;
		    page[1] = a2;

		    first=last=h->first_hrt_core;  // initially we will sync only with BSP

		    core->vm_regs.rax = 0;

		    h->trans_count = last-first+1;

		    for (cur=first;cur<=last;cur++) { 

#if USE_UPCALL_MAGIC_PF
			PrintDebug(core->vm_info,core,"hvm: injecting magic #PF into core %llu\n",cur);
			core->vm_info->cores[cur].ctrl_regs.cr2 = UPCALL_MAGIC_ADDRESS;
			if (v3_raise_exception_with_error(&core->vm_info->cores[cur],
							  PF_EXCEPTION, 
							  UPCALL_MAGIC_ERROR)) { 
			    PrintError(core->vm_info,core, "hvm: cannot inject HRT #PF to core %llu\n",cur);
			    core->vm_regs.rax = -1;
			    break;
			}
#else
			PrintDebug(core->vm_info,core,"hvm: injecting SW intr 0x%u into core %llu\n",h->hrt_int_vector,cur);
			if (v3_raise_swintr(&core->vm_info->cores[cur],h->hrt_int_vector)) { 
			    PrintError(core->vm_info,core, "hvm: cannot inject HRT interrupt to core %llu\n",cur);
			    core->vm_regs.rax = -1;
			    break;
			}
#endif
			// Force core to exit now
			v3_interrupt_cpu(core->vm_info,core->vm_info->cores[cur].pcpu_id,0);
			  
		    }
		    if (core->vm_regs.rax==0) { 
			if (a1==0x28) { 
			    h->trans_state = HRT_SYNCSETUP;
			} else {
			    h->trans_state = HRT_SYNCTEARDOWN;			    
			}
		    }  else {
			PrintError(core->vm_info,core,"hvm: in inconsistent state due to HRT call failure\n");
			h->trans_state = HRT_IDLE;
			h->trans_count = 0;
		    }
		}
	    }
	    break;

	case 0x2f: // function exec or sync done
	    if (v3_is_hvm_ros_core(core)) { 
		PrintError(core->vm_info,core, "hvm: request for exec or sync done from ROS core\n");
		core->vm_regs.rax=-1;
	    } else {
		if (ENFORCE_STATE_MACHINE && 
		    h->trans_state!=HRT_CALL && 
		    h->trans_state!=HRT_PARCALL && 
		    h->trans_state!=HRT_SYNCSETUP &&
		    h->trans_state!=HRT_SYNCTEARDOWN) {
		    PrintError(core->vm_info,core,"hvm: function or sync completion when not in HRT_CALL, HRT_PARCALL, HRT_SYNCSETUP, or HRT_SYNCTEARDOWN state\n");
		    core->vm_regs.rax=-1;
		} else {
		    uint64_t one=1;
		    PrintDebug(core->vm_info,core, "hvm: function or sync complete\n");
		    if (__sync_fetch_and_sub(&h->trans_count,one)==1) {
			// last one, switch state
			if (h->trans_state==HRT_SYNCSETUP) { 
			    h->trans_state=HRT_SYNC;
			    PrintDebug(core->vm_info,core, "hvm: function complete - now synchronous\n");
			} else {
			    h->trans_state=HRT_IDLE;
			}
		    }
		    core->vm_regs.rax=0;
		}
	    }
		    
	    break;

	case 0x30: // merge address space
	case 0x31: // unmerge address space
	    if (v3_is_hvm_hrt_core(core)) { 
		PrintError(core->vm_info,core,"hvm: request to %smerge address space from HRT core\n", a1==0x30 ? "" : "un");
		core->vm_regs.rax=-1;
	    } else {
		if (ENFORCE_STATE_MACHINE && h->trans_state!=HRT_IDLE) { 
		    PrintError(core->vm_info,core,"hvm: request to %smerge address space in non-idle state\n",a1==0x30 ? "" : "un");
		    core->vm_regs.rax=-1;
		} else {
		    uint64_t *page = (uint64_t *) h->comm_page_hva;

		    PrintDebug(core->vm_info,core,"hvm: %smerge address space request with %p\n",a1==0x30 ? "" : "un",(void*)core->ctrl_regs.cr3);
		    // should sanity check to make sure guest is in 64 bit without anything strange

		    page[0] = a1;
		    page[1] = core->ctrl_regs.cr3;  // this is a do-not-care for an unmerge

		    core->vm_regs.rax = 0;
#if USE_UPCALL_MAGIC_PF
		    PrintDebug(core->vm_info,core,"hvm: injecting magic #PF into core %u\n",h->first_hrt_core);
		    core->vm_info->cores[h->first_hrt_core].ctrl_regs.cr2 = UPCALL_MAGIC_ADDRESS;
		    if (v3_raise_exception_with_error(&core->vm_info->cores[h->first_hrt_core],
						      PF_EXCEPTION,  
						      UPCALL_MAGIC_ERROR)) { 
		      PrintError(core->vm_info,core, "hvm: cannot inject HRT #PF to core %u\n",h->first_hrt_core);
		      core->vm_regs.rax = -1;
		      break;
		    }
#else
		    PrintDebug(core->vm_info,core,"hvm: injecting SW intr 0x%u into core %u\n",h->hrt_int_vector,h->first_hrt_core);
		    if (v3_raise_swintr(&core->vm_info->cores[h->first_hrt_core],h->hrt_int_vector)) { 
			PrintError(core->vm_info,core, "hvm: cannot inject HRT interrupt to core %u\n",h->first_hrt_core);
			core->vm_regs.rax = -1;
		    } 
#endif		
		    // Force core to exit now
		    v3_interrupt_cpu(core->vm_info,core->vm_info->cores[h->first_hrt_core].pcpu_id,0);

		    h->trans_state = HRT_MERGE;
		}
		
	    }
		
	    break;
	    

	case 0x3f: // merge operation done
	    if (v3_is_hvm_ros_core(core)) { 
		PrintError(core->vm_info,core, "hvm: request for merge done from ROS core\n");
		core->vm_regs.rax=-1;
	    } else {
		if (ENFORCE_STATE_MACHINE && h->trans_state!=HRT_MERGE) {
		    PrintError(core->vm_info,core,"hvm: merge/unmerge done when in non-idle state\n");
		    core->vm_regs.rax=-1;
		} else {
		    PrintDebug(core->vm_info,core, "hvm: merge or unmerge complete - back to idle\n");
		    h->trans_state=HRT_IDLE;
		    core->vm_regs.rax=0;
		}
	    }
		    
	    break;

	default:
	    PrintError(core->vm_info,core,"hvm: unknown hypercall %llx\n",a1);
	    core->vm_regs.rax=-1;
	    break;
    }
		
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
    char *hrt_file_id=0;

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

    if (v3_register_hypercall(vm, HVM_HCALL, 
			      hvm_hcall_handler, 0)) { 
	PrintError(vm,VCORE_NONE, "hvm: cannot register hypercall....\n");
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
    PrintDebug(vm, VCORE_NONE, "hvm: HVM VM deinit\n");

    v3_remove_hypercall(vm,HVM_HCALL);

    if (vm->hvm_state.comm_page_hpa) { 
	struct v3_mem_region *r = v3_get_mem_region(vm,-1,(addr_t)vm->hvm_state.comm_page_hpa);
	if (!r) { 
	    PrintError(vm,VCORE_NONE,"hvm: odd, VM has comm_page_hpa, but no shadow memory\n");
	} else {
	    v3_delete_mem_region(vm,r);
	}
    }

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
    PrintDebug(core->vm_info, VCORE_NONE, "hvm: HVM core deinit\n");

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
    return vm->mem_size;
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
	return gpa<vm->hvm_state.first_hrt_gpa;
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

int      v3_hvm_should_deliver_ipi(struct guest_info *src, struct guest_info *dest)
{
    if (!src) {
	// ioapic or msi to apic
	return !dest->hvm_state.is_hrt;
    } else {
	// apic to apic
	return src->hvm_state.is_hrt || (!src->hvm_state.is_hrt && !dest->hvm_state.is_hrt) ;
    }
}

void     v3_hvm_find_apics_seen_by_core(struct guest_info *core, struct v3_vm_info *vm, 
					uint32_t *start_apic, uint32_t *num_apics)
{
    if (!core) { 
	// Seen from ioapic, msi, etc: 
	if (vm->hvm_state.is_hvm) {
	    // HVM VM shows only the ROS cores/apics to ioapic, msi, etc
	    *start_apic = 0;
	    *num_apics = vm->hvm_state.first_hrt_core;
	} else {
	    // Non-HVM shows all cores/APICs to apic, msi, etc.
	    *start_apic = 0;
	    *num_apics = vm->num_cores;
	}
    } else {
	// Seen from apic
	if (core->hvm_state.is_hrt) { 
	    // HRT core/apic sees all apics
	    // (this policy may change...)
	    *start_apic = 0;
	    *num_apics = vm->num_cores;
	} else {
	    // non-HRT core/apic sees only non-HRT cores/apics
	    *start_apic = 0 ;
	    *num_apics = vm->hvm_state.first_hrt_core;
	}
    }
}

#define MAX(x,y) ((x)>(y)?(x):(y))
#define MIN(x,y) ((x)<(y)?(x):(y))


static uint64_t boot_state_end_addr(struct v3_vm_info *vm) 
{
    return PAGE_ADDR(vm->mem_size);
}
   
static void get_null_int_handler_loc(struct v3_vm_info *vm, void **base, uint64_t *limit)
{
    *base = (void*) PAGE_ADDR(boot_state_end_addr(vm) - PAGE_SIZE);
    *limit = PAGE_SIZE;
}

extern v3_cpu_arch_t v3_mach_type;

extern void *v3_hvm_svm_null_int_handler_start;
extern void *v3_hvm_svm_null_int_handler_end;
extern void *v3_hvm_vmx_null_int_handler_start;
extern void *v3_hvm_vmx_null_int_handler_end;

static void write_null_int_handler(struct v3_vm_info *vm)
{
    void *base;
    uint64_t limit;
    void *data;
    uint64_t len;

    get_null_int_handler_loc(vm,&base,&limit);

    switch (v3_mach_type) {
#ifdef V3_CONFIG_SVM
	case V3_SVM_CPU:
	case V3_SVM_REV3_CPU:
	    data = (void*) &v3_hvm_svm_null_int_handler_start;
	    len = (void*) &v3_hvm_svm_null_int_handler_end - data;
	    break;
#endif
#if V3_CONFIG_VMX
	case V3_VMX_CPU:
	case V3_VMX_EPT_CPU:
	case V3_VMX_EPT_UG_CPU:
	    data = (void*) &v3_hvm_vmx_null_int_handler_start;
	    len = (void*) &v3_hvm_vmx_null_int_handler_end - data;
	    break;
#endif
	default:
	    PrintError(vm,VCORE_NONE,"hvm: cannot determine CPU type to select null interrupt handler...\n");
	    data = 0;
	    len = 0;
    }

    if (data) {
	v3_write_gpa_memory(&vm->cores[0],(addr_t)(base),len,(uint8_t*)data);
    }

    PrintDebug(vm,VCORE_NONE,"hvm: wrote null interrupt handler at %p (%llu bytes)\n",base,len);
}


static void get_idt_loc(struct v3_vm_info *vm, void **base, uint64_t *limit)
{
    *base = (void*) PAGE_ADDR(boot_state_end_addr(vm) - 2 * PAGE_SIZE);
    *limit = 16*256;
}

// default IDT entries (int and trap gates)
//
// Format is 16 bytes long:
//   16 offsetlo   => 0
//   16 selector   => (target code selector) => 0x8 // entry 1 of GDT
//    3 ist        => (stack) = 0 => current stack
//    5 reserved   => 0
//    4 type       => 0xe=>INT, 0xf=>TRAP 
//    1 reserved   => 0  (indicates "system" by being zero)
//    2 dpl        => 0
//    1 present    => 1
//   16 offsetmid  => 0
//   32 offsethigh => 0   (total is a 64 bit offset)
//   32 reserved   => 0
//
// 00 00 | 08 00 | 00 | 8[typenybble] | offsetmid | offsethigh | reserved
// 
// Note little endian
//
static uint64_t idt64_trap_gate_entry_mask[2] = { 0x00008f0000080000, 0x0 } ;
static uint64_t idt64_int_gate_entry_mask[2] =  { 0x00008e0000080000, 0x0 };

static void write_idt(struct v3_vm_info *vm)
{
    void *base;
    uint64_t limit;
    void *handler;
    uint64_t handler_len;
    int i;
    uint64_t trap_gate[2];
    uint64_t int_gate[2];

    get_idt_loc(vm,&base,&limit);

    get_null_int_handler_loc(vm,&handler,&handler_len);

    handler += vm->hvm_state.gva_offset;

    memcpy(trap_gate,idt64_trap_gate_entry_mask,16);
    memcpy(int_gate,idt64_int_gate_entry_mask,16);

    if (handler) {
	// update the entries for the handler location
	uint8_t *mask;
	uint8_t *hand;
	
	hand = (uint8_t*) &handler;

	mask = (uint8_t *)trap_gate;
	memcpy(&(mask[0]),&(hand[0]),2); // offset low
	memcpy(&(mask[6]),&(hand[2]),2); // offset med
	memcpy(&(mask[8]),&(hand[4]),4); // offset high

	mask = (uint8_t *)int_gate;
	memcpy(&(mask[0]),&(hand[0]),2); // offset low
	memcpy(&(mask[6]),&(hand[2]),2); // offset med
	memcpy(&(mask[8]),&(hand[4]),4); // offset high

	PrintDebug(vm,VCORE_NONE,"hvm: Adding default null trap and int gates\n");
    }

    for (i=0;i<32;i++) { 
	v3_write_gpa_memory(&vm->cores[0],(addr_t)(base+i*16),16,(uint8_t*)trap_gate);
    }

    for (i=32;i<256;i++) { 
	v3_write_gpa_memory(&vm->cores[0],(addr_t)(base+i*16),16,(uint8_t*)int_gate);
    }

    PrintDebug(vm,VCORE_NONE,"hvm: wrote IDT at %p\n",base);
}



static void get_gdt_loc(struct v3_vm_info *vm, void **base, uint64_t *limit)
{
    *base = (void*)PAGE_ADDR(boot_state_end_addr(vm) - 3 * PAGE_SIZE);
    *limit = 8*3;
}

static uint64_t gdt64[3] = {
    0x0000000000000000, /* null */
    0x00a09a0000000000, /* code (note lme bit) */
    0x00a0920000000000, /* data (most entries don't matter) */
};

static void write_gdt(struct v3_vm_info *vm)
{
    void *base;
    uint64_t limit;

    get_gdt_loc(vm,&base,&limit);
    v3_write_gpa_memory(&vm->cores[0],(addr_t)base,limit,(uint8_t*) gdt64);

    PrintDebug(vm,VCORE_NONE,"hvm: wrote GDT at %p\n",base);
}



static void get_tss_loc(struct v3_vm_info *vm, void **base, uint64_t *limit)
{
    *base = (void*)PAGE_ADDR(boot_state_end_addr(vm) - 4 * PAGE_SIZE);
    *limit = PAGE_SIZE;
}

static void write_tss(struct v3_vm_info *vm)
{
    void *base;
    uint64_t limit;

    get_tss_loc(vm,&base,&limit);

    v3_set_gpa_memory(&vm->cores[0],(addr_t)base,limit,0);

    PrintDebug(vm,VCORE_NONE,"hvm: wrote TSS at %p\n",base);
}


#define TOP_HALF_START  0xffff800000000000ULL
#define BOTTOM_HALF_END 0x00007fffffffffffULL


#define L4_UNIT PAGE_SIZE
#define L3_UNIT (512ULL * L4_UNIT)
#define L2_UNIT (512ULL * L3_UNIT)
#define L1_UNIT (512ULL * L2_UNIT)

static void compute_pts_4KB(struct v3_vm_info *vm, 
			    uint64_t *l1, uint64_t *l2, uint64_t *l3, uint64_t *l4)    
{

    // we map the physical memory up to max_mem_mapped either at 0x0 or at TOP_HALF start
    // that is, it either fills in the first 256 rows of PML4 or the last 256 rows of PML4
    // so it is the same number of page tables regardless

    uint64_t max_gva = vm->hvm_state.max_mem_mapped;

    *l1 = 1;  // 1 PML4
    *l2 = CEIL_DIV(CEIL_DIV(max_gva,512ULL*512ULL*4096ULL),512);
    *l3 = CEIL_DIV(CEIL_DIV(max_gva,512ULL*4096ULL),512);
    *l4 = CEIL_DIV(CEIL_DIV(max_gva,4096ULL),512);
}



/*
  PTS MAP using 1 GB pages
  n second levels pts, highest gva, highest address
  1 top level


OR
  
  PTS MAP using 2 MB pages
  n third level pts, highest gva, highest address
  m second level pts, highest gva, highest address
  1 top level pt

OR

  PTS MAP using 4 KB pages
  n 4th level, highest gva, highest address
  m 3rd level, highest gva, hihgest address
  l second level, highest gva, highest address
  1 top level pt

OR
  PTS MAP using 512 GB pages when this becomes available

*/


static void get_pt_loc(struct v3_vm_info *vm, void **base, uint64_t *limit)
{
    uint64_t l1,l2,l3,l4;
    uint64_t num_pt;

    compute_pts_4KB(vm,&l1,&l2,&l3,&l4);

    if (vm->hvm_state.hrt_flags & MB_TAG_MB64_HRT_FLAG_MAP_512GB) { 
	num_pt = l1;
    } else if (vm->hvm_state.hrt_flags & MB_TAG_MB64_HRT_FLAG_MAP_1GB) { 
	num_pt = l1 + l2;
    } else if (vm->hvm_state.hrt_flags & MB_TAG_MB64_HRT_FLAG_MAP_2MB) {
	num_pt = l1 + l2 + l3;
    } else if (vm->hvm_state.hrt_flags & MB_TAG_MB64_HRT_FLAG_MAP_4KB) { 
	num_pt = l1 + l2 + l3 + l4;
    } else {
	PrintError(vm,VCORE_NONE,"hvm: Cannot determine PT location flags=0x%llx memsize=0x%llx\n",vm->hvm_state.hrt_flags,(uint64_t)vm->mem_size);
	return;
    }

    *base = (void*)PAGE_ADDR(boot_state_end_addr(vm)-(4+num_pt)*PAGE_SIZE);
    *limit = num_pt*PAGE_SIZE;
}

static void write_pts(struct v3_vm_info *vm)
{
    uint64_t size;
    uint64_t num_l1, num_l2, num_l3, num_l4;
    void *start_l1, *start_l2, *start_l3, *start_l4;
    uint64_t max_level;
    void *cur_pt;
    void *cur_gva;
    void *cur_gpa;
    void *min_gpa = 0;
    void *max_gpa = (void*) vm->hvm_state.max_mem_mapped;
    void *min_gva = (void*) vm->hvm_state.gva_offset;
#ifdef V3_CONFIG_DEBUG_HVM
    void *max_gva = min_gva+vm->hvm_state.max_mem_mapped;
#endif
    uint64_t i, pt;
    uint64_t i_start,i_end;
    
    struct pml4e64 *pml4e;
    struct pdpe64 *pdpe;
    struct pde64 *pde;
    struct pte64 *pte;

    if (vm->hvm_state.hrt_flags & MB_TAG_MB64_HRT_FLAG_MAP_512GB) { 
	PrintError(vm,VCORE_NONE,"hvm: Attempt to build 512 GB pages\n");
	max_level = 1;
    } else if (vm->hvm_state.hrt_flags & MB_TAG_MB64_HRT_FLAG_MAP_1GB) { 
	max_level = 2;
    } else if (vm->hvm_state.hrt_flags & MB_TAG_MB64_HRT_FLAG_MAP_2MB) {
	max_level = 3;
    } else if (vm->hvm_state.hrt_flags & MB_TAG_MB64_HRT_FLAG_MAP_4KB) { 
	max_level = 4;
    } else {
	PrintError(vm,VCORE_NONE,"hvm: Cannot determine PT levels\n");
	return;
    }

    get_pt_loc(vm,&start_l1,&size);
    compute_pts_4KB(vm,&num_l1,&num_l2,&num_l3,&num_l4);

    start_l2=start_l1+PAGE_SIZE*num_l1;
    start_l3=start_l2+PAGE_SIZE*num_l2;
    start_l4=start_l3+PAGE_SIZE*num_l3;

    PrintDebug(vm,VCORE_NONE,"hvm: writing %llu levels of PTs start at address %p\n", max_level,start_l1);
    PrintDebug(vm,VCORE_NONE,"hvm: min_gva=%p, max_gva=%p, min_gpa=%p, max_gpa=%p\n",min_gva,max_gva,min_gpa,max_gpa);
    PrintDebug(vm,VCORE_NONE,"hvm: num_l1=%llu, num_l2=%llu, num_l3=%llu, num_l4=%llu\n", num_l1, num_l2, num_l3, num_l4);
    PrintDebug(vm,VCORE_NONE,"hvm: start_l1=%p, start_l2=%p, start_l3=%p, start_l4=%p\n", start_l1, start_l2, start_l3, start_l4);

    cur_pt=start_l1;

    // build PML4 (only one)
    if (v3_gpa_to_hva(&vm->cores[0],(addr_t)cur_pt,(addr_t*)&pml4e)) { 
	PrintError(vm,VCORE_NONE,"hvm: Cannot translate pml4 location\n");
	return;
    }

    memset(pml4e,0,PAGE_SIZE);

    if (min_gva==0x0) { 
	i_start=0; i_end = num_l2;
    } else if (min_gva==(void*)TOP_HALF_START) { 
	i_start=256; i_end=256+num_l2;
    } else {
	PrintError(vm,VCORE_NONE,"hvm: unsupported gva offset\n");
	return;
    }

    for (i=i_start, cur_gva=min_gva, cur_gpa=min_gpa;
	 (i<i_end);
	 i++, cur_gva+=L1_UNIT, cur_gpa+=L1_UNIT) {

	pml4e[i].present=1;
	pml4e[i].writable=1;
	
	if (max_level==1) { 
	    PrintError(vm,VCORE_NONE,"hvm: Intel has not yet defined a PML4E large page\n");
	    pml4e[i].pdp_base_addr = PAGE_BASE_ADDR((addr_t)(cur_gpa));
	    //PrintDebug(vm,VCORE_NONE,"hvm: pml4: gva %p to frame 0%llx\n", cur_gva, (uint64_t)pml4e[i].pdp_base_addr);
	} else {
	    pml4e[i].pdp_base_addr = PAGE_BASE_ADDR((addr_t)(start_l2+(i-i_start)*PAGE_SIZE));
	    //PrintDebug(vm,VCORE_NONE,"hvm: pml4: gva %p to frame 0%llx\n", cur_gva, (uint64_t)pml4e[i].pdp_base_addr);
	}
    }

    // 512 GB only
    if (max_level==1) {
	return;
    }



    for (cur_pt=start_l2, pt=0, cur_gpa=min_gpa, cur_gva=min_gva;
	 pt<num_l2;
	 cur_pt+=PAGE_SIZE, pt++) { 

	// build PDPE
	if (v3_gpa_to_hva(&vm->cores[0],(addr_t)cur_pt,(addr_t*)&pdpe)) { 
	    PrintError(vm,VCORE_NONE,"hvm: Cannot translate pdpe location\n");
	    return;
	}
	
	memset(pdpe,0,PAGE_SIZE);
	
	for (i=0; 
	     i<512 && cur_gpa<max_gpa; 
	     i++, cur_gva+=L2_UNIT, cur_gpa+=L2_UNIT) {

	    pdpe[i].present=1;
	    pdpe[i].writable=1;
	
	    if (max_level==2) { 
		pdpe[i].large_page=1;
		pdpe[i].pd_base_addr = PAGE_BASE_ADDR((addr_t)(cur_gpa));
		//PrintDebug(vm,VCORE_NONE,"hvm: pdpe: gva %p to frame 0%llx\n", cur_gva, (uint64_t)pdpe[i].pd_base_addr);
	    } else {
		pdpe[i].pd_base_addr = PAGE_BASE_ADDR((addr_t)(start_l3+(pt*512+i)*PAGE_SIZE));
		//PrintDebug(vm,VCORE_NONE,"hvm: pdpe: gva %p to frame 0%llx\n", cur_gva, (uint64_t)pdpe[i].pd_base_addr);
	    }
	}
    }
	
    //1 GB only
    if (max_level==2) { 
	return;
    }

    for (cur_pt=start_l3, pt=0, cur_gpa=min_gpa, cur_gva=min_gva;
	 pt<num_l3;
	 cur_pt+=PAGE_SIZE, pt++) { 

	// build PDE
	if (v3_gpa_to_hva(&vm->cores[0],(addr_t)cur_pt,(addr_t*)&pde)) { 
	    PrintError(vm,VCORE_NONE,"hvm: Cannot translate pde location\n");
	    return;
	}
	
	memset(pde,0,PAGE_SIZE);
	
	for (i=0; 
	     i<512 && cur_gpa<max_gpa; 
	     i++, cur_gva+=L3_UNIT, cur_gpa+=L3_UNIT) {

	    pde[i].present=1;
	    pde[i].writable=1;
	
	    if (max_level==3) { 
		pde[i].large_page=1;
		pde[i].pt_base_addr = PAGE_BASE_ADDR((addr_t)(cur_gpa));
		//PrintDebug(vm,VCORE_NONE,"hvm: pde: gva %p to frame 0%llx\n", cur_gva, (uint64_t) pde[i].pt_base_addr);
	    } else {
	        pde[i].pt_base_addr = PAGE_BASE_ADDR((addr_t)(start_l4+(pt*512+i)*PAGE_SIZE));
	        //PrintDebug(vm,VCORE_NONE,"hvm: pde: gva %p to frame 0%llx\n", cur_gva, (uint64_t)pde[i].pt_base_addr);
	    }
	}
    }

    //2 MB only
    if (max_level==3) { 
	return;
    }


    // 4 KB
    for (cur_pt=start_l4, pt=0, cur_gpa=min_gpa, cur_gva=min_gva;
	 pt<num_l4;
	 cur_pt+=PAGE_SIZE, pt++) { 

	// build PTE
	if (v3_gpa_to_hva(&vm->cores[0],(addr_t)cur_pt,(addr_t*)&pte)) { 
	    PrintError(vm,VCORE_NONE,"hvm: Cannot translate pte location\n");
	    return;
	}
	
	memset(pte,0,PAGE_SIZE);
	
	for (i=0; 
	     i<512 && cur_gpa<max_gpa; 
	     i++, cur_gva+=L4_UNIT, cur_gpa+=L4_UNIT) {

	    pte[i].present=1;
	    pte[i].writable=1;
	    pte[i].page_base_addr = PAGE_BASE_ADDR((addr_t)(cur_gpa));
	    //PrintDebug(vm,VCORE_NONE,"hvm: pte: gva %p to frame 0%llx\n", cur_gva, (uint64_t)pte[i].page_base_addr);
	}
    }

    return;
}


static void get_mb_info_loc(struct v3_vm_info *vm, void **base, uint64_t *limit)
{
    
    get_pt_loc(vm,base, limit);
    *base-=PAGE_SIZE;
    *limit=PAGE_SIZE;
}


int v3_build_hrt_multiboot_tag(struct guest_info *core, mb_info_hrt_t *hrt)
{
    struct v3_vm_info *vm = core->vm_info;

    hrt->tag.type = MB_INFO_HRT_TAG;
    hrt->tag.size = sizeof(mb_info_hrt_t);

    hrt->total_num_apics = vm->num_cores;
    hrt->first_hrt_apic_id = vm->hvm_state.first_hrt_core;
    hrt->have_hrt_ioapic=0;
    hrt->first_hrt_ioapic_entry=0;

    hrt->cpu_freq_khz = V3_CPU_KHZ();

    hrt->hrt_flags = vm->hvm_state.hrt_flags;
    hrt->max_mem_mapped = vm->hvm_state.max_mem_mapped;
    hrt->first_hrt_gpa = vm->hvm_state.first_hrt_gpa;
    hrt->gva_offset = vm->hvm_state.gva_offset;
    hrt->comm_page_gpa = vm->hvm_state.comm_page_gpa;
    hrt->hrt_int_vector = vm->hvm_state.hrt_int_vector;
    
    return 0;
}

static void write_mb_info(struct v3_vm_info *vm) 
{
    if (vm->hvm_state.hrt_type!=HRT_MBOOT64) { 
	PrintError(vm, VCORE_NONE,"hvm: Cannot handle this HRT type\n");
	return;
    } else {
	uint8_t buf[256];
	uint64_t size;
	void *base;
	uint64_t limit;

	get_mb_info_loc(vm,&base,&limit);
	
	if ((size=v3_build_multiboot_table(&vm->cores[vm->hvm_state.first_hrt_core],buf,256))==-1) { 
	    PrintError(vm,VCORE_NONE,"hvm: Failed to build MB info\n");
	    return;
	}

	if (size>limit) { 
	    PrintError(vm,VCORE_NONE,"hvm: MB info is too large\n");
	    return;
	}
	
	v3_write_gpa_memory(&vm->cores[vm->hvm_state.first_hrt_core],
			    (addr_t)base,
			    size,
			    buf);

	PrintDebug(vm,VCORE_NONE, "hvm: wrote MB info at %p\n", base);
    }
}

#define SCRATCH_STACK_SIZE 4096


static void get_hrt_loc(struct v3_vm_info *vm, void **base, uint64_t *limit)
{
    void *mb_base;
    uint64_t mb_limit;
    
    get_mb_info_loc(vm,&mb_base,&mb_limit);
    
    mb_base-=SCRATCH_STACK_SIZE*v3_get_hvm_hrt_cores(vm);

    *base = (void*)PAGE_ADDR(vm->hvm_state.first_hrt_gpa);

    if (mb_base < *base+PAGE_SIZE) { 
	PrintError(vm,VCORE_NONE,"hvm: HRT stack colides with HRT\n");
    }

    *limit = mb_base - *base;
}


#define ERROR(fmt, args...) PrintError(VM_NONE,VCORE_NONE,"hvm: " fmt,##args)
#define INFO(fmt, args...) PrintDebug(VM_NONE,VCORE_NONE,"hvm: " fmt,##args)

#define ELF_MAGIC    0x464c457f
#define MB2_MAGIC    0xe85250d6

#define MB2_INFO_MAGIC    0x36d76289

static int is_elf(uint8_t *data, uint64_t size)
{
    if (*((uint32_t*)data)==ELF_MAGIC) {
	return 1;
    } else { 
	return 0;
    }
}

static mb_header_t *find_mb_header(uint8_t *data, uint64_t size)
{
    uint64_t limit = size > 32768 ? 32768 : size;
    uint64_t i;

    // Scan for the .boot magic cookie
    // must be in first 32K, assume 4 byte aligned
    for (i=0;i<limit;i+=4) { 
	if (*((uint32_t*)&data[i])==MB2_MAGIC) {
	    INFO("Found multiboot header at offset 0x%llx\n",i);
	    return (mb_header_t *) &data[i];
	}
    }
    return 0;
}


static int configure_hrt(struct v3_vm_info *vm, mb_data_t *mb)
{
    struct v3_vm_hvm *h = &vm->hvm_state;
    uint64_t f = mb->mb64_hrt->hrt_flags;
    uint64_t maxmap = mb->mb64_hrt->max_mem_to_map;
    uint64_t gvaoff = mb->mb64_hrt->gva_offset;
    uint64_t gvaentry = mb->mb64_hrt->gva_entry;
    uint64_t commgpa = mb->mb64_hrt->comm_page_gpa;
    uint8_t  vec = mb->mb64_hrt->hrt_int_vector;
    

    PrintDebug(vm,VCORE_NONE,"hvm: HRT request: flags=0x%llx max_map=0x%llx gva_off=%llx gva_entry=%llx comm_page=0x%llx vector=0x%x\n",
	       f, maxmap, gvaoff,gvaentry,commgpa, vec);

    if (maxmap<0x100000000ULL) { 
	PrintDebug(vm,VCORE_NONE,"hvm: revising request up to 4 GB max map\n");
	maxmap=0x100000000ULL;
    }

    if (f & MB_TAG_MB64_HRT_FLAG_MAP_512GB) { 
	PrintError(vm,VCORE_NONE,"hvm: support for 512 GB pages is not yet available in hardware\n");
	return -1;
    } else if (f & MB_TAG_MB64_HRT_FLAG_MAP_1GB) { 
	f &= ~0x3c;
	f |= MB_TAG_MB64_HRT_FLAG_MAP_1GB;
	h->max_mem_mapped = maxmap;
	PrintDebug(vm,VCORE_NONE,"hvm: 1 GB pages selected\n");
    } else if (f & MB_TAG_MB64_HRT_FLAG_MAP_2MB) { 
	f &= ~0x3c;
	f |= MB_TAG_MB64_HRT_FLAG_MAP_2MB;
	h->max_mem_mapped = maxmap;
	PrintDebug(vm,VCORE_NONE,"hvm: 2 MB pages selected\n");
    } else if (f & MB_TAG_MB64_HRT_FLAG_MAP_4KB) { 
	f &= ~0x3c;
	f |= MB_TAG_MB64_HRT_FLAG_MAP_4KB;
	h->max_mem_mapped = maxmap;
	PrintDebug(vm,VCORE_NONE,"hvm: 4 KB pages selected\n");
    } else {
	PrintError(vm,VCORE_NONE,"hvm: no page table model is requested\n");
	return -1;
    }

    if (f & MB_TAG_MB64_HRT_FLAG_RELOC) {
	PrintError(vm,VCORE_NONE,"hvm: relocatable hrt not currently supported\n");
	return -1;
    }

    h->hrt_flags = f;

    if (maxmap>h->max_mem_mapped) { 
	PrintError(vm,VCORE_NONE,"hvm: requested 0x%llx bytes mapped, which is more than currently supported\n",maxmap);
	return -1;
    }

    if (gvaoff!=0 && gvaoff!=TOP_HALF_START) { 
	PrintError(vm,VCORE_NONE,"hvm: currently only GVA offsets of 0 and %llx are supported\n", TOP_HALF_START);
	return -1;
    }
    
    h->gva_offset = gvaoff;

    h->gva_entry = gvaentry;

    if (mb->addr->load_addr < h->first_hrt_gpa) { 
	PrintError(vm,VCORE_NONE,"hvm: load start address of HRT is below first HRT GPA\n");
	return -1;
    }
    
    if (mb->addr->bss_end_addr > (vm->mem_size-(1024*1024*64))) {
	PrintError(vm,VCORE_NONE,"hvm: bss end address of HRT above last allowed GPA\n");
	return -1;
    }
    
    if (vec<32) { 
	PrintError(vm,VCORE_NONE,"hvm: cannot support vector %x\n",vec);
	return -1;
    }
    
    h->hrt_int_vector = vec;
    
    
    if (commgpa < vm->mem_size) { 
	PrintError(vm,VCORE_NONE,"hvm: cannot map comm page over physical memory\n");
	return -1;
    } 

    h->comm_page_gpa = commgpa;

    if (!h->comm_page_hpa) { 
	if (!(h->comm_page_hpa=V3_AllocPages(1))) { 
	    PrintError(vm,VCORE_NONE,"hvm: unable to allocate space for comm page\n");
	    return -1;
	}

	h->comm_page_hva = V3_VAddr(h->comm_page_hpa);
	
	memset(h->comm_page_hva,0,PAGE_SIZE_4KB);
	
	if (v3_add_shadow_mem(vm,-1,h->comm_page_gpa,h->comm_page_gpa+PAGE_SIZE_4KB,(addr_t)h->comm_page_hpa)) { 
	    PrintError(vm,VCORE_NONE,"hvm: unable to map communication page\n");
	    V3_FreePages((void*)(h->comm_page_gpa),1);
	    return -1;
	}
	
	
	PrintDebug(vm,VCORE_NONE,"hvm: added comm page for first time\n");
    }

    memset(h->comm_page_hva,0,PAGE_SIZE_4KB);
    
    
    PrintDebug(vm,VCORE_NONE,"hvm: HRT configuration: flags=0x%llx max_mem_mapped=0x%llx gva_offset=0x%llx gva_entry=0x%llx comm_page=0x%llx vector=0x%x\n",
 	       h->hrt_flags,h->max_mem_mapped, h->gva_offset,h->gva_entry, h->comm_page_gpa, h->hrt_int_vector);
    
    return 0;

}

static int setup_mb_kernel_hrt(struct v3_vm_info *vm)
{
    mb_data_t mb;

    if (v3_parse_multiboot_header(vm->hvm_state.hrt_file,&mb)) { 
	PrintError(vm,VCORE_NONE, "hvm: failed to parse multiboot kernel header\n");
	return -1;
    }

    if (configure_hrt(vm,&mb)) {
	PrintError(vm,VCORE_NONE, "hvm: cannot configure HRT\n");
	return -1;
    }
    
    if (v3_write_multiboot_kernel(vm,&mb,vm->hvm_state.hrt_file,
				  (void*)vm->hvm_state.first_hrt_gpa,
				  vm->mem_size-vm->hvm_state.first_hrt_gpa)) {
	PrintError(vm,VCORE_NONE, "hvm: failed to write multiboot kernel into memory\n");
	return -1;
    }

    if (vm->hvm_state.gva_entry) { 
	vm->hvm_state.hrt_entry_addr = vm->hvm_state.gva_entry;
    } else {
	vm->hvm_state.hrt_entry_addr = (uint64_t) mb.entry->entry_addr + vm->hvm_state.gva_offset;
    }

    vm->hvm_state.hrt_type = HRT_MBOOT64;

    return 0;

}


static int setup_hrt(struct v3_vm_info *vm)
{
    if (is_elf(vm->hvm_state.hrt_file->data,vm->hvm_state.hrt_file->size) && 
	find_mb_header(vm->hvm_state.hrt_file->data,vm->hvm_state.hrt_file->size)) { 

	PrintDebug(vm,VCORE_NONE,"hvm: appears to be a multiboot kernel\n");
	if (setup_mb_kernel_hrt(vm)) { 
	    PrintError(vm,VCORE_NONE,"hvm: multiboot kernel setup failed\n");
	    return -1;
	} 
    } else {
	PrintError(vm,VCORE_NONE,"hvm: supplied HRT is not a multiboot kernel\n");
	return -1;
    }

    return 0;
}


	

/*
  GPA layout:

  HRT
  ---
  ROS

  We do not touch the ROS portion of the address space.
  The HRT portion looks like:

  INT_HANDLER (1 page - page aligned)
  IDT (1 page - page aligned)
  GDT (1 page - page aligned)
  TSS (1 page - page asligned)
  PAGETABLES  (identy map of first N GB)
     ROOT PT first (lowest memory addr), followed by 2nd level PTs in order,
     followed by 3rd level PTs in order, followed by 4th level
     PTs in order.  
  MBINFO_PAGE
  SCRATCH_STACK_HRT_CORE0 
  SCRATCH_STACK_HRT_CORE1
  ..
  SCRATCH_STACK_HRT_COREN
  ...
  HRT (as many pages as needed, page-aligned, starting at first HRT address)
  ---
  ROS


*/


int v3_setup_hvm_vm_for_boot(struct v3_vm_info *vm)
{
    if (!vm->hvm_state.is_hvm) { 
	PrintDebug(vm,VCORE_NONE,"hvm: skipping HVM setup for boot as this is not an HVM\n");
	return 0;
    }

    PrintDebug(vm,VCORE_NONE,"hvm: setup of HVM memory begins\n");

    if (setup_hrt(vm)) {
	PrintError(vm,VCORE_NONE,"hvm: failed to setup HRT\n");
	return -1;
    } 

    // the locations of all the other items are determined by
    // the HRT setup, so these must happen after

    write_null_int_handler(vm);
    write_idt(vm);
    write_gdt(vm);
    write_tss(vm);

    write_pts(vm);

    // this must happen last
    write_mb_info(vm);

    PrintDebug(vm,VCORE_NONE,"hvm: setup of HVM memory done\n");

    return 0;
}

/*
  On entry for every core:

   IDTR points to stub IDT
   GDTR points to stub GDT
   TS   points to stub TSS
   CR3 points to root page table
   CR0 has PE and PG
   EFER has LME AND LMA (and NX for compatibility with Linux)
   RSP is TOS of core's scratch stack (looks like a call)

   RAX = MB magic cookie
   RBX = address of multiboot info table
   RCX = this core id / apic id (0..N-1)
   RDX = this core id - first HRT core ID (==0 for the first HRT core)

   All addresses are virtual addresses, offset as needed by gva_offset

   Other regs are zeroed

   shadow/nested paging state reset for long mode

*/
int v3_setup_hvm_hrt_core_for_boot(struct guest_info *core)
{
    void *base;
    uint64_t limit;
    uint64_t gva_offset;

    rdtscll(core->hvm_state.last_boot_start);
    

    if (!core->hvm_state.is_hrt) { 
	PrintDebug(core->vm_info,core,"hvm: skipping HRT setup for core %u as it is not an HRT core\n", core->vcpu_id);
	return 0;
    }


    PrintDebug(core->vm_info, core, "hvm: setting up HRT core (%u) for boot\n", core->vcpu_id);

    gva_offset = core->vm_info->hvm_state.gva_offset;
    
    memset(&core->vm_regs,0,sizeof(core->vm_regs));
    memset(&core->ctrl_regs,0,sizeof(core->ctrl_regs));
    memset(&core->dbg_regs,0,sizeof(core->dbg_regs));
    memset(&core->segments,0,sizeof(core->segments));    
    memset(&core->msrs,0,sizeof(core->msrs));    
    memset(&core->fp_state,0,sizeof(core->fp_state));    

    // We are in long mode with virtual memory and we want
    // to start immediatley
    core->cpl = 0; // we are going right into the kernel
    core->cpu_mode = LONG;
    core->mem_mode = VIRTUAL_MEM; 
    core->core_run_state = CORE_RUNNING ;


    // magic
    core->vm_regs.rax = MB2_INFO_MAGIC;

    // multiboot info pointer
    get_mb_info_loc(core->vm_info, &base,&limit);
    core->vm_regs.rbx = (uint64_t) base + gva_offset;  

    // core number
    core->vm_regs.rcx = core->vcpu_id;
    
    // HRT core number
    core->vm_regs.rdx = core->vcpu_id - core->vm_info->hvm_state.first_hrt_core;

    // Now point to scratch stack for this core
    // it begins at an ofset relative to the MB info page
    get_mb_info_loc(core->vm_info, &base,&limit);
    base = base + gva_offset;
    base -= core->vm_regs.rdx * SCRATCH_STACK_SIZE;
    core->vm_regs.rsp = (v3_reg_t) base;  
    core->vm_regs.rbp = (v3_reg_t) base-8; 

    // push onto the stack a bad rbp and bad return address
    core->vm_regs.rsp-=16;
    v3_set_gpa_memory(core,
		      core->vm_regs.rsp-gva_offset,
		      16,
		      0xff);


    // HRT entry point
    get_hrt_loc(core->vm_info, &base,&limit);
    if (core->vm_info->hvm_state.gva_entry) { 
      core->rip = core->vm_info->hvm_state.gva_entry;
    } else {
      core->rip = (uint64_t) core->vm_info->hvm_state.hrt_entry_addr + gva_offset; 
    }
      


    PrintDebug(core->vm_info,core,"hvm: hrt core %u has rip=%p, rsp=%p, rbp=%p, rax=%p, rbx=%p, rcx=%p, rdx=%p\n",
	       (core->vcpu_id - core->vm_info->hvm_state.first_hrt_core),
	       (void*)(core->rip),
	       (void*)(core->vm_regs.rsp),
	       (void*)(core->vm_regs.rbp),
	       (void*)(core->vm_regs.rax),
	       (void*)(core->vm_regs.rbx),
	       (void*)(core->vm_regs.rcx),
	       (void*)(core->vm_regs.rdx));

    // Setup CRs for long mode and our stub page table
    // CR0: PG, PE
    core->ctrl_regs.cr0 = 0x80000001;
    core->shdw_pg_state.guest_cr0 = core->ctrl_regs.cr0;

    // CR2: don't care (output from #PF)
    // CE3: set to our PML4E, without setting PCD or PWT
    get_pt_loc(core->vm_info, &base,&limit);
    core->ctrl_regs.cr3 = PAGE_ADDR((addr_t)base);  // not offset as this is a GPA
    core->shdw_pg_state.guest_cr3 = core->ctrl_regs.cr3;

    // CR4: PGE, PAE, PSE (last byte: 1 0 1 1 0 0 0 0)
    core->ctrl_regs.cr4 = 0xb0;
    core->shdw_pg_state.guest_cr4 = core->ctrl_regs.cr4;
    // CR8 as usual
    // RFLAGS zeroed is fine: come in with interrupts off
    // EFER needs SVME LMA LME (last 16 bits: 0 0 0 1 1 1 0 1 0 0 0 0 0 0 0 0
    core->ctrl_regs.efer = 0x1d00;
    core->shdw_pg_state.guest_efer.value = core->ctrl_regs.efer;


    /* 
       Notes on selectors:

       selector is 13 bits of index, 1 bit table indicator 
       (0=>GDT), 2 bit RPL
       
       index is scaled by 8, even in long mode, where some entries 
       are 16 bytes long.... 
          -> code, data descriptors have 8 byte format
             because base, limit, etc, are ignored (no segmentation)
          -> interrupt/trap gates have 16 byte format 
             because offset needs to be 64 bits
    */
    
    // Install our stub IDT
    get_idt_loc(core->vm_info, &base,&limit);
    base += gva_offset;
    core->segments.idtr.selector = 0;  // entry 0 (NULL) of the GDT
    core->segments.idtr.base = (addr_t) base;  // only base+limit are used
    core->segments.idtr.limit = limit-1;
    core->segments.idtr.type = 0x0;
    core->segments.idtr.system = 0; 
    core->segments.idtr.dpl = 0;
    core->segments.idtr.present = 0;
    core->segments.idtr.long_mode = 0;

    // Install our stub GDT
    get_gdt_loc(core->vm_info, &base,&limit);
    base += gva_offset;
    core->segments.gdtr.selector = 0;  // entry 0 (NULL) of the GDT
    core->segments.gdtr.base = (addr_t) base;
    core->segments.gdtr.limit = limit-1;   // only base+limit are used
    core->segments.gdtr.type = 0x0;
    core->segments.gdtr.system = 0; 
    core->segments.gdtr.dpl = 0;
    core->segments.gdtr.present = 0;
    core->segments.gdtr.long_mode = 0;
    
    // And our TSS
    get_tss_loc(core->vm_info, &base,&limit);
    base += gva_offset;  
    core->segments.tr.selector = 0;
    core->segments.tr.base = (addr_t) base;
    core->segments.tr.limit = limit-1;
    core->segments.tr.type = 0x9;
    core->segments.tr.system = 0;   // available 64 bit TSS 
    core->segments.tr.dpl = 0;
    core->segments.tr.present = 1;
    core->segments.tr.long_mode = 0; // not used
    
    base = 0x0; // these are not offset as we want to make all gvas visible
    limit = -1;

    // And CS
    core->segments.cs.selector = 0x8 ; // entry 1 of GDT (RPL=0)
    core->segments.cs.base = (addr_t) base;   // not used
    core->segments.cs.limit = limit;          // not used
    core->segments.cs.type = 0xe;             // only C is used
    core->segments.cs.system = 1;             // not a system segment
    core->segments.cs.dpl = 0;                       
    core->segments.cs.present = 1;
    core->segments.cs.long_mode = 1;

    // DS, SS, etc are identical
    core->segments.ds.selector = 0x10; // entry 2 of GDT (RPL=0)
    core->segments.ds.base = (addr_t) base;
    core->segments.ds.limit = limit;
    core->segments.ds.type = 0x6;            // ignored
    core->segments.ds.system = 1;            // not a system segment
    core->segments.ds.dpl = 0;
    core->segments.ds.present = 1;
    core->segments.ds.long_mode = 1;
    
    memcpy(&core->segments.ss,&core->segments.ds,sizeof(core->segments.ds));
    memcpy(&core->segments.es,&core->segments.ds,sizeof(core->segments.ds));
    memcpy(&core->segments.fs,&core->segments.ds,sizeof(core->segments.ds));
    memcpy(&core->segments.gs,&core->segments.ds,sizeof(core->segments.ds));
    

    // reset paging here for shadow... 

    if (core->shdw_pg_mode != NESTED_PAGING) { 
	PrintError(core->vm_info, core, "hvm: shadow paging guest... this will end badly\n");
	return -1;
    }


    return 0;
}

int v3_handle_hvm_reset(struct guest_info *core)
{

    if (core->core_run_state != CORE_RESETTING) { 
	return 0;
    }

    if (!core->vm_info->hvm_state.is_hvm) { 
	return 0;
    }

    if (v3_is_hvm_hrt_core(core)) { 
	// this is an HRT reset
	int rc=0;

	// wait for all the HRT cores
	v3_counting_barrier(&core->vm_info->reset_barrier);

	if (core->vcpu_id==core->vm_info->hvm_state.first_hrt_core) { 
	    // I am leader
	    core->vm_info->run_state = VM_RESETTING;
	}

	core->core_run_state = CORE_RESETTING;

	if (core->vcpu_id==core->vm_info->hvm_state.first_hrt_core) {
	    // we really only need to clear the bss
	    // and recopy the .data, but for now we'll just
	    // do everything
	    rc |= v3_setup_hvm_vm_for_boot(core->vm_info);

	    if (rc) { 
		PrintError(core->vm_info,core,"hvm: failed to setup HVM VM for boot rc=%d\n",rc);
	    }
	}

	// now everyone is ready to reset
	rc |= v3_setup_hvm_hrt_core_for_boot(core);

	if (rc) { 
	    PrintError(core->vm_info,core,"hvm: failed to setup HVM core for boot rc=%d\n",rc);
	}

	core->core_run_state = CORE_RUNNING;

	if (core->vcpu_id==core->vm_info->hvm_state.first_hrt_core) { 
	    // leader
	    core->vm_info->run_state = VM_RUNNING;
            core->vm_info->hvm_state.trans_state = HRT_IDLE;
	}

	v3_counting_barrier(&core->vm_info->reset_barrier);

	if (rc<0) { 
	    PrintError(core->vm_info,core,"hvm: reset failed\n");
	    return rc;
	} else {
	    return 1;
	}

    } else { 
	// ROS core will be handled by normal reset functionality
	return 0;
    }
}
