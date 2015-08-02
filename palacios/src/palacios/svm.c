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
 *         Peter Dinda <jarusl@cs.northwestern.edu> (Reset)
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */



#include <palacios/svm.h>
#include <palacios/vmm.h>

#include <palacios/vmcb.h>
#include <palacios/vmm_mem.h>
#include <palacios/vmm_paging.h>
#include <palacios/svm_handler.h>

#include <palacios/vmm_debug.h>
#include <palacios/vm_guest_mem.h>

#include <palacios/vmm_decoder.h>
#include <palacios/vmm_string.h>
#include <palacios/vmm_lowlevel.h>
#include <palacios/svm_msr.h>

#include <palacios/vmm_rbtree.h>
#include <palacios/vmm_barrier.h>
#include <palacios/vmm_debug.h>

#include <palacios/vmm_perftune.h>

#include <palacios/vmm_bios.h>


#ifdef V3_CONFIG_CHECKPOINT
#include <palacios/vmm_checkpoint.h>
#endif

#include <palacios/vmm_direct_paging.h>

#include <palacios/vmm_ctrl_regs.h>
#include <palacios/svm_io.h>

#include <palacios/vmm_sprintf.h>

#ifdef V3_CONFIG_MEM_TRACK
#include <palacios/vmm_mem_track.h>
#endif 

#ifdef V3_CONFIG_TM_FUNC
#include <extensions/trans_mem.h>
#endif

#ifndef V3_CONFIG_DEBUG_SVM
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif



uint32_t v3_last_exit;

// This is a global pointer to the host's VMCB
// These are physical addresses
static addr_t host_vmcbs[V3_CONFIG_MAX_CPUS] = { [0 ... V3_CONFIG_MAX_CPUS - 1] = 0};



extern void v3_stgi();
extern void v3_clgi();
//extern int v3_svm_launch(vmcb_t * vmcb, struct v3_gprs * vm_regs, uint64_t * fs, uint64_t * gs);
extern int v3_svm_launch(vmcb_t * vmcb, struct v3_gprs * vm_regs, vmcb_t * host_vmcb);



static vmcb_t * Allocate_VMCB() {
    vmcb_t * vmcb_page = NULL;
    addr_t vmcb_pa = (addr_t)V3_AllocPages(1);   // need not be shadow safe, not exposed to guest

    if ((void *)vmcb_pa == NULL) {
      PrintError(VM_NONE, VCORE_NONE, "Error allocating VMCB\n");
	return NULL;
    }

    vmcb_page = (vmcb_t *)V3_VAddr((void *)vmcb_pa);

    memset(vmcb_page, 0, 4096);

    return vmcb_page;
}


static int v3_svm_handle_efer_write(struct guest_info * core, uint_t msr, struct v3_msr src, void * priv_data)
{
    int status;

    // Call arch-independent handler
    if ((status = v3_handle_efer_write(core, msr, src, priv_data)) != 0) {
	return status;
    }

    // SVM-specific code
    {
	// Ensure that hardware visible EFER.SVME bit is set (SVM Enable)
	struct efer_64 * hw_efer = (struct efer_64 *)&(core->ctrl_regs.efer);
	hw_efer->svme = 1;
    }

    return 0;
}

/*
 * This is invoked both on an initial boot and on a reset
 * 
 * The difference is that on a reset we will not rehook anything
 *
 */

