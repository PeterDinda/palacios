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

#ifdef V3_CONFIG_CHECKPOINT
#include <palacios/vmm_checkpoint.h>
#endif

#include <palacios/vmm_direct_paging.h>

#include <palacios/vmm_ctrl_regs.h>
#include <palacios/svm_io.h>

#include <palacios/vmm_sprintf.h>


#ifndef V3_CONFIG_DEBUG_SVM
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


uint32_t v3_last_exit;

// This is a global pointer to the host's VMCB
static addr_t host_vmcbs[V3_CONFIG_MAX_CPUS] = { [0 ... V3_CONFIG_MAX_CPUS - 1] = 0};



extern void v3_stgi();
extern void v3_clgi();
//extern int v3_svm_launch(vmcb_t * vmcb, struct v3_gprs * vm_regs, uint64_t * fs, uint64_t * gs);
extern int v3_svm_launch(vmcb_t * vmcb, struct v3_gprs * vm_regs, vmcb_t * host_vmcb);


static vmcb_t * Allocate_VMCB() {
    vmcb_t * vmcb_page = NULL;
    addr_t vmcb_pa = (addr_t)V3_AllocPages(1);

    if ((void *)vmcb_pa == NULL) {
	PrintError("Error allocating VMCB\n");
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


static void Init_VMCB_BIOS(vmcb_t * vmcb, struct guest_info * core) {
    vmcb_ctrl_t * ctrl_area = GET_VMCB_CTRL_AREA(vmcb);
    vmcb_saved_state_t * guest_state = GET_VMCB_SAVE_STATE_AREA(vmcb);
    uint_t i;


    //
    ctrl_area->svm_instrs.VMRUN = 1;
    ctrl_area->svm_instrs.VMMCALL = 1;
    ctrl_area->svm_instrs.VMLOAD = 1;
    ctrl_area->svm_instrs.VMSAVE = 1;
    ctrl_area->svm_instrs.STGI = 1;
    ctrl_area->svm_instrs.CLGI = 1;
    ctrl_area->svm_instrs.SKINIT = 1;
    ctrl_area->svm_instrs.ICEBP = 1;
    ctrl_area->svm_instrs.WBINVD = 1;
    ctrl_area->svm_instrs.MONITOR = 1;
    ctrl_area->svm_instrs.MWAIT_always = 1;
    ctrl_area->svm_instrs.MWAIT_if_armed = 1;
    ctrl_area->instrs.INVLPGA = 1;
    ctrl_area->instrs.CPUID = 1;

    ctrl_area->instrs.HLT = 1;

#ifdef V3_CONFIG_TIME_VIRTUALIZE_TSC
    ctrl_area->instrs.RDTSC = 1;
    ctrl_area->svm_instrs.RDTSCP = 1;
#endif

    // guest_state->cr0 = 0x00000001;    // PE 
  
    /*
      ctrl_area->exceptions.de = 1;
      ctrl_area->exceptions.df = 1;
      
      ctrl_area->exceptions.ts = 1;
      ctrl_area->exceptions.ss = 1;
      ctrl_area->exceptions.ac = 1;
      ctrl_area->exceptions.mc = 1;
      ctrl_area->exceptions.gp = 1;
      ctrl_area->exceptions.ud = 1;
      ctrl_area->exceptions.np = 1;
      ctrl_area->exceptions.of = 1;
      
      ctrl_area->exceptions.nmi = 1;
    */
    

    ctrl_area->instrs.NMI = 1;
    ctrl_area->instrs.SMI = 0; // allow SMIs to run in guest
    ctrl_area->instrs.INIT = 1;
    ctrl_area->instrs.PAUSE = 1;
    ctrl_area->instrs.shutdown_evts = 1;


    /* DEBUG FOR RETURN CODE */
    ctrl_area->exit_code = 1;


    /* Setup Guest Machine state */

    core->vm_regs.rsp = 0x00;
    core->rip = 0xfff0;

    core->vm_regs.rdx = 0x00000f00;


    core->cpl = 0;

    core->ctrl_regs.rflags = 0x00000002; // The reserved bit is always 1
    core->ctrl_regs.cr0 = 0x60010010; // Set the WP flag so the memory hooks work in real-mode
    core->ctrl_regs.efer |= EFER_MSR_svm_enable;





    core->segments.cs.selector = 0xf000;
    core->segments.cs.limit = 0xffff;
    core->segments.cs.base = 0x0000000f0000LL;

    // (raw attributes = 0xf3)
    core->segments.cs.type = 0x3;
    core->segments.cs.system = 0x1;
    core->segments.cs.dpl = 0x3;
    core->segments.cs.present = 1;



    struct v3_segment * segregs [] = {&(core->segments.ss), &(core->segments.ds), 
				      &(core->segments.es), &(core->segments.fs), 
				      &(core->segments.gs), NULL};

    for ( i = 0; segregs[i] != NULL; i++) {
	struct v3_segment * seg = segregs[i];
	
	seg->selector = 0x0000;
	//    seg->base = seg->selector << 4;
	seg->base = 0x00000000;
	seg->limit = ~0u;

	// (raw attributes = 0xf3)
	seg->type = 0x3;
	seg->system = 0x1;
	seg->dpl = 0x3;
	seg->present = 1;
    }

    core->segments.gdtr.limit = 0x0000ffff;
    core->segments.gdtr.base = 0x0000000000000000LL;
    core->segments.idtr.limit = 0x0000ffff;
    core->segments.idtr.base = 0x0000000000000000LL;

    core->segments.ldtr.selector = 0x0000;
    core->segments.ldtr.limit = 0x0000ffff;
    core->segments.ldtr.base = 0x0000000000000000LL;
    core->segments.tr.selector = 0x0000;
    core->segments.tr.limit = 0x0000ffff;
    core->segments.tr.base = 0x0000000000000000LL;


    core->dbg_regs.dr6 = 0x00000000ffff0ff0LL;
    core->dbg_regs.dr7 = 0x0000000000000400LL;


    ctrl_area->IOPM_BASE_PA = (addr_t)V3_PAddr(core->vm_info->io_map.arch_data);
    ctrl_area->instrs.IOIO_PROT = 1;
	    
    ctrl_area->MSRPM_BASE_PA = (addr_t)V3_PAddr(core->vm_info->msr_map.arch_data);
    ctrl_area->instrs.MSR_PROT = 1;   


    PrintDebug("Exiting on interrupts\n");
    ctrl_area->guest_ctrl.V_INTR_MASKING = 1;
    ctrl_area->instrs.INTR = 1;


    v3_hook_msr(core->vm_info, EFER_MSR, 
		&v3_handle_efer_read,
		&v3_svm_handle_efer_write, 
		core);

    if (core->shdw_pg_mode == SHADOW_PAGING) {
	PrintDebug("Creating initial shadow page table\n");
	
	/* JRL: This is a performance killer, and a simplistic solution */
	/* We need to fix this */
	ctrl_area->TLB_CONTROL = 1;
	ctrl_area->guest_ASID = 1;
	
	
	if (v3_init_passthrough_pts(core) == -1) {
	    PrintError("Could not initialize passthrough page tables\n");
	    return ;
	}


	core->shdw_pg_state.guest_cr0 = 0x0000000000000010LL;
	PrintDebug("Created\n");
	
	core->ctrl_regs.cr0 |= 0x80000000;
	core->ctrl_regs.cr3 = core->direct_map_pt;

	ctrl_area->cr_reads.cr0 = 1;
	ctrl_area->cr_writes.cr0 = 1;
	//ctrl_area->cr_reads.cr4 = 1;
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

	PrintDebug("NP_Enable at 0x%p\n", (void *)&(ctrl_area->NP_ENABLE));

	// Set the Nested Page Table pointer
	if (v3_init_passthrough_pts(core) == -1) {
	    PrintError("Could not initialize Nested page tables\n");
	    return ;
	}

	ctrl_area->N_CR3 = core->direct_map_pt;

	guest_state->g_pat = 0x7040600070406ULL;
    }
    
    /* tell the guest that we don't support SVM */
    v3_hook_msr(core->vm_info, SVM_VM_CR_MSR, 
	&v3_handle_vm_cr_read,
	&v3_handle_vm_cr_write, 
	core);


    {
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

    PrintDebug("Allocating VMCB\n");
    core->vmm_data = (void *)Allocate_VMCB();
    
    if (core->vmm_data == NULL) {
	PrintError("Could not allocate VMCB, Exiting...\n");
	return -1;
    }

    if (vm_class == V3_PC_VM) {
	PrintDebug("Initializing VMCB (addr=%p)\n", (void *)core->vmm_data);
	Init_VMCB_BIOS((vmcb_t*)(core->vmm_data), core);
    } else {
	PrintError("Invalid VM class\n");
	return -1;
    }

    return 0;
}


int v3_deinit_svm_vmcb(struct guest_info * core) {
    V3_FreePages(V3_PAddr(core->vmm_data), 1);
    return 0;
}


#ifdef V3_CONFIG_CHECKPOINT
int v3_svm_save_core(struct guest_info * core, void * ctx){

    v3_chkpt_save_8(ctx, "cpl", &(core->cpl));
    v3_chkpt_save(ctx, "vmcb_data", PAGE_SIZE, core->vmm_data);

    return 0;
}

int v3_svm_load_core(struct guest_info * core, void * ctx){
    
    v3_chkpt_load_8(ctx, "cpl", &(core->cpl));

    if (v3_chkpt_load(ctx, "vmcb_data", PAGE_SIZE, core->vmm_data) == -1) {
	return -1;
    }

    return 0;
}
#endif

static int update_irq_exit_state(struct guest_info * info) {
    vmcb_ctrl_t * guest_ctrl = GET_VMCB_CTRL_AREA((vmcb_t*)(info->vmm_data));

    // Fix for QEMU bug using EVENTINJ as an internal cache
    guest_ctrl->EVENTINJ.valid = 0;

    if ((info->intr_core_state.irq_pending == 1) && (guest_ctrl->guest_ctrl.V_IRQ == 0)) {
	
#ifdef V3_CONFIG_DEBUG_INTERRUPTS
	PrintDebug("INTAK cycle completed for irq %d\n", info->intr_core_state.irq_vector);
#endif

	info->intr_core_state.irq_started = 1;
	info->intr_core_state.irq_pending = 0;

	v3_injecting_intr(info, info->intr_core_state.irq_vector, V3_EXTERNAL_IRQ);
    }

    if ((info->intr_core_state.irq_started == 1) && (guest_ctrl->exit_int_info.valid == 0)) {
#ifdef V3_CONFIG_DEBUG_INTERRUPTS
	PrintDebug("Interrupt %d taken by guest\n", info->intr_core_state.irq_vector);
#endif

	// Interrupt was taken fully vectored
	info->intr_core_state.irq_started = 0;

    } else if ((info->intr_core_state.irq_started == 1) && (guest_ctrl->exit_int_info.valid == 1)) {
#ifdef V3_CONFIG_DEBUG_INTERRUPTS
	PrintDebug("EXIT INT INFO is set (vec=%d)\n", guest_ctrl->exit_int_info.vector);
#endif
    }

    return 0;
}


static int update_irq_entry_state(struct guest_info * info) {
    vmcb_ctrl_t * guest_ctrl = GET_VMCB_CTRL_AREA((vmcb_t*)(info->vmm_data));


    if (info->intr_core_state.irq_pending == 0) {
	guest_ctrl->guest_ctrl.V_IRQ = 0;
	guest_ctrl->guest_ctrl.V_INTR_VECTOR = 0;
    }
    
    if (v3_excp_pending(info)) {
	uint_t excp = v3_get_excp_number(info);
	
	guest_ctrl->EVENTINJ.type = SVM_INJECTION_EXCEPTION;
	
	if (info->excp_state.excp_error_code_valid) {
	    guest_ctrl->EVENTINJ.error_code = info->excp_state.excp_error_code;
	    guest_ctrl->EVENTINJ.ev = 1;
#ifdef V3_CONFIG_DEBUG_INTERRUPTS
	    PrintDebug("Injecting exception %d with error code %x\n", excp, guest_ctrl->EVENTINJ.error_code);
#endif
	}
	
	guest_ctrl->EVENTINJ.vector = excp;
	
	guest_ctrl->EVENTINJ.valid = 1;

#ifdef V3_CONFIG_DEBUG_INTERRUPTS
	PrintDebug("<%d> Injecting Exception %d (CR2=%p) (EIP=%p)\n", 
		   (int)info->num_exits, 
		   guest_ctrl->EVENTINJ.vector, 
		   (void *)(addr_t)info->ctrl_regs.cr2,
		   (void *)(addr_t)info->rip);
#endif

	v3_injecting_excp(info, excp);
    } else if (info->intr_core_state.irq_started == 1) {
#ifdef V3_CONFIG_DEBUG_INTERRUPTS
	PrintDebug("IRQ pending from previous injection\n");
#endif
	guest_ctrl->guest_ctrl.V_IRQ = 1;
	guest_ctrl->guest_ctrl.V_INTR_VECTOR = info->intr_core_state.irq_vector;
	guest_ctrl->guest_ctrl.V_IGN_TPR = 1;
	guest_ctrl->guest_ctrl.V_INTR_PRIO = 0xf;

    } else {
	switch (v3_intr_pending(info)) {
	    case V3_EXTERNAL_IRQ: {
		uint32_t irq = v3_get_intr(info);

		guest_ctrl->guest_ctrl.V_IRQ = 1;
		guest_ctrl->guest_ctrl.V_INTR_VECTOR = irq;
		guest_ctrl->guest_ctrl.V_IGN_TPR = 1;
		guest_ctrl->guest_ctrl.V_INTR_PRIO = 0xf;

#ifdef V3_CONFIG_DEBUG_INTERRUPTS
		PrintDebug("Injecting Interrupt %d (EIP=%p)\n", 
			   guest_ctrl->guest_ctrl.V_INTR_VECTOR, 
			   (void *)(addr_t)info->rip);
#endif

		info->intr_core_state.irq_pending = 1;
		info->intr_core_state.irq_vector = irq;
		
		break;
	    }
	    case V3_NMI:
		guest_ctrl->EVENTINJ.type = SVM_INJECTION_NMI;
		break;
	    case V3_SOFTWARE_INTR:
		guest_ctrl->EVENTINJ.type = SVM_INJECTION_SOFT_INTR;

#ifdef V3_CONFIG_DEBUG_INTERRUPTS
		PrintDebug("Injecting software interrupt --  type: %d, vector: %d\n", 
			   SVM_INJECTION_SOFT_INTR, info->intr_core_state.swintr_vector);
#endif
		guest_ctrl->EVENTINJ.vector = info->intr_core_state.swintr_vector;
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
    sint64_t tsc_offset;

    // Conditionally yield the CPU if the timeslice has expired
    v3_yield_cond(info);

    // Perform any additional yielding needed for time adjustment
    v3_adjust_time(info);

    // disable global interrupts for vm state transition
    v3_clgi();

    // Update timer devices after being in the VM, with interupts
    // disabled, but before doing IRQ updates, so that any interrupts they 
    //raise get seen immediately.
    v3_update_timers(info);

    // Synchronize the guest state to the VMCB
    guest_state->cr0 = info->ctrl_regs.cr0;
    guest_state->cr2 = info->ctrl_regs.cr2;
    guest_state->cr3 = info->ctrl_regs.cr3;
    guest_state->cr4 = info->ctrl_regs.cr4;
    guest_state->dr6 = info->dbg_regs.dr6;
    guest_state->dr7 = info->dbg_regs.dr7;
    guest_ctrl->guest_ctrl.V_TPR = info->ctrl_regs.cr8 & 0xff;
    guest_state->rflags = info->ctrl_regs.rflags;
    guest_state->efer = info->ctrl_regs.efer;
    
    guest_state->cpl = info->cpl;

    v3_set_vmcb_segments((vmcb_t*)(info->vmm_data), &(info->segments));

    guest_state->rax = info->vm_regs.rax;
    guest_state->rip = info->rip;
    guest_state->rsp = info->vm_regs.rsp;

#ifdef V3_CONFIG_SYMCALL
    if (info->sym_core_state.symcall_state.sym_call_active == 0) {
	update_irq_entry_state(info);
    }
#else 
    update_irq_entry_state(info);
#endif


    /* ** */

    /*
      PrintDebug("SVM Entry to CS=%p  rip=%p...\n", 
      (void *)(addr_t)info->segments.cs.base, 
      (void *)(addr_t)info->rip);
    */

#ifdef V3_CONFIG_SYMCALL
    if (info->sym_core_state.symcall_state.sym_call_active == 1) {
	if (guest_ctrl->guest_ctrl.V_IRQ == 1) {
	    V3_Print("!!! Injecting Interrupt during Sym call !!!\n");
	}
    }
#endif

    v3_time_enter_vm(info);
    tsc_offset = v3_tsc_host_offset(&info->time_state);
    guest_ctrl->TSC_OFFSET = tsc_offset;


    //V3_Print("Calling v3_svm_launch\n");

    v3_svm_launch((vmcb_t *)V3_PAddr(info->vmm_data), &(info->vm_regs), (vmcb_t *)host_vmcbs[V3_Get_CPU()]);

    //V3_Print("SVM Returned: Exit Code: %x, guest_rip=%lx\n", (uint32_t)(guest_ctrl->exit_code), (unsigned long)guest_state->rip);

    v3_last_exit = (uint32_t)(guest_ctrl->exit_code);

    // Immediate exit from VM time bookkeeping
    v3_time_exit_vm(info);

    info->num_exits++;

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
    info->ctrl_regs.cr8 = guest_ctrl->guest_ctrl.V_TPR;
    info->ctrl_regs.rflags = guest_state->rflags;
    info->ctrl_regs.efer = guest_state->efer;
    
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
    v3_yield_cond(info);

    {
	int ret = v3_handle_svm_exit(info, exit_code, exit_info1, exit_info2);
	
	if (ret != 0) {
	    PrintError("Error in SVM exit handler (ret=%d)\n", ret);
	    PrintError("  last Exit was %d (exit code=0x%llx)\n", v3_last_exit, (uint64_t) exit_code);
	    return -1;
	}
    }


    return 0;
}


int v3_start_svm_guest(struct guest_info * info) {
    //    vmcb_saved_state_t * guest_state = GET_VMCB_SAVE_STATE_AREA((vmcb_t*)(info->vmm_data));
    //  vmcb_ctrl_t * guest_ctrl = GET_VMCB_CTRL_AREA((vmcb_t*)(info->vmm_data));

    PrintDebug("Starting SVM core %u (on logical core %u)\n", info->vcpu_id, info->pcpu_id);

    if (info->vcpu_id == 0) {
	info->core_run_state = CORE_RUNNING;
    } else  { 
	PrintDebug("SVM core %u (on %u): Waiting for core initialization\n", info->vcpu_id, info->pcpu_id);

	while (info->core_run_state == CORE_STOPPED) {
	    
	    if (info->vm_info->run_state == VM_STOPPED) {
		// The VM was stopped before this core was initialized. 
		return 0;
	    }

	    v3_yield(info);
	    //PrintDebug("SVM core %u: still waiting for INIT\n", info->vcpu_id);
	}

	PrintDebug("SVM core %u(on %u) initialized\n", info->vcpu_id, info->pcpu_id);

	// We'll be paranoid about race conditions here
	v3_wait_at_barrier(info);
    } 

    PrintDebug("SVM core %u(on %u): I am starting at CS=0x%x (base=0x%p, limit=0x%x),  RIP=0x%p\n", 
	       info->vcpu_id, info->pcpu_id, 
	       info->segments.cs.selector, (void *)(info->segments.cs.base), 
	       info->segments.cs.limit, (void *)(info->rip));



    PrintDebug("SVM core %u: Launching SVM VM (vmcb=%p) (on cpu %u)\n", 
	       info->vcpu_id, (void *)info->vmm_data, info->pcpu_id);
    //PrintDebugVMCB((vmcb_t*)(info->vmm_data));
    
    v3_start_time(info);

    while (1) {

	if (info->vm_info->run_state == VM_STOPPED) {
	    info->core_run_state = CORE_STOPPED;
	    break;
	}
	
	if (v3_svm_enter(info) == -1) {
	    vmcb_ctrl_t * guest_ctrl = GET_VMCB_CTRL_AREA((vmcb_t*)(info->vmm_data));
	    addr_t host_addr;
	    addr_t linear_addr = 0;
	    
	    info->vm_info->run_state = VM_ERROR;
	    
	    V3_Print("SVM core %u: SVM ERROR!!\n", info->vcpu_id); 
	    
	    v3_print_guest_state(info);
	    
	    V3_Print("SVM core %u: SVM Exit Code: %p\n", info->vcpu_id, (void *)(addr_t)guest_ctrl->exit_code); 
	    
	    V3_Print("SVM core %u: exit_info1 low = 0x%.8x\n", info->vcpu_id, *(uint_t*)&(guest_ctrl->exit_info1));
	    V3_Print("SVM core %u: exit_info1 high = 0x%.8x\n", info->vcpu_id, *(uint_t *)(((uchar_t *)&(guest_ctrl->exit_info1)) + 4));
	    
	    V3_Print("SVM core %u: exit_info2 low = 0x%.8x\n", info->vcpu_id, *(uint_t*)&(guest_ctrl->exit_info2));
	    V3_Print("SVM core %u: exit_info2 high = 0x%.8x\n", info->vcpu_id, *(uint_t *)(((uchar_t *)&(guest_ctrl->exit_info2)) + 4));
	    
	    linear_addr = get_addr_linear(info, info->rip, &(info->segments.cs));
	    
	    if (info->mem_mode == PHYSICAL_MEM) {
		v3_gpa_to_hva(info, linear_addr, &host_addr);
	    } else if (info->mem_mode == VIRTUAL_MEM) {
		v3_gva_to_hva(info, linear_addr, &host_addr);
	    }
	    
	    V3_Print("SVM core %u: Host Address of rip = 0x%p\n", info->vcpu_id, (void *)host_addr);
	    
	    V3_Print("SVM core %u: Instr (15 bytes) at %p:\n", info->vcpu_id, (void *)host_addr);
	    v3_dump_mem((uint8_t *)host_addr, 15);
	    
	    v3_print_stack(info);

	    break;
	}

	v3_wait_at_barrier(info);


	if (info->vm_info->run_state == VM_STOPPED) {
	    info->core_run_state = CORE_STOPPED;
	    break;
	}

	

/*
	if ((info->num_exits % 50000) == 0) {
	    V3_Print("SVM Exit number %d\n", (uint32_t)info->num_exits);
	    v3_print_guest_state(info);
	}
*/
	
    }

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
  
    PrintDebug("CPUID_EXT_FEATURE_IDS_ecx=0x%x\n", ecx);

    if ((ecx & CPUID_EXT_FEATURE_IDS_ecx_svm_avail) == 0) {
      V3_Print("SVM Not Available\n");
      return 0;
    }  else {
	v3_get_msr(SVM_VM_CR_MSR, &vm_cr_high, &vm_cr_low);
	
	PrintDebug("SVM_VM_CR_MSR = 0x%x 0x%x\n", vm_cr_high, vm_cr_low);
	
	if ((vm_cr_low & SVM_VM_CR_MSR_svmdis) == 1) {
	    V3_Print("SVM is available but is disabled.\n");
	    
	    v3_cpuid(CPUID_SVM_REV_AND_FEATURE_IDS, &eax, &ebx, &ecx, &edx);
	    
	    PrintDebug("CPUID_SVM_REV_AND_FEATURE_IDS_edx=0x%x\n", edx);
	    
	    if ((edx & CPUID_SVM_REV_AND_FEATURE_IDS_edx_svml) == 0) {
		V3_Print("SVM BIOS Disabled, not unlockable\n");
	    } else {
		V3_Print("SVM is locked with a key\n");
	    }
	    return 0;

	} else {
	    V3_Print("SVM is available and  enabled.\n");

	    v3_cpuid(CPUID_SVM_REV_AND_FEATURE_IDS, &eax, &ebx, &ecx, &edx);
	    PrintDebug("CPUID_SVM_REV_AND_FEATURE_IDS_eax=0x%x\n", eax);
	    PrintDebug("CPUID_SVM_REV_AND_FEATURE_IDS_ebx=0x%x\n", ebx);
	    PrintDebug("CPUID_SVM_REV_AND_FEATURE_IDS_ecx=0x%x\n", ecx);
	    PrintDebug("CPUID_SVM_REV_AND_FEATURE_IDS_edx=0x%x\n", edx);

	    return 1;
	}
    }
}

static int has_svm_nested_paging() {
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    
    v3_cpuid(CPUID_SVM_REV_AND_FEATURE_IDS, &eax, &ebx, &ecx, &edx);
    
    //PrintDebug("CPUID_EXT_FEATURE_IDS_edx=0x%x\n", edx);
    
    if ((edx & CPUID_SVM_REV_AND_FEATURE_IDS_edx_np) == 0) {
	V3_Print("SVM Nested Paging not supported\n");
	return 0;
    } else {
	V3_Print("SVM Nested Paging supported\n");
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

    V3_Print("SVM Enabled\n");

    // Setup the host state save area
    host_vmcbs[cpu_id] = (addr_t)V3_AllocPages(4);

    /* 64-BIT-ISSUE */
    //  msr.e_reg.high = 0;
    //msr.e_reg.low = (uint_t)host_vmcb;
    msr.r_reg = host_vmcbs[cpu_id];

    PrintDebug("Host State being saved at %p\n", (void *)host_vmcbs[cpu_id]);
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

    V3_Print("Host CPU %d host area freed, and SVM disabled\n", cpu_id);
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
    
    PrintDebug("VMSave Cycle Latency: %d\n", (uint32_t)(end - start));
    
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


	PrintDebug("VMLoad Cycle Latency: %d\n", (uint32_t)(end - start));
    }
    /* End Latency Test */

#endif







#if 0
void Init_VMCB_pe(vmcb_t *vmcb, struct guest_info vm_info) {
  vmcb_ctrl_t * ctrl_area = GET_VMCB_CTRL_AREA(vmcb);
  vmcb_saved_state_t * guest_state = GET_VMCB_SAVE_STATE_AREA(vmcb);
  uint_t i = 0;


  guest_state->rsp = vm_info.vm_regs.rsp;
  guest_state->rip = vm_info.rip;


  /* I pretty much just gutted this from TVMM */
  /* Note: That means its probably wrong */

  // set the segment registers to mirror ours
  guest_state->cs.selector = 1<<3;
  guest_state->cs.attrib.fields.type = 0xa; // Code segment+read
  guest_state->cs.attrib.fields.S = 1;
  guest_state->cs.attrib.fields.P = 1;
  guest_state->cs.attrib.fields.db = 1;
  guest_state->cs.attrib.fields.G = 1;
  guest_state->cs.limit = 0xfffff;
  guest_state->cs.base = 0;
  
  struct vmcb_selector *segregs [] = {&(guest_state->ss), &(guest_state->ds), &(guest_state->es), &(guest_state->fs), &(guest_state->gs), NULL};
  for ( i = 0; segregs[i] != NULL; i++) {
    struct vmcb_selector * seg = segregs[i];
    
    seg->selector = 2<<3;
    seg->attrib.fields.type = 0x2; // Data Segment+read/write
    seg->attrib.fields.S = 1;
    seg->attrib.fields.P = 1;
    seg->attrib.fields.db = 1;
    seg->attrib.fields.G = 1;
    seg->limit = 0xfffff;
    seg->base = 0;
  }


  {
    /* JRL THIS HAS TO GO */
    
    //    guest_state->tr.selector = GetTR_Selector();
    guest_state->tr.attrib.fields.type = 0x9; 
    guest_state->tr.attrib.fields.P = 1;
    // guest_state->tr.limit = GetTR_Limit();
    //guest_state->tr.base = GetTR_Base();// - 0x2000;
    /* ** */
  }


  /* ** */


  guest_state->efer |= EFER_MSR_svm_enable;
  guest_state->rflags = 0x00000002; // The reserved bit is always 1
  ctrl_area->svm_instrs.VMRUN = 1;
  guest_state->cr0 = 0x00000001;    // PE 
  ctrl_area->guest_ASID = 1;


  //  guest_state->cpl = 0;



  // Setup exits

  ctrl_area->cr_writes.cr4 = 1;
  
  ctrl_area->exceptions.de = 1;
  ctrl_area->exceptions.df = 1;
  ctrl_area->exceptions.pf = 1;
  ctrl_area->exceptions.ts = 1;
  ctrl_area->exceptions.ss = 1;
  ctrl_area->exceptions.ac = 1;
  ctrl_area->exceptions.mc = 1;
  ctrl_area->exceptions.gp = 1;
  ctrl_area->exceptions.ud = 1;
  ctrl_area->exceptions.np = 1;
  ctrl_area->exceptions.of = 1;
  ctrl_area->exceptions.nmi = 1;

  

  ctrl_area->instrs.IOIO_PROT = 1;
  ctrl_area->IOPM_BASE_PA = (uint_t)V3_AllocPages(3);
  
  {
    reg_ex_t tmp_reg;
    tmp_reg.r_reg = ctrl_area->IOPM_BASE_PA;
    memset((void*)(tmp_reg.e_reg.low), 0xffffffff, PAGE_SIZE * 2);
  }

  ctrl_area->instrs.INTR = 1;

  
  {
    char gdt_buf[6];
    char idt_buf[6];

    memset(gdt_buf, 0, 6);
    memset(idt_buf, 0, 6);


    uint_t gdt_base, idt_base;
    ushort_t gdt_limit, idt_limit;
    
    GetGDTR(gdt_buf);
    gdt_base = *(ulong_t*)((uchar_t*)gdt_buf + 2) & 0xffffffff;
    gdt_limit = *(ushort_t*)(gdt_buf) & 0xffff;
    PrintDebug("GDT: base: %x, limit: %x\n", gdt_base, gdt_limit);

    GetIDTR(idt_buf);
    idt_base = *(ulong_t*)(idt_buf + 2) & 0xffffffff;
    idt_limit = *(ushort_t*)(idt_buf) & 0xffff;
    PrintDebug("IDT: base: %x, limit: %x\n",idt_base, idt_limit);


    // gdt_base -= 0x2000;
    //idt_base -= 0x2000;

    guest_state->gdtr.base = gdt_base;
    guest_state->gdtr.limit = gdt_limit;
    guest_state->idtr.base = idt_base;
    guest_state->idtr.limit = idt_limit;


  }
  
  
  // also determine if CPU supports nested paging
  /*
  if (vm_info.page_tables) {
    //   if (0) {
    // Flush the TLB on entries/exits
    ctrl_area->TLB_CONTROL = 1;

    // Enable Nested Paging
    ctrl_area->NP_ENABLE = 1;

    PrintDebug("NP_Enable at 0x%x\n", &(ctrl_area->NP_ENABLE));

        // Set the Nested Page Table pointer
    ctrl_area->N_CR3 |= ((addr_t)vm_info.page_tables & 0xfffff000);


    //   ctrl_area->N_CR3 = Get_CR3();
    // guest_state->cr3 |= (Get_CR3() & 0xfffff000);

    guest_state->g_pat = 0x7040600070406ULL;

    PrintDebug("Set Nested CR3: lo: 0x%x  hi: 0x%x\n", (uint_t)*(&(ctrl_area->N_CR3)), (uint_t)*((unsigned char *)&(ctrl_area->N_CR3) + 4));
    PrintDebug("Set Guest CR3: lo: 0x%x  hi: 0x%x\n", (uint_t)*(&(guest_state->cr3)), (uint_t)*((unsigned char *)&(guest_state->cr3) + 4));
    // Enable Paging
    //    guest_state->cr0 |= 0x80000000;
  }
  */

}





#endif