static void Init_VMCB_BIOS(vmcb_t * vmcb, struct guest_info * core) {
    vmcb_ctrl_t * ctrl_area = GET_VMCB_CTRL_AREA(vmcb);
    vmcb_saved_state_t * guest_state = GET_VMCB_SAVE_STATE_AREA(vmcb);
    uint_t i;

    if (core->core_run_state!=CORE_INVALID && core->core_run_state!=CORE_RESETTING) { 
	PrintError(core->vm_info, core, "Atempt to Init_VMCB_BIOS in invalid state (%d)\n",core->core_run_state);
	return;
    }

    // need to invalidate any shadow page tables early
    if (core->shdw_pg_mode == SHADOW_PAGING && core->core_run_state==CORE_RESETTING) {
	if (v3_get_vm_cpu_mode(core) != REAL) {
	    if (v3_invalidate_shadow_pts(core) == -1) {
		PrintError(core->vm_info,core,"Could not invalidate shadow page tables\n");
		return;
	    }
	}
    }

    // Guarantee we are starting from a clean slate
    // even on a reset
    memset(vmcb,0,4096);

    ctrl_area->svm_instrs.VMRUN = 1;
    ctrl_area->svm_instrs.VMMCALL = 1;
    ctrl_area->svm_instrs.VMLOAD = 1;
    ctrl_area->svm_instrs.VMSAVE = 1;
    ctrl_area->svm_instrs.STGI = 1;
    ctrl_area->svm_instrs.CLGI = 1;
    ctrl_area->svm_instrs.SKINIT = 1; // secure startup... why
    ctrl_area->svm_instrs.ICEBP = 1;  // in circuit emulator breakpoint
    ctrl_area->svm_instrs.WBINVD = 1; // write back and invalidate caches... why?
    ctrl_area->svm_instrs.MONITOR = 1;
    ctrl_area->svm_instrs.MWAIT_always = 1;
    ctrl_area->svm_instrs.MWAIT_if_armed = 1;
    ctrl_area->instrs.INVLPGA = 1;   // invalidate page in asid... AMD ERRATA
    ctrl_area->instrs.CPUID = 1;

    ctrl_area->instrs.HLT = 1;

    /* Set at VMM launch as needed */
    ctrl_area->instrs.RDTSC = 0;
    ctrl_area->svm_instrs.RDTSCP = 0;


#ifdef V3_CONFIG_TM_FUNC
    v3_tm_set_excp_intercepts(ctrl_area);
#endif
    

    ctrl_area->instrs.NMI = 1;
    ctrl_area->instrs.SMI = 0; // allow SMIs to run in guest
    ctrl_area->instrs.INIT = 1;
    //    ctrl_area->instrs.PAUSE = 1;    // do not care as does not halt
    ctrl_area->instrs.shutdown_evts = 1;


    /* DEBUG FOR RETURN CODE */
    ctrl_area->exit_code = 1;


    /* Setup Guest Machine state */

    memset(&core->vm_regs,0,sizeof(core->vm_regs));
    memset(&core->ctrl_regs,0,sizeof(core->ctrl_regs));
    memset(&core->dbg_regs,0,sizeof(core->dbg_regs));
    memset(&core->segments,0,sizeof(core->segments));    
    memset(&core->msrs,0,sizeof(core->msrs));    
    memset(&core->fp_state,0,sizeof(core->fp_state));    

    // reset interrupts
    core->intr_core_state.irq_pending=0; 
    core->intr_core_state.irq_started=0; 
    core->intr_core_state.swintr_posted=0; 

    // reset exceptions
    core->excp_state.excp_pending=0;

    // reset of gprs to expected values at init
    core->vm_regs.rsp = 0x00;
    core->rip = 0xfff0;
    core->vm_regs.rdx = 0x00000f00;  // family/stepping/etc

    
    core->cpl = 0;

    core->ctrl_regs.rflags = 0x00000002; // The reserved bit is always 1

    core->ctrl_regs.cr0 = 0x60010010; // Set the WP flag so the memory hooks work in real-mode
    core->shdw_pg_state.guest_cr0 = core->ctrl_regs.cr0;

    // cr3 zeroed above
    core->shdw_pg_state.guest_cr3 = core->ctrl_regs.cr3;
    // cr4 zeroed above
    core->shdw_pg_state.guest_cr4 = core->ctrl_regs.cr4;

    core->ctrl_regs.efer |= EFER_MSR_svm_enable ;
    core->shdw_pg_state.guest_efer.value = core->ctrl_regs.efer;

    core->segments.cs.selector = 0xf000;
    core->segments.cs.limit = 0xffff;
    core->segments.cs.base = 0x0000f0000LL;

    // (raw attributes = 0xf3)
    core->segments.cs.type = 0xa;
    core->segments.cs.system = 0x1;
    core->segments.cs.dpl = 0x0;
    core->segments.cs.present = 1;



    struct v3_segment * segregs [] = {&(core->segments.ss), &(core->segments.ds), 
				      &(core->segments.es), &(core->segments.fs), 
				      &(core->segments.gs), NULL};

    for ( i = 0; segregs[i] != NULL; i++) {
	struct v3_segment * seg = segregs[i];
	
	seg->selector = 0x0000;
	//    seg->base = seg->selector << 4;
	seg->base = 0x00000000;
	seg->limit = 0xffff;

	// (raw attributes = 0xf3)
	seg->type = 0x2;
	seg->system = 0x1;
	seg->dpl = 0x0;
	seg->present = 1;
    }

    core->segments.gdtr.selector = 0x0000;
    core->segments.gdtr.limit = 0x0000ffff;
    core->segments.gdtr.base = 0x0000000000000000LL;
    core->segments.gdtr.dpl = 0x0;

    core->segments.idtr.selector = 0x0000; 
    core->segments.idtr.limit = 0x0000ffff;
    core->segments.idtr.base = 0x0000000000000000LL;
    core->segments.ldtr.limit = 0x0000ffff;
    core->segments.ldtr.base = 0x0000000000000000LL;
    core->segments.ldtr.system = 0;
    core->segments.ldtr.type = 0x2;
    core->segments.ldtr.dpl = 0x0;

    core->segments.tr.selector = 0x0000;
    core->segments.tr.limit = 0x0000ffff;
    core->segments.tr.base = 0x0000000000000000LL;
    core->segments.tr.system = 0;
    core->segments.tr.type = 0x3;
    core->segments.tr.dpl = 0x0;

    core->dbg_regs.dr6 = 0x00000000ffff0ff0LL;
    core->dbg_regs.dr7 = 0x0000000000000400LL;


    ctrl_area->IOPM_BASE_PA = (addr_t)V3_PAddr(core->vm_info->io_map.arch_data);
    ctrl_area->instrs.IOIO_PROT = 1;
	    
    ctrl_area->MSRPM_BASE_PA = (addr_t)V3_PAddr(core->vm_info->msr_map.arch_data);
    ctrl_area->instrs.MSR_PROT = 1;   


    ctrl_area->guest_ctrl.V_INTR_MASKING = 1;
    ctrl_area->instrs.INTR = 1;
    // The above also assures the TPR changes (CR8) are only virtual


    // However, we need to see TPR writes since they will
    // affect the virtual apic
    // we reflect out cr8 to ctrl_regs->apic_tpr
    ctrl_area->cr_reads.cr8 = 1;
    ctrl_area->cr_writes.cr8 = 1;
    // We will do all TPR comparisons in the virtual apic
    // We also do not want the V_TPR to be able to mask the PIC
    ctrl_area->guest_ctrl.V_IGN_TPR = 1;

    

    if (core->core_run_state == CORE_INVALID) { 
	v3_hook_msr(core->vm_info, EFER_MSR, 
		    &v3_handle_efer_read,
		    &v3_svm_handle_efer_write, 
		    core);
    }

    if (core->shdw_pg_mode == SHADOW_PAGING) {
	
	/* JRL: This is a performance killer, and a simplistic solution */
	/* We need to fix this */
	ctrl_area->TLB_CONTROL = 1;
	ctrl_area->guest_ASID = 1;
	

	if (core->core_run_state == CORE_INVALID) { 
	    if (v3_init_passthrough_pts(core) == -1) {
		PrintError(core->vm_info, core, "Could not initialize passthrough page tables\n");
		return ;
	    }
	    // the shadow page tables are OK since we have not initialized hem yet
	} else {
	    // CORE_RESETTING
	    // invalidation of shadow page tables happened earlier in this function
	}

	core->shdw_pg_state.guest_cr0 = 0x0000000000000010LL;
	
	core->ctrl_regs.cr0 |= 0x80000000;

        v3_activate_passthrough_pt(core);

	ctrl_area->cr_reads.cr0 = 1;
	ctrl_area->cr_writes.cr0 = 1;
	//intercept cr4 read so shadow pager can use PAE independently of guest
	ctrl_area->cr_reads.cr4 = 1;
	ctrl_area->cr_writes.cr4 = 1;
	ctrl_area->cr_reads.cr3 = 1;
	ctrl_area->cr_writes.cr3 = 1;


	ctrl_area->instrs.INVLPG = 1;

	ctrl_area->exceptions.pf = 1;

	guest_state->g_pat = 0x7040600070406ULL;


    } else if (core->shdw_pg_mode == NESTED_PAGING) {
	// Flush the TLB on entries/exits
	ctrl_area->TLB_CONTROL = 1;
	ctrl_area->guest_ASID = 1;

	// Enable Nested Paging
	ctrl_area->NP_ENABLE = 1;

	// Set the Nested Page Table pointer
	if (core->core_run_state == CORE_INVALID) { 
	    if (v3_init_passthrough_pts(core) == -1) {
		PrintError(core->vm_info, core, "Could not initialize Nested page tables\n");
		return ;
	    }
	} else {
	    // the existing nested page tables will work fine
	}

	ctrl_area->N_CR3 = core->direct_map_pt;

	guest_state->g_pat = 0x7040600070406ULL;
    }
    
    /* tell the guest that we don't support SVM */
    if (core->core_run_state == CORE_INVALID) { 
	v3_hook_msr(core->vm_info, SVM_VM_CR_MSR, 
		    &v3_handle_vm_cr_read,
		    &v3_handle_vm_cr_write, 
		    core);
    }

    if (core->core_run_state == CORE_INVALID) { 
#define INT_PENDING_AMD_MSR		0xc0010055

	v3_hook_msr(core->vm_info, IA32_STAR_MSR, NULL, NULL, NULL);
	v3_hook_msr(core->vm_info, IA32_LSTAR_MSR, NULL, NULL, NULL);
	v3_hook_msr(core->vm_info, IA32_FMASK_MSR, NULL, NULL, NULL);
	v3_hook_msr(core->vm_info, IA32_KERN_GS_BASE_MSR, NULL, NULL, NULL);
	v3_hook_msr(core->vm_info, IA32_CSTAR_MSR, NULL, NULL, NULL);

	v3_hook_msr(core->vm_info, SYSENTER_CS_MSR, NULL, NULL, NULL);
	v3_hook_msr(core->vm_info, SYSENTER_ESP_MSR, NULL, NULL, NULL);
	v3_hook_msr(core->vm_info, SYSENTER_EIP_MSR, NULL, NULL, NULL);


	v3_hook_msr(core->vm_info, FS_BASE_MSR, NULL, NULL, NULL);
	v3_hook_msr(core->vm_info, GS_BASE_MSR, NULL, NULL, NULL);

	// Passthrough read operations are ok.
	v3_hook_msr(core->vm_info, INT_PENDING_AMD_MSR, NULL, v3_msr_unhandled_write, NULL);
    }


}


int v3_init_svm_vmcb(struct guest_info * core, v3_vm_class_t vm_class) {

    PrintDebug(core->vm_info, core, "Allocating VMCB\n");
    core->vmm_data = (void *)Allocate_VMCB();
    
    if (core->vmm_data == NULL) {
	PrintError(core->vm_info, core, "Could not allocate VMCB, Exiting...\n");
	return -1;
    }

    if (vm_class == V3_PC_VM) {
	PrintDebug(core->vm_info, core, "Initializing VMCB (addr=%p)\n", (void *)core->vmm_data);
	Init_VMCB_BIOS((vmcb_t*)(core->vmm_data), core);
    } else {
	PrintError(core->vm_info, core, "Invalid VM class\n");
	return -1;
    }

    core->core_run_state = CORE_STOPPED;

    return 0;
}


int v3_deinit_svm_vmcb(struct guest_info * core) {
    if (core && core->vmm_data) { 
	V3_FreePages(V3_PAddr(core->vmm_data), 1);
    }
    return 0;
}


static int svm_handle_standard_reset(struct guest_info *core)
{
    if (core->core_run_state != CORE_RESETTING) { 
	return 0;
    }

    PrintDebug(core->vm_info,core,"Handling standard reset (guest state before follows)\n");

#ifdef V3_CONFIG_DEBUG_SVM
    v3_print_guest_state(core);
#endif

    // wait until all resetting cores get here (ROS or whole VM)
    v3_counting_barrier(&core->vm_info->reset_barrier);

    // I could be a ROS core, or I could be in a non-HVM 
    // either way, if I'm core 0, I'm the leader
    if (core->vcpu_id==0) {
	uint64_t mem_size=core->vm_info->mem_size;

#ifdef V3_CONFIG_HVM
	// on a ROS reset, we should only 
	// manipulate the part of the memory seen by
	// the ROS
	if (core->vm_info->hvm_state.is_hvm) { 
	    mem_size=v3_get_hvm_ros_memsize(core->vm_info);
	}
#endif
	core->vm_info->run_state = VM_RESETTING;
	// copy bioses again because some, 
	// like seabios, assume
	// this should also blow away the BDA and EBDA
	PrintDebug(core->vm_info,core,"Clear memory (%p bytes)\n",(void*)core->vm_info->mem_size);
	if (v3_set_gpa_memory(core, 0, mem_size, 0)!=mem_size) { 
	    PrintError(core->vm_info,core,"Clear of memory failed\n");
	}
	PrintDebug(core->vm_info,core,"Copying bioses\n");
	if (v3_setup_bioses(core->vm_info, core->vm_info->cfg_data->cfg)) { 
	    PrintError(core->vm_info,core,"Setup of bioses failed\n");
	}
    }

    Init_VMCB_BIOS((vmcb_t*)(core->vmm_data), core);

    PrintDebug(core->vm_info,core,"InitVMCB done\n");

    core->cpl = 0;
    core->cpu_mode = REAL;
    core->mem_mode = PHYSICAL_MEM;
    //core->num_exits=0;

    PrintDebug(core->vm_info,core,"Machine reset to REAL/PHYSICAL\n");

    memset(V3_VAddr((void*)(host_vmcbs[V3_Get_CPU()])),0,4096*4); // good measure...

    // core zero will be restarted by the main execution loop
    core->core_run_state = CORE_STOPPED;

    if (core->vcpu_id==0) { 
	core->vm_info->run_state = VM_RUNNING;
    } 

#ifdef V3_CONFIG_DEBUG_SVM
    PrintDebug(core->vm_info,core,"VMCB state at end of reset\n");
    PrintDebugVMCB((vmcb_t*)(core->vmm_data));
    PrintDebug(core->vm_info,core,"Guest state at end of reset\n");
    v3_print_guest_state(core);
#endif

    // wait until we are all ready to go
    v3_counting_barrier(&core->vm_info->reset_barrier);

    PrintDebug(core->vm_info,core,"Returning with request for recycle loop\n");

    return 1; // reboot is occuring

}

#ifdef V3_CONFIG_CHECKPOINT
int v3_svm_save_core(struct guest_info * core, void * ctx){

  vmcb_saved_state_t * guest_area = GET_VMCB_SAVE_STATE_AREA(core->vmm_data); 

  // Special case saves of data we need immediate access to
  // in some cases
  V3_CHKPT_SAVE(ctx, "CPL", core->cpl, failout);
  V3_CHKPT_SAVE(ctx,"STAR", guest_area->star, failout); 
  V3_CHKPT_SAVE(ctx,"CSTAR", guest_area->cstar, failout); 
  V3_CHKPT_SAVE(ctx,"LSTAR", guest_area->lstar, failout); 
  V3_CHKPT_SAVE(ctx,"SFMASK", guest_area->sfmask, failout); 
  V3_CHKPT_SAVE(ctx,"KERNELGSBASE", guest_area->KernelGsBase, failout); 
  V3_CHKPT_SAVE(ctx,"SYSENTER_CS", guest_area->sysenter_cs, failout); 
  V3_CHKPT_SAVE(ctx,"SYSENTER_ESP", guest_area->sysenter_esp, failout); 
  V3_CHKPT_SAVE(ctx,"SYSENTER_EIP", guest_area->sysenter_eip, failout); 
  
// and then we save the whole enchilada
  if (v3_chkpt_save(ctx, "VMCB_DATA", PAGE_SIZE, core->vmm_data)) { 
    PrintError(core->vm_info, core, "Could not save SVM vmcb\n");
    goto failout;
  }
  
  return 0;

 failout:
  PrintError(core->vm_info, core, "Failed to save SVM state for core\n");
  return -1;

}

int v3_svm_load_core(struct guest_info * core, void * ctx){
    

  vmcb_saved_state_t * guest_area = GET_VMCB_SAVE_STATE_AREA(core->vmm_data); 

  // Reload what we special cased, which we will overwrite in a minute
  V3_CHKPT_LOAD(ctx, "CPL", core->cpl, failout);
  V3_CHKPT_LOAD(ctx,"STAR", guest_area->star, failout); 
  V3_CHKPT_LOAD(ctx,"CSTAR", guest_area->cstar, failout); 
  V3_CHKPT_LOAD(ctx,"LSTAR", guest_area->lstar, failout); 
  V3_CHKPT_LOAD(ctx,"SFMASK", guest_area->sfmask, failout); 
  V3_CHKPT_LOAD(ctx,"KERNELGSBASE", guest_area->KernelGsBase, failout); 
  V3_CHKPT_LOAD(ctx,"SYSENTER_CS", guest_area->sysenter_cs, failout); 
  V3_CHKPT_LOAD(ctx,"SYSENTER_ESP", guest_area->sysenter_esp, failout); 
  V3_CHKPT_LOAD(ctx,"SYSENTER_EIP", guest_area->sysenter_eip, failout); 
  
  // and then we load the whole enchilada
  if (v3_chkpt_load(ctx, "VMCB_DATA", PAGE_SIZE, core->vmm_data)) { 
    PrintError(core->vm_info, core, "Could not load SVM vmcb\n");
    goto failout;
  }
  
  return 0;

 failout:
  PrintError(core->vm_info, core, "Failed to save SVM state for core\n");
  return -1;

}
#endif

static int update_irq_exit_state(struct guest_info * info) {
    vmcb_ctrl_t * guest_ctrl = GET_VMCB_CTRL_AREA((vmcb_t*)(info->vmm_data));

    // Fix for QEMU bug using EVENTINJ as an internal cache
    guest_ctrl->EVENTINJ.valid = 0;

    if ((info->intr_core_state.irq_pending == 1) && (guest_ctrl->guest_ctrl.V_IRQ == 0)) {
	
#ifdef V3_CONFIG_DEBUG_INTERRUPTS
	PrintDebug(info->vm_info, info, "INTAK cycle completed for irq %d\n", info->intr_core_state.irq_vector);
#endif

	info->intr_core_state.irq_started = 1;
	info->intr_core_state.irq_pending = 0;

	v3_injecting_intr(info, info->intr_core_state.irq_vector, V3_EXTERNAL_IRQ);
    }

    if ((info->intr_core_state.irq_started == 1) && (guest_ctrl->exit_int_info.valid == 0)) {
#ifdef V3_CONFIG_DEBUG_INTERRUPTS
	PrintDebug(info->vm_info, info, "Interrupt %d taken by guest\n", info->intr_core_state.irq_vector);
#endif

	// Interrupt was taken fully vectored
	info->intr_core_state.irq_started = 0;

    } else if ((info->intr_core_state.irq_started == 1) && (guest_ctrl->exit_int_info.valid == 1)) {
#ifdef V3_CONFIG_DEBUG_INTERRUPTS
	PrintDebug(info->vm_info, info, "EXIT INT INFO is set (vec=%d)\n", guest_ctrl->exit_int_info.vector);
#endif
    }

    return 0;
}


static int update_irq_entry_state(struct guest_info * info) {
    vmcb_ctrl_t * guest_ctrl = GET_VMCB_CTRL_AREA((vmcb_t*)(info->vmm_data));

    if (guest_ctrl->exit_int_info.valid) {
	// We need to complete the previous injection
	guest_ctrl->EVENTINJ = guest_ctrl->exit_int_info;

	PrintDebug(info->vm_info,info,"Continuing injection of event - eventinj=0x%llx\n",*(uint64_t*)&guest_ctrl->EVENTINJ);

	return 0;
    }


    if (info->intr_core_state.irq_pending == 0) {
	guest_ctrl->guest_ctrl.V_IRQ = 0;
	guest_ctrl->guest_ctrl.V_INTR_VECTOR = 0;
    }
    
    if (v3_excp_pending(info)) {

	uint_t excp = v3_get_excp_number(info);
	
	guest_ctrl->EVENTINJ.type = SVM_INJECTION_EXCEPTION;
	guest_ctrl->EVENTINJ.vector = excp;

	if (info->excp_state.excp_error_code_valid) {
	    guest_ctrl->EVENTINJ.error_code = info->excp_state.excp_error_code;
	    guest_ctrl->EVENTINJ.ev = 1;
#ifdef V3_CONFIG_DEBUG_INTERRUPTS
	    PrintDebug(info->vm_info, info, "Injecting exception %d with error code %x\n", excp, guest_ctrl->EVENTINJ.error_code);
#endif
	} else {
	    guest_ctrl->EVENTINJ.error_code = 0;
	    guest_ctrl->EVENTINJ.ev = 0;
	}

	guest_ctrl->EVENTINJ.rsvd = 0;
	guest_ctrl->EVENTINJ.valid = 1;

#ifdef V3_CONFIG_DEBUG_INTERRUPTS
	PrintDebug(info->vm_info, info, "<%d> Injecting Exception %d (CR2=%p) (EIP=%p)\n", 
		   (int)info->num_exits, 
		   guest_ctrl->EVENTINJ.vector, 
		   (void *)(addr_t)info->ctrl_regs.cr2,
		   (void *)(addr_t)info->rip);
#endif

	v3_injecting_excp(info, excp);

    } else if (info->intr_core_state.irq_started == 1) {

#ifdef V3_CONFIG_DEBUG_INTERRUPTS
	PrintDebug(info->vm_info, info, "IRQ pending from previous injection\n");
#endif
	guest_ctrl->guest_ctrl.V_IRQ = 1;
	guest_ctrl->guest_ctrl.V_INTR_VECTOR = info->intr_core_state.irq_vector;

	// We ignore the virtual TPR on this injection
	// TPR/PPR tests have already been done in the APIC.
	guest_ctrl->guest_ctrl.V_IGN_TPR = 1;
	guest_ctrl->guest_ctrl.V_INTR_PRIO = info->intr_core_state.irq_vector >> 4 ;  // 0xf;

    } else {
	switch (v3_intr_pending(info)) {
	    case V3_EXTERNAL_IRQ: {
 	        int irq = v3_get_intr(info); 

		if (irq<0) {
		  break;
		}

		guest_ctrl->guest_ctrl.V_IRQ = 1;
		guest_ctrl->guest_ctrl.V_INTR_VECTOR = irq;

		// We ignore the virtual TPR on this injection
		// TPR/PPR tests have already been done in the APIC.
		guest_ctrl->guest_ctrl.V_IGN_TPR = 1;
		guest_ctrl->guest_ctrl.V_INTR_PRIO = info->intr_core_state.irq_vector >> 4 ;  // 0xf;

#ifdef V3_CONFIG_DEBUG_INTERRUPTS
		PrintDebug(info->vm_info, info, "Injecting Interrupt %d (EIP=%p)\n", 
			   guest_ctrl->guest_ctrl.V_INTR_VECTOR, 
			   (void *)(addr_t)info->rip);
#endif

		info->intr_core_state.irq_pending = 1;
		info->intr_core_state.irq_vector = irq;

		break;
		
	    }
	    case V3_NMI:
#ifdef V3_CONFIG_DEBUG_INTERRUPTS
		PrintDebug(info->vm_info, info, "Injecting NMI\n");
#endif
		guest_ctrl->EVENTINJ.type = SVM_INJECTION_NMI;
		guest_ctrl->EVENTINJ.ev = 0;
		guest_ctrl->EVENTINJ.error_code = 0;
		guest_ctrl->EVENTINJ.rsvd = 0;
		guest_ctrl->EVENTINJ.valid = 1;

		break;

	    case V3_SOFTWARE_INTR:
#ifdef V3_CONFIG_DEBUG_INTERRUPTS
		PrintDebug(info->vm_info, info, "Injecting software interrupt --  type: %d, vector: %d\n", 
			   SVM_INJECTION_SOFT_INTR, info->intr_core_state.swintr_vector);
#endif
		guest_ctrl->EVENTINJ.type = SVM_INJECTION_SOFT_INTR;
		guest_ctrl->EVENTINJ.vector = info->intr_core_state.swintr_vector;
		guest_ctrl->EVENTINJ.ev = 0;
		guest_ctrl->EVENTINJ.error_code = 0;
		guest_ctrl->EVENTINJ.rsvd = 0;
		guest_ctrl->EVENTINJ.valid = 1;
            
		/* reset swintr state */
		info->intr_core_state.swintr_posted = 0;
		info->intr_core_state.swintr_vector = 0;

		break;
	    case V3_VIRTUAL_IRQ:
		guest_ctrl->EVENTINJ.type = SVM_INJECTION_IRQ;
		break;

	    case V3_INVALID_INTR:
	    default:
		break;
	}
	
    }

    return 0;
}

int 
v3_svm_config_tsc_virtualization(struct guest_info * info) {
    vmcb_ctrl_t * ctrl_area = GET_VMCB_CTRL_AREA((vmcb_t*)(info->vmm_data));


    if (info->time_state.flags & VM_TIME_TRAP_RDTSC) {
	ctrl_area->instrs.RDTSC = 1;
	ctrl_area->svm_instrs.RDTSCP = 1;
    } else {
	ctrl_area->instrs.RDTSC = 0;
	ctrl_area->svm_instrs.RDTSCP = 0;

	if (info->time_state.flags & VM_TIME_TSC_PASSTHROUGH) {
        	ctrl_area->TSC_OFFSET = 0;
	} else {
        	ctrl_area->TSC_OFFSET = v3_tsc_host_offset(&info->time_state);
	}
    }
    return 0;
}



/* 
 * CAUTION and DANGER!!! 
 * 
 * The VMCB CANNOT(!!) be accessed outside of the clgi/stgi calls inside this function
 * When exectuing a symbiotic call, the VMCB WILL be overwritten, so any dependencies 
 * on its contents will cause things to break. The contents at the time of the exit WILL 
 * change before the exit handler is executed.
 */
int v3_svm_enter(struct guest_info * info) {
    vmcb_ctrl_t * guest_ctrl = GET_VMCB_CTRL_AREA((vmcb_t*)(info->vmm_data));
    vmcb_saved_state_t * guest_state = GET_VMCB_SAVE_STATE_AREA((vmcb_t*)(info->vmm_data)); 
    addr_t exit_code = 0, exit_info1 = 0, exit_info2 = 0;
    uint64_t guest_cycles = 0;


    // Conditionally yield the CPU if the timeslice has expired
    v3_schedule(info);

#ifdef V3_CONFIG_MEM_TRACK
    v3_mem_track_entry(info);
#endif 

    // Update timer devices after being in the VM before doing 
    // IRQ updates, so that any interrupts they raise get seen 
    // immediately.

    v3_advance_time(info, NULL);

    v3_update_timers(info);


    // disable global interrupts for vm state transition
    v3_clgi();

    // Synchronize the guest state to the VMCB
    guest_state->cr0 = info->ctrl_regs.cr0;
    guest_state->cr2 = info->ctrl_regs.cr2;
    guest_state->cr3 = info->ctrl_regs.cr3;
    guest_state->cr4 = info->ctrl_regs.cr4;
    guest_state->dr6 = info->dbg_regs.dr6;
    guest_state->dr7 = info->dbg_regs.dr7;

    // CR8 is now updated by read/writes and it contains the APIC TPR
    // the V_TPR should be just the class part of that.
    // This update is here just for completeness.  We currently
    // are ignoring V_TPR on all injections and doing the priority logivc
    // in the APIC.
    // guest_ctrl->guest_ctrl.V_TPR = ((info->ctrl_regs.apic_tpr) >> 4) & 0xf;

    //guest_ctrl->guest_ctrl.V_TPR = info->ctrl_regs.cr8 & 0xff;
    // 
    
    guest_state->rflags = info->ctrl_regs.rflags;

    // LMA ,LME, SVE?

    guest_state->efer = info->ctrl_regs.efer;
    
    /* Synchronize MSRs */
    guest_state->star = info->msrs.star;
    guest_state->lstar = info->msrs.lstar;
    guest_state->sfmask = info->msrs.sfmask;
    guest_state->KernelGsBase = info->msrs.kern_gs_base;

    guest_state->cpl = info->cpl;

    v3_set_vmcb_segments((vmcb_t*)(info->vmm_data), &(info->segments));

    guest_state->rax = info->vm_regs.rax;
    guest_state->rip = info->rip;
    guest_state->rsp = info->vm_regs.rsp;

    V3_FP_ENTRY_RESTORE(info);

#ifdef V3_CONFIG_SYMCALL
    if (info->sym_core_state.symcall_state.sym_call_active == 0) {
	update_irq_entry_state(info);
    }
#else 

    update_irq_entry_state(info);
#endif

#ifdef V3_CONFIG_TM_FUNC
    v3_tm_check_intr_state(info, guest_ctrl, guest_state);
#endif


    /* ** */

    /*
      PrintDebug(info->vm_info, info, "SVM Entry to CS=%p  rip=%p...\n", 
      (void *)(addr_t)info->segments.cs.base, 
      (void *)(addr_t)info->rip);
    */

#ifdef V3_CONFIG_SYMCALL
    if (info->sym_core_state.symcall_state.sym_call_active == 1) {
	if (guest_ctrl->guest_ctrl.V_IRQ == 1) {
	    V3_Print(info->vm_info, info, "!!! Injecting Interrupt during Sym call !!!\n");
	}
    }
#endif

    v3_svm_config_tsc_virtualization(info);

    //V3_Print(info->vm_info, info, "Calling v3_svm_launch\n");
    {	
	uint64_t entry_tsc = 0;
	uint64_t exit_tsc = 0;
	
#ifdef V3_CONFIG_PWRSTAT_TELEMETRY
	v3_pwrstat_telemetry_enter(info);
#endif

#ifdef V3_CONFIG_PMU_TELEMETRY
	v3_pmu_telemetry_enter(info);
#endif


	if (guest_ctrl->EVENTINJ.valid && guest_ctrl->interrupt_shadow) { 
#ifdef V3_CONFIG_DEBUG_INTERRUPTS
	    PrintDebug(info->vm_info,info,"Event injection during an interrupt shadow\n");
#endif
	}

	rdtscll(entry_tsc);

	v3_svm_launch((vmcb_t *)V3_PAddr(info->vmm_data), &(info->vm_regs), (vmcb_t *)host_vmcbs[V3_Get_CPU()]);

	rdtscll(exit_tsc);

#ifdef V3_CONFIG_PMU_TELEMETRY
	v3_pmu_telemetry_exit(info);
#endif

#ifdef V3_CONFIG_PWRSTAT_TELEMETRY
	v3_pwrstat_telemetry_exit(info);
#endif

	guest_cycles = exit_tsc - entry_tsc;
    }


    //V3_Print(info->vm_info, info, "SVM Returned: Exit Code: %x, guest_rip=%lx\n", (uint32_t)(guest_ctrl->exit_code), (unsigned long)guest_state->rip);

    v3_last_exit = (uint32_t)(guest_ctrl->exit_code);

    v3_advance_time(info, &guest_cycles);

    info->num_exits++;

    V3_FP_EXIT_SAVE(info);

    // Save Guest state from VMCB
    info->rip = guest_state->rip;
    info->vm_regs.rsp = guest_state->rsp;
    info->vm_regs.rax = guest_state->rax;

    info->cpl = guest_state->cpl;

    info->ctrl_regs.cr0 = guest_state->cr0;
    info->ctrl_regs.cr2 = guest_state->cr2;
    info->ctrl_regs.cr3 = guest_state->cr3;
    info->ctrl_regs.cr4 = guest_state->cr4;
    info->dbg_regs.dr6 = guest_state->dr6;
    info->dbg_regs.dr7 = guest_state->dr7;
    //
    // We do not track this anymore
    // V_TPR is ignored and we do the logic in the APIC
    //info->ctrl_regs.cr8 = guest_ctrl->guest_ctrl.V_TPR;
    //
    info->ctrl_regs.rflags = guest_state->rflags;
    info->ctrl_regs.efer = guest_state->efer;
    
    /* Synchronize MSRs */
    info->msrs.star =  guest_state->star;
    info->msrs.lstar = guest_state->lstar;
    info->msrs.sfmask = guest_state->sfmask;
    info->msrs.kern_gs_base = guest_state->KernelGsBase;

    v3_get_vmcb_segments((vmcb_t*)(info->vmm_data), &(info->segments));
    info->cpu_mode = v3_get_vm_cpu_mode(info);
    info->mem_mode = v3_get_vm_mem_mode(info);
    /* ** */

    // save exit info here
    exit_code = guest_ctrl->exit_code;
    exit_info1 = guest_ctrl->exit_info1;
    exit_info2 = guest_ctrl->exit_info2;

#ifdef V3_CONFIG_SYMCALL
    if (info->sym_core_state.symcall_state.sym_call_active == 0) {
	update_irq_exit_state(info);
    }
#else
    update_irq_exit_state(info);
#endif

    // reenable global interrupts after vm exit
    v3_stgi();

    // Conditionally yield the CPU if the timeslice has expired
    v3_schedule(info);

    // This update timers is for time-dependent handlers
    // if we're slaved to host time
    v3_advance_time(info, NULL);
    v3_update_timers(info);


    {
	int ret = v3_handle_svm_exit(info, exit_code, exit_info1, exit_info2);
	
	if (ret != 0) {
	    PrintError(info->vm_info, info, "Error in SVM exit handler (ret=%d)\n", ret);
	    PrintError(info->vm_info, info, "  last Exit was %d (exit code=0x%llx)\n", v3_last_exit, (uint64_t) exit_code);

	    return -1;
	}
    }


    if (info->timeouts.timeout_active) {
	/* Check to see if any timeouts have expired */
	v3_handle_timeouts(info, guest_cycles);
    }

#ifdef V3_CONFIG_MEM_TRACK
    v3_mem_track_exit(info);
#endif 


    return 0;
}

int v3_start_svm_guest(struct guest_info * info) {

    int started=0;

    //    vmcb_saved_state_t * guest_state = GET_VMCB_SAVE_STATE_AREA((vmcb_t*)(info->vmm_data));
    //  vmcb_ctrl_t * guest_ctrl = GET_VMCB_CTRL_AREA((vmcb_t*)(info->vmm_data));

    PrintDebug(info->vm_info, info, "Starting SVM core %u (on logical core %u)\n", info->vcpu_id, info->pcpu_id);


#ifdef V3_CONFIG_MULTIBOOT
    if (v3_setup_multiboot_core_for_boot(info)) { 
	PrintError(info->vm_info, info, "Failed to setup Multiboot core...\n");
	return -1;
    }
#endif

#ifdef V3_CONFIG_HVM
    if (v3_setup_hvm_hrt_core_for_boot(info)) { 
	PrintError(info->vm_info, info, "Failed to setup HRT core...\n");
	return -1;
    } 
#endif
 
    while (1) {

	if (info->core_run_state == CORE_STOPPED) {

	    if (info->vcpu_id == 0) {
		info->core_run_state = CORE_RUNNING;
	    } else  { 
		PrintDebug(info->vm_info, info, "SVM core %u (on %u): Waiting for core initialization\n", info->vcpu_id, info->pcpu_id);

		V3_NO_WORK(info);

		// Compiler must not optimize away this read
		while (*((volatile int *)(&info->core_run_state)) == CORE_STOPPED) {
		    
		    if (info->vm_info->run_state == VM_STOPPED) {
			// The VM was stopped before this core was initialized. 
			return 0;
		    }
		    
		    V3_STILL_NO_WORK(info);

		    //PrintDebug(info->vm_info, info, "SVM core %u: still waiting for INIT\n", info->vcpu_id);
		}

		V3_HAVE_WORK_AGAIN(info);
		
		PrintDebug(info->vm_info, info, "SVM core %u(on %u) initialized\n", info->vcpu_id, info->pcpu_id);
		
		// We'll be paranoid about race conditions here
		v3_wait_at_barrier(info);
	    } 
	}

	if (!started) {

	    started=1;
	    
	    PrintDebug(info->vm_info, info, "SVM core %u(on %u): I am starting at CS=0x%x (base=0x%p, limit=0x%x),  RIP=0x%p\n", 
		       info->vcpu_id, info->pcpu_id, 
		       info->segments.cs.selector, (void *)(info->segments.cs.base), 
		       info->segments.cs.limit, (void *)(info->rip));
	    
	    
	    
	    PrintDebug(info->vm_info, info, "SVM core %u: Launching SVM VM (vmcb=%p) (on cpu %u)\n", 
		       info->vcpu_id, (void *)info->vmm_data, info->pcpu_id);

#ifdef V3_CONFIG_DEBUG_SVM
	    PrintDebugVMCB((vmcb_t*)(info->vmm_data));
#endif
	    
	    v3_start_time(info);
	}
	
	if (info->vm_info->run_state == VM_STOPPED) {
	    info->core_run_state = CORE_STOPPED;
	    break;
	}
	
	
#ifdef V3_CONFIG_HVM
	if (v3_handle_hvm_reset(info) > 0) { 
	    continue;
	}
#endif
       
#ifdef V3_CONFIG_MULTIBOOT
	if (v3_handle_multiboot_reset(info) > 0) {
	    continue;
	}
#endif
	
	if (svm_handle_standard_reset(info) > 0 ) {
	    continue;
	}
	


#ifdef V3_CONFIG_PMU_TELEMETRY
	v3_pmu_telemetry_start(info);
#endif
	
#ifdef V3_CONFIG_PWRSTAT_TELEMETRY
	v3_pwrstat_telemetry_start(info);
#endif
	
	if (v3_svm_enter(info) == -1 ) {
	    vmcb_ctrl_t * guest_ctrl = GET_VMCB_CTRL_AREA((vmcb_t*)(info->vmm_data));
	    addr_t host_addr;
	    addr_t linear_addr = 0;
	    
	    info->vm_info->run_state = VM_ERROR;
	    
	    V3_Print(info->vm_info, info, "SVM core %u: SVM ERROR!!\n", info->vcpu_id); 
	    
	    v3_print_guest_state(info);
	    
	    V3_Print(info->vm_info, info, "SVM core %u: SVM Exit Code: %p\n", info->vcpu_id, (void *)(addr_t)guest_ctrl->exit_code); 
	    
	    V3_Print(info->vm_info, info, "SVM core %u: exit_info1 low = 0x%.8x\n", info->vcpu_id, *(uint_t*)&(guest_ctrl->exit_info1));
	    V3_Print(info->vm_info, info, "SVM core %u: exit_info1 high = 0x%.8x\n", info->vcpu_id, *(uint_t *)(((uchar_t *)&(guest_ctrl->exit_info1)) + 4));
	    
	    V3_Print(info->vm_info, info, "SVM core %u: exit_info2 low = 0x%.8x\n", info->vcpu_id, *(uint_t*)&(guest_ctrl->exit_info2));
	    V3_Print(info->vm_info, info, "SVM core %u: exit_info2 high = 0x%.8x\n", info->vcpu_id, *(uint_t *)(((uchar_t *)&(guest_ctrl->exit_info2)) + 4));
	    
	    linear_addr = get_addr_linear(info, info->rip, &(info->segments.cs));
	    
	    if (info->mem_mode == PHYSICAL_MEM) {
		v3_gpa_to_hva(info, linear_addr, &host_addr);
	    } else if (info->mem_mode == VIRTUAL_MEM) {
		v3_gva_to_hva(info, linear_addr, &host_addr);
	    }
	    
	    V3_Print(info->vm_info, info, "SVM core %u: Host Address of rip = 0x%p\n", info->vcpu_id, (void *)host_addr);
	    
	    V3_Print(info->vm_info, info, "SVM core %u: Instr (15 bytes) at %p:\n", info->vcpu_id, (void *)host_addr);
	    v3_dump_mem((uint8_t *)host_addr, 15);
	    
	    v3_print_stack(info);
	    
	    break;
	}
	
	v3_wait_at_barrier(info);
	

	if (info->vm_info->run_state == VM_STOPPED) {
	    PrintDebug(info->vm_info,info,"Stopping core as VM is stopped\n");
	    info->core_run_state = CORE_STOPPED;
	    break;
	}

	

/*
	if ((info->num_exits % 50000) == 0) {
	    V3_Print(info->vm_info, info, "SVM Exit number %d\n", (uint32_t)info->num_exits);
	    v3_print_guest_state(info);
	}
*/
	
    }

#ifdef V3_CONFIG_PMU_TELEMETRY
    v3_pmu_telemetry_end(info);
#endif

#ifdef V3_CONFIG_PWRSTAT_TELEMETRY
    v3_pwrstat_telemetry_end(info);
#endif
    // Need to take down the other cores on error... 

    return 0;
}




int v3_reset_svm_vm_core(struct guest_info * core, addr_t rip) {
    // init vmcb_bios

    // Write the RIP, CS, and descriptor
    // assume the rest is already good to go
    //
    // vector VV -> rip at 0
    //              CS = VV00
    //  This means we start executing at linear address VV000
    //
    // So the selector needs to be VV00
    // and the base needs to be VV000
    //
    core->rip = 0;
    core->segments.cs.selector = rip << 8;
    core->segments.cs.limit = 0xffff;
    core->segments.cs.base = rip << 12;

    return 0;
}






/* Checks machine SVM capability */
/* Implemented from: AMD Arch Manual 3, sect 15.4 */ 
int v3_is_svm_capable() {
    uint_t vm_cr_low = 0, vm_cr_high = 0;
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;

    v3_cpuid(CPUID_EXT_FEATURE_IDS, &eax, &ebx, &ecx, &edx);
  
    PrintDebug(VM_NONE, VCORE_NONE,  "CPUID_EXT_FEATURE_IDS_ecx=0x%x\n", ecx);

    if ((ecx & CPUID_EXT_FEATURE_IDS_ecx_svm_avail) == 0) {
      V3_Print(VM_NONE, VCORE_NONE,  "SVM Not Available\n");
      return 0;
    }  else {
	v3_get_msr(SVM_VM_CR_MSR, &vm_cr_high, &vm_cr_low);
	
	PrintDebug(VM_NONE, VCORE_NONE, "SVM_VM_CR_MSR = 0x%x 0x%x\n", vm_cr_high, vm_cr_low);
	
	if ((vm_cr_low & SVM_VM_CR_MSR_svmdis) == 1) {
	    V3_Print(VM_NONE, VCORE_NONE, "SVM is available but is disabled.\n");
	    
	    v3_cpuid(CPUID_SVM_REV_AND_FEATURE_IDS, &eax, &ebx, &ecx, &edx);
	    
	    PrintDebug(VM_NONE, VCORE_NONE,  "CPUID_SVM_REV_AND_FEATURE_IDS_edx=0x%x\n", edx);
	    
	    if ((edx & CPUID_SVM_REV_AND_FEATURE_IDS_edx_svml) == 0) {
		V3_Print(VM_NONE, VCORE_NONE,  "SVM BIOS Disabled, not unlockable\n");
	    } else {
		V3_Print(VM_NONE, VCORE_NONE,  "SVM is locked with a key\n");
	    }
	    return 0;

	} else {
	    V3_Print(VM_NONE, VCORE_NONE,  "SVM is available and  enabled.\n");

	    v3_cpuid(CPUID_SVM_REV_AND_FEATURE_IDS, &eax, &ebx, &ecx, &edx);
	    PrintDebug(VM_NONE, VCORE_NONE, "CPUID_SVM_REV_AND_FEATURE_IDS_eax=0x%x\n", eax);
	    PrintDebug(VM_NONE, VCORE_NONE, "CPUID_SVM_REV_AND_FEATURE_IDS_ebx=0x%x\n", ebx);
	    PrintDebug(VM_NONE, VCORE_NONE, "CPUID_SVM_REV_AND_FEATURE_IDS_ecx=0x%x\n", ecx);
	    PrintDebug(VM_NONE, VCORE_NONE, "CPUID_SVM_REV_AND_FEATURE_IDS_edx=0x%x\n", edx);

	    if (!(edx & 0x8)) { 
	      PrintError(VM_NONE,VCORE_NONE, "WARNING: NO SVM SUPPORT FOR NRIP - SW INTR INJECTION WILL LIKELY FAIL\n");
	    }

	    return 1;
	}
    }
}

static int has_svm_nested_paging() {
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    
    v3_cpuid(CPUID_SVM_REV_AND_FEATURE_IDS, &eax, &ebx, &ecx, &edx);
    
    //PrintDebug(VM_NONE, VCORE_NONE,  "CPUID_EXT_FEATURE_IDS_edx=0x%x\n", edx);
    
    if ((edx & CPUID_SVM_REV_AND_FEATURE_IDS_edx_np) == 0) {
	V3_Print(VM_NONE, VCORE_NONE, "SVM Nested Paging not supported\n");
	return 0;
    } else {
	V3_Print(VM_NONE, VCORE_NONE, "SVM Nested Paging supported\n");
	return 1;
    }
 }
 


void v3_init_svm_cpu(int cpu_id) {
    reg_ex_t msr;
    extern v3_cpu_arch_t v3_cpu_types[];

    // Enable SVM on the CPU
    v3_get_msr(EFER_MSR, &(msr.e_reg.high), &(msr.e_reg.low));
    msr.e_reg.low |= EFER_MSR_svm_enable;
    v3_set_msr(EFER_MSR, 0, msr.e_reg.low);

    V3_Print(VM_NONE, VCORE_NONE,  "SVM Enabled\n");

    // Setup the host state save area
    host_vmcbs[cpu_id] = (addr_t)V3_AllocPages(4); // need not be shadow-safe, not exposed to guest

    if (!host_vmcbs[cpu_id]) {
	PrintError(VM_NONE, VCORE_NONE,  "Failed to allocate VMCB\n");
	return;
    }

    /* 64-BIT-ISSUE */
    //  msr.e_reg.high = 0;
    //msr.e_reg.low = (uint_t)host_vmcb;
    msr.r_reg = host_vmcbs[cpu_id];

    PrintDebug(VM_NONE, VCORE_NONE,  "Host State being saved at %p\n", (void *)host_vmcbs[cpu_id]);
    v3_set_msr(SVM_VM_HSAVE_PA_MSR, msr.e_reg.high, msr.e_reg.low);


    if (has_svm_nested_paging() == 1) {
	v3_cpu_types[cpu_id] = V3_SVM_REV3_CPU;
    } else {
	v3_cpu_types[cpu_id] = V3_SVM_CPU;
    }
}



void v3_deinit_svm_cpu(int cpu_id) {
    reg_ex_t msr;
    extern v3_cpu_arch_t v3_cpu_types[];

    // reset SVM_VM_HSAVE_PA_MSR
    // Does setting it to NULL disable??
    msr.r_reg = 0;
    v3_set_msr(SVM_VM_HSAVE_PA_MSR, msr.e_reg.high, msr.e_reg.low);

    // Disable SVM?
    v3_get_msr(EFER_MSR, &(msr.e_reg.high), &(msr.e_reg.low));
    msr.e_reg.low &= ~EFER_MSR_svm_enable;
    v3_set_msr(EFER_MSR, 0, msr.e_reg.low);

    v3_cpu_types[cpu_id] = V3_INVALID_CPU;

    V3_FreePages((void *)host_vmcbs[cpu_id], 4);

    V3_Print(VM_NONE, VCORE_NONE,  "Host CPU %d host area freed, and SVM disabled\n", cpu_id);
    return;
}


















































#if 0
/* 
 * Test VMSAVE/VMLOAD Latency 
 */
#define vmsave ".byte 0x0F,0x01,0xDB ; "
#define vmload ".byte 0x0F,0x01,0xDA ; "
{
    uint32_t start_lo, start_hi;
    uint32_t end_lo, end_hi;
    uint64_t start, end;
    
    __asm__ __volatile__ (
			  "rdtsc ; "
			  "movl %%eax, %%esi ; "
			  "movl %%edx, %%edi ; "
			  "movq  %%rcx, %%rax ; "
			  vmsave
			  "rdtsc ; "
			  : "=D"(start_hi), "=S"(start_lo), "=a"(end_lo),"=d"(end_hi)
			  : "c"(host_vmcb[cpu_id]), "0"(0), "1"(0), "2"(0), "3"(0)
			  );
    
    start = start_hi;
    start <<= 32;
    start += start_lo;
    
    end = end_hi;
    end <<= 32;
    end += end_lo;
    
    PrintDebug(core->vm_info, core, "VMSave Cycle Latency: %d\n", (uint32_t)(end - start));
    
    __asm__ __volatile__ (
			  "rdtsc ; "
			  "movl %%eax, %%esi ; "
			  "movl %%edx, %%edi ; "
			  "movq  %%rcx, %%rax ; "
			  vmload
			  "rdtsc ; "
			  : "=D"(start_hi), "=S"(start_lo), "=a"(end_lo),"=d"(end_hi)
			      : "c"(host_vmcb[cpu_id]), "0"(0), "1"(0), "2"(0), "3"(0)
			      );
	
	start = start_hi;
	start <<= 32;
	start += start_lo;

	end = end_hi;
	end <<= 32;
	end += end_lo;


	PrintDebug(core->vm_info, core, "VMLoad Cycle Latency: %d\n", (uint32_t)(end - start));
    }
    /* End Latency Test */

#endif







