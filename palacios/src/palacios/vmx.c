/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2011, Jack Lange <jarusl@cs.northwestern.edu>
 * Copyright (c) 2011, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#include <palacios/vmx.h>
#include <palacios/vmm.h>
#include <palacios/vmx_handler.h>
#include <palacios/vmcs.h>
#include <palacios/vmx_lowlevel.h>
#include <palacios/vmm_lowlevel.h>
#include <palacios/vmm_ctrl_regs.h>
#include <palacios/vmm_config.h>
#include <palacios/vmm_time.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_direct_paging.h>
#include <palacios/vmx_io.h>
#include <palacios/vmx_msr.h>
#include <palacios/vmm_decoder.h>
#include <palacios/vmm_barrier.h>
#include <palacios/vmm_timeout.h>
#include <palacios/vmm_debug.h>

#ifdef V3_CONFIG_CHECKPOINT
#include <palacios/vmm_checkpoint.h>
#endif

#include <palacios/vmx_ept.h>
#include <palacios/vmx_assist.h>
#include <palacios/vmx_hw_info.h>

#ifndef V3_CONFIG_DEBUG_VMX
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


/* These fields contain the hardware feature sets supported by the local CPU */
static struct vmx_hw_info hw_info;

extern v3_cpu_arch_t v3_mach_type;

static addr_t host_vmcs_ptrs[V3_CONFIG_MAX_CPUS] = { [0 ... V3_CONFIG_MAX_CPUS - 1] = 0};

extern int v3_vmx_launch(struct v3_gprs * vm_regs, struct guest_info * info, struct v3_ctrl_regs * ctrl_regs);
extern int v3_vmx_resume(struct v3_gprs * vm_regs, struct guest_info * info, struct v3_ctrl_regs * ctrl_regs);

static int inline check_vmcs_write(vmcs_field_t field, addr_t val) {
    int ret = 0;

    ret = vmcs_write(field, val);

    if (ret != VMX_SUCCESS) {
        PrintError("VMWRITE error on %s!: %d\n", v3_vmcs_field_to_str(field), ret);
        return 1;
    }


    

    return 0;
}

static int inline check_vmcs_read(vmcs_field_t field, void * val) {
    int ret = 0;

    ret = vmcs_read(field, val);

    if (ret != VMX_SUCCESS) {
        PrintError("VMREAD error on %s!: %d\n", v3_vmcs_field_to_str(field), ret);
    }

    return ret;
}




static addr_t allocate_vmcs() {
    void *temp;
    struct vmcs_data * vmcs_page = NULL;

    PrintDebug("Allocating page\n");

    temp = V3_AllocPages(1);
    if (!temp) { 
	PrintError("Cannot allocate VMCS\n");
	return -1;
    }
    vmcs_page = (struct vmcs_data *)V3_VAddr(temp);
    memset(vmcs_page, 0, 4096);

    vmcs_page->revision = hw_info.basic_info.revision;
    PrintDebug("VMX Revision: 0x%x\n", vmcs_page->revision);

    return (addr_t)V3_PAddr((void *)vmcs_page);
}


#if 0
static int debug_efer_read(struct guest_info * core, uint_t msr, struct v3_msr * src, void * priv_data) {
    struct v3_msr * efer = (struct v3_msr *)&(core->ctrl_regs.efer);
    V3_Print("\n\nEFER READ (val = %p)\n", (void *)efer->value);
    
    v3_print_guest_state(core);
    v3_print_vmcs();


    src->value = efer->value;
    return 0;
}

static int debug_efer_write(struct guest_info * core, uint_t msr, struct v3_msr src, void * priv_data) {
    struct v3_msr * efer = (struct v3_msr *)&(core->ctrl_regs.efer);
    V3_Print("\n\nEFER WRITE (old_val = %p) (new_val = %p)\n", (void *)efer->value, (void *)src.value);
    
    v3_print_guest_state(core);
    v3_print_vmcs();

    efer->value = src.value;

    return 0;
}
#endif


static int init_vmcs_bios(struct guest_info * core, struct vmx_data * vmx_state) {
    int vmx_ret = 0;

    /* Get Available features */
    struct vmx_pin_ctrls avail_pin_ctrls;
    avail_pin_ctrls.value = v3_vmx_get_ctrl_features(&(hw_info.pin_ctrls));
    /* ** */


    // disable global interrupts for vm state initialization
    v3_disable_ints();

    PrintDebug("Loading VMCS\n");
    vmx_ret = vmcs_load(vmx_state->vmcs_ptr_phys);
    vmx_state->state = VMX_UNLAUNCHED;

    if (vmx_ret != VMX_SUCCESS) {
        PrintError("VMPTRLD failed\n");
        return -1;
    }


    /*** Setup default state from HW ***/

    vmx_state->pin_ctrls.value = hw_info.pin_ctrls.def_val;
    vmx_state->pri_proc_ctrls.value = hw_info.proc_ctrls.def_val;
    vmx_state->exit_ctrls.value = hw_info.exit_ctrls.def_val;
    vmx_state->entry_ctrls.value = hw_info.entry_ctrls.def_val;
    vmx_state->sec_proc_ctrls.value = hw_info.sec_proc_ctrls.def_val;

    /* Print Control MSRs */
    V3_Print("CR0 MSR: req_val=%p, req_mask=%p\n", (void *)(addr_t)hw_info.cr0.req_val, (void *)(addr_t)hw_info.cr0.req_mask);
    V3_Print("CR4 MSR: req_val=%p, req_mask=%p\n", (void *)(addr_t)hw_info.cr4.req_val, (void *)(addr_t)hw_info.cr4.req_mask);



    /******* Setup Host State **********/

    /* Cache GDTR, IDTR, and TR in host struct */


    /********** Setup VMX Control Fields ***********/

    /* Add external interrupts, NMI exiting, and virtual NMI */
    vmx_state->pin_ctrls.nmi_exit = 1;
    vmx_state->pin_ctrls.virt_nmi = 1;
    vmx_state->pin_ctrls.ext_int_exit = 1;



    /* We enable the preemption timer by default to measure accurate guest time */
    if (avail_pin_ctrls.active_preempt_timer) {
	V3_Print("VMX Preemption Timer is available\n");
	vmx_state->pin_ctrls.active_preempt_timer = 1;
	vmx_state->exit_ctrls.save_preempt_timer = 1;
    }

    // we want it to use this when halting
    vmx_state->pri_proc_ctrls.hlt_exit = 1;

    // cpuid tells it that it does not have these instructions
    vmx_state->pri_proc_ctrls.monitor_exit = 1;
    vmx_state->pri_proc_ctrls.mwait_exit = 1;

    // we don't need to handle a pause, although this is where
    // we could pull out of a spin lock acquire or schedule to find its partner
    vmx_state->pri_proc_ctrls.pause_exit = 0;

    vmx_state->pri_proc_ctrls.tsc_offset = 1;
#ifdef V3_CONFIG_TIME_VIRTUALIZE_TSC
    vmx_state->pri_proc_ctrls.rdtsc_exit = 1;
#endif

    /* Setup IO map */
    vmx_state->pri_proc_ctrls.use_io_bitmap = 1;
    vmx_ret |= check_vmcs_write(VMCS_IO_BITMAP_A_ADDR, (addr_t)V3_PAddr(core->vm_info->io_map.arch_data));
    vmx_ret |= check_vmcs_write(VMCS_IO_BITMAP_B_ADDR, 
            (addr_t)V3_PAddr(core->vm_info->io_map.arch_data) + PAGE_SIZE_4KB);


    vmx_state->pri_proc_ctrls.use_msr_bitmap = 1;
    vmx_ret |= check_vmcs_write(VMCS_MSR_BITMAP, (addr_t)V3_PAddr(core->vm_info->msr_map.arch_data));



#ifdef __V3_64BIT__
    // Ensure host runs in 64-bit mode at each VM EXIT
    vmx_state->exit_ctrls.host_64_on = 1;
#endif



    // Restore host's EFER register on each VM EXIT
    vmx_state->exit_ctrls.ld_efer = 1;

    // Save/restore guest's EFER register to/from VMCS on VM EXIT/ENTRY
    vmx_state->exit_ctrls.save_efer = 1;
    vmx_state->entry_ctrls.ld_efer  = 1;

    vmx_state->exit_ctrls.save_pat = 1;
    vmx_state->exit_ctrls.ld_pat = 1;
    vmx_state->entry_ctrls.ld_pat = 1;

    /* Temporary GPF trap */
    //  vmx_state->excp_bmap.gp = 1;

    // Setup Guests initial PAT field
    vmx_ret |= check_vmcs_write(VMCS_GUEST_PAT, 0x0007040600070406LL);

    // Capture CR8 mods so that we can keep the apic_tpr correct
    vmx_state->pri_proc_ctrls.cr8_ld_exit = 1;
    vmx_state->pri_proc_ctrls.cr8_str_exit = 1;


    /* Setup paging */
    if (core->shdw_pg_mode == SHADOW_PAGING) {
        PrintDebug("Creating initial shadow page table\n");

        if (v3_init_passthrough_pts(core) == -1) {
            PrintError("Could not initialize passthrough page tables\n");
            return -1;
        }
        
#define CR0_PE 0x00000001
#define CR0_PG 0x80000000
#define CR0_WP 0x00010000 // To ensure mem hooks work
#define CR0_NE 0x00000020
        vmx_ret |= check_vmcs_write(VMCS_CR0_MASK, (CR0_PE | CR0_PG | CR0_WP | CR0_NE));


	// Cause VM_EXIT whenever CR4.VMXE or CR4.PAE bits are written
	vmx_ret |= check_vmcs_write(VMCS_CR4_MASK, CR4_VMXE | CR4_PAE);

        core->ctrl_regs.cr3 = core->direct_map_pt;

        // vmx_state->pinbased_ctrls |= NMI_EXIT;

        /* Add CR exits */
        vmx_state->pri_proc_ctrls.cr3_ld_exit = 1;
        vmx_state->pri_proc_ctrls.cr3_str_exit = 1;
	
	vmx_state->pri_proc_ctrls.invlpg_exit = 1;
	
	/* Add page fault exits */
	vmx_state->excp_bmap.pf = 1;

	// Setup VMX Assist
	v3_vmxassist_init(core, vmx_state);

	// Hook all accesses to EFER register
	v3_hook_msr(core->vm_info, EFER_MSR, 
		    &v3_handle_efer_read,
		    &v3_handle_efer_write, 
		    core);

    } else if ((core->shdw_pg_mode == NESTED_PAGING) && 
	       (v3_mach_type == V3_VMX_EPT_CPU)) {

#define CR0_PE 0x00000001
#define CR0_PG 0x80000000
#define CR0_WP 0x00010000 // To ensure mem hooks work
#define CR0_NE 0x00000020
        vmx_ret |= check_vmcs_write(VMCS_CR0_MASK, (CR0_PE | CR0_PG | CR0_WP | CR0_NE));

        // vmx_state->pinbased_ctrls |= NMI_EXIT;

	// Cause VM_EXIT whenever CR4.VMXE or CR4.PAE bits are written
	vmx_ret |= check_vmcs_write(VMCS_CR4_MASK, CR4_VMXE | CR4_PAE);
	
        /* Disable CR exits */
	vmx_state->pri_proc_ctrls.cr3_ld_exit = 0;
	vmx_state->pri_proc_ctrls.cr3_str_exit = 0;

	vmx_state->pri_proc_ctrls.invlpg_exit = 0;

	/* Add page fault exits */
	//	vmx_state->excp_bmap.pf = 1; // This should never happen..., enabled to catch bugs
	
	// Setup VMX Assist
	v3_vmxassist_init(core, vmx_state);

	/* Enable EPT */
	vmx_state->pri_proc_ctrls.sec_ctrls = 1; // Enable secondary proc controls
	vmx_state->sec_proc_ctrls.enable_ept = 1; // enable EPT paging



	if (v3_init_ept(core, &hw_info) == -1) {
	    PrintError("Error initializing EPT\n");
	    return -1;
	}

	// Hook all accesses to EFER register
	v3_hook_msr(core->vm_info, EFER_MSR, NULL, NULL, NULL);

    } else if ((core->shdw_pg_mode == NESTED_PAGING) && 
	       (v3_mach_type == V3_VMX_EPT_UG_CPU)) {
	int i = 0;
	// For now we will assume that unrestricted guest mode is assured w/ EPT


	core->vm_regs.rsp = 0x00;
	core->rip = 0xfff0;
	core->vm_regs.rdx = 0x00000f00;
	core->ctrl_regs.rflags = 0x00000002; // The reserved bit is always 1
	core->ctrl_regs.cr0 = 0x60010030; 
	core->ctrl_regs.cr4 = 0x00002010; // Enable VMX and PSE flag
	

	core->segments.cs.selector = 0xf000;
	core->segments.cs.limit = 0xffff;
	core->segments.cs.base = 0x0000000f0000LL;

	// (raw attributes = 0xf3)
	core->segments.cs.type = 0xb;
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


	    seg->type = 0x3;
	    seg->system = 0x1;
	    seg->dpl = 0x0;
	    seg->present = 1;
	    //    seg->granularity = 1;

	}


	core->segments.gdtr.limit = 0x0000ffff;
	core->segments.gdtr.base = 0x0000000000000000LL;

	core->segments.idtr.limit = 0x0000ffff;
	core->segments.idtr.base = 0x0000000000000000LL;

	core->segments.ldtr.selector = 0x0000;
	core->segments.ldtr.limit = 0x0000ffff;
	core->segments.ldtr.base = 0x0000000000000000LL;
	core->segments.ldtr.type = 0x2;
	core->segments.ldtr.present = 1;

	core->segments.tr.selector = 0x0000;
	core->segments.tr.limit = 0x0000ffff;
	core->segments.tr.base = 0x0000000000000000LL;
	core->segments.tr.type = 0xb;
	core->segments.tr.present = 1;

	//	core->dbg_regs.dr6 = 0x00000000ffff0ff0LL;
	core->dbg_regs.dr7 = 0x0000000000000400LL;

	/* Enable EPT */
	vmx_state->pri_proc_ctrls.sec_ctrls = 1; // Enable secondary proc controls
	vmx_state->sec_proc_ctrls.enable_ept = 1; // enable EPT paging
	vmx_state->sec_proc_ctrls.unrstrct_guest = 1; // enable unrestricted guest operation


	/* Disable shadow paging stuff */
	vmx_state->pri_proc_ctrls.cr3_ld_exit = 0;
	vmx_state->pri_proc_ctrls.cr3_str_exit = 0;

	vmx_state->pri_proc_ctrls.invlpg_exit = 0;


	// Cause VM_EXIT whenever the CR4.VMXE bit is set
	vmx_ret |= check_vmcs_write(VMCS_CR4_MASK, CR4_VMXE);
#define CR0_NE 0x00000020
	vmx_ret |= check_vmcs_write(VMCS_CR0_MASK, CR0_NE);
	((struct cr0_32 *)&(core->shdw_pg_state.guest_cr0))->ne = 1;

	if (v3_init_ept(core, &hw_info) == -1) {
	    PrintError("Error initializing EPT\n");
	    return -1;
	}

	// Hook all accesses to EFER register
	//	v3_hook_msr(core->vm_info, EFER_MSR, &debug_efer_read, &debug_efer_write, core);
	v3_hook_msr(core->vm_info, EFER_MSR, NULL, NULL, NULL);
    } else {
	PrintError("Invalid Virtual paging mode (pg_mode=%d) (mach_type=%d)\n", core->shdw_pg_mode, v3_mach_type);
	return -1;
    }


    // hook vmx msrs

    // Setup SYSCALL/SYSENTER MSRs in load/store area
    
    // save STAR, LSTAR, FMASK, KERNEL_GS_BASE MSRs in MSR load/store area
    {

	struct vmcs_msr_save_area * msr_entries = NULL;
	int max_msrs = (hw_info.misc_info.max_msr_cache_size + 1) * 4;
	int msr_ret = 0;

	V3_Print("Setting up MSR load/store areas (max_msr_count=%d)\n", max_msrs);

	if (max_msrs < 4) {
	    PrintError("Max MSR cache size is too small (%d)\n", max_msrs);
	    return -1;
	}

	vmx_state->msr_area_paddr = (addr_t)V3_AllocPages(1);
	
	if (vmx_state->msr_area_paddr == (addr_t)NULL) {
	    PrintError("could not allocate msr load/store area\n");
	    return -1;
	}

	msr_entries = (struct vmcs_msr_save_area *)V3_VAddr((void *)(vmx_state->msr_area_paddr));
	vmx_state->msr_area = msr_entries; // cache in vmx_info

	memset(msr_entries, 0, PAGE_SIZE);

	msr_entries->guest_star.index = IA32_STAR_MSR;
	msr_entries->guest_lstar.index = IA32_LSTAR_MSR;
	msr_entries->guest_fmask.index = IA32_FMASK_MSR;
	msr_entries->guest_kern_gs.index = IA32_KERN_GS_BASE_MSR;

	msr_entries->host_star.index = IA32_STAR_MSR;
	msr_entries->host_lstar.index = IA32_LSTAR_MSR;
	msr_entries->host_fmask.index = IA32_FMASK_MSR;
	msr_entries->host_kern_gs.index = IA32_KERN_GS_BASE_MSR;

	msr_ret |= check_vmcs_write(VMCS_EXIT_MSR_STORE_CNT, 4);
	msr_ret |= check_vmcs_write(VMCS_EXIT_MSR_LOAD_CNT, 4);
	msr_ret |= check_vmcs_write(VMCS_ENTRY_MSR_LOAD_CNT, 4);

	msr_ret |= check_vmcs_write(VMCS_EXIT_MSR_STORE_ADDR, (addr_t)V3_PAddr(msr_entries->guest_msrs));
	msr_ret |= check_vmcs_write(VMCS_ENTRY_MSR_LOAD_ADDR, (addr_t)V3_PAddr(msr_entries->guest_msrs));
	msr_ret |= check_vmcs_write(VMCS_EXIT_MSR_LOAD_ADDR, (addr_t)V3_PAddr(msr_entries->host_msrs));


	msr_ret |= v3_hook_msr(core->vm_info, IA32_STAR_MSR, NULL, NULL, NULL);
	msr_ret |= v3_hook_msr(core->vm_info, IA32_LSTAR_MSR, NULL, NULL, NULL);
	msr_ret |= v3_hook_msr(core->vm_info, IA32_FMASK_MSR, NULL, NULL, NULL);
	msr_ret |= v3_hook_msr(core->vm_info, IA32_KERN_GS_BASE_MSR, NULL, NULL, NULL);


	// IMPORTANT: These MSRs appear to be cached by the hardware....
	msr_ret |= v3_hook_msr(core->vm_info, SYSENTER_CS_MSR, NULL, NULL, NULL);
	msr_ret |= v3_hook_msr(core->vm_info, SYSENTER_ESP_MSR, NULL, NULL, NULL);
	msr_ret |= v3_hook_msr(core->vm_info, SYSENTER_EIP_MSR, NULL, NULL, NULL);

	msr_ret |= v3_hook_msr(core->vm_info, FS_BASE_MSR, NULL, NULL, NULL);
	msr_ret |= v3_hook_msr(core->vm_info, GS_BASE_MSR, NULL, NULL, NULL);

	msr_ret |= v3_hook_msr(core->vm_info, IA32_PAT_MSR, NULL, NULL, NULL);

	// Not sure what to do about this... Does not appear to be an explicit hardware cache version...
	msr_ret |= v3_hook_msr(core->vm_info, IA32_CSTAR_MSR, NULL, NULL, NULL);

	if (msr_ret != 0) {
	    PrintError("Error configuring MSR save/restore area\n");
	    return -1;
	}


    }    

    /* Sanity check ctrl/reg fields against hw_defaults */




    /*** Write all the info to the VMCS ***/
  
    /*
    {
	// IS THIS NECESSARY???
#define DEBUGCTL_MSR 0x1d9
	struct v3_msr tmp_msr;
	v3_get_msr(DEBUGCTL_MSR, &(tmp_msr.hi), &(tmp_msr.lo));
	vmx_ret |= check_vmcs_write(VMCS_GUEST_DBG_CTL, tmp_msr.value);
	core->dbg_regs.dr7 = 0x400;
    }
    */

#ifdef __V3_64BIT__
    vmx_ret |= check_vmcs_write(VMCS_LINK_PTR, (addr_t)0xffffffffffffffffULL);
#else
    vmx_ret |= check_vmcs_write(VMCS_LINK_PTR, (addr_t)0xffffffffUL);
    vmx_ret |= check_vmcs_write(VMCS_LINK_PTR_HIGH, (addr_t)0xffffffffUL);
#endif



 

    if (v3_update_vmcs_ctrl_fields(core)) {
        PrintError("Could not write control fields!\n");
        return -1;
    }
    
    /*
    if (v3_update_vmcs_host_state(core)) {
        PrintError("Could not write host state\n");
        return -1;
    }
    */

    // reenable global interrupts for vm state initialization now
    // that the vm state is initialized. If another VM kicks us off, 
    // it'll update our vmx state so that we know to reload ourself
    v3_enable_ints();

    return 0;
}


static void __init_vmx_vmcs(void * arg) {
    struct guest_info * core = arg;
    struct vmx_data * vmx_state = NULL;
    int vmx_ret = 0;
    
    vmx_state = (struct vmx_data *)V3_Malloc(sizeof(struct vmx_data));

    if (!vmx_state) {
	PrintError("Unable to allocate in initializing vmx vmcs\n");
	return;
    }

    memset(vmx_state, 0, sizeof(struct vmx_data));

    PrintDebug("vmx_data pointer: %p\n", (void *)vmx_state);

    PrintDebug("Allocating VMCS\n");
    vmx_state->vmcs_ptr_phys = allocate_vmcs();

    PrintDebug("VMCS pointer: %p\n", (void *)(vmx_state->vmcs_ptr_phys));

    core->vmm_data = vmx_state;
    vmx_state->state = VMX_UNLAUNCHED;

    PrintDebug("Initializing VMCS (addr=%p)\n", core->vmm_data);
    
    // TODO: Fix vmcs fields so they're 32-bit

    PrintDebug("Clearing VMCS: %p\n", (void *)vmx_state->vmcs_ptr_phys);
    vmx_ret = vmcs_clear(vmx_state->vmcs_ptr_phys);

    if (vmx_ret != VMX_SUCCESS) {
        PrintError("VMCLEAR failed\n");
        return; 
    }

    if (core->vm_info->vm_class == V3_PC_VM) {
	PrintDebug("Initializing VMCS\n");
	if (init_vmcs_bios(core, vmx_state) == -1) {
	    PrintError("Error initializing VMCS to BIOS state\n");
	    return;
	}
    } else {
	PrintError("Invalid VM Class\n");
	return;
    }

    PrintDebug("Serializing VMCS: %p\n", (void *)vmx_state->vmcs_ptr_phys);
    vmx_ret = vmcs_clear(vmx_state->vmcs_ptr_phys);

    core->core_run_state = CORE_STOPPED;
    return;
}



int v3_init_vmx_vmcs(struct guest_info * core, v3_vm_class_t vm_class) {
    extern v3_cpu_arch_t v3_cpu_types[];

    if (v3_cpu_types[V3_Get_CPU()] == V3_INVALID_CPU) {
	int i = 0;

	for (i = 0; i < V3_CONFIG_MAX_CPUS; i++) {
	    if (v3_cpu_types[i] != V3_INVALID_CPU) {
		break;
	    }
	}

	if (i == V3_CONFIG_MAX_CPUS) {
	    PrintError("Could not find VALID CPU for VMX guest initialization\n");
	    return -1;
	}

	V3_Call_On_CPU(i, __init_vmx_vmcs, core);

    } else {
	__init_vmx_vmcs(core);
    }

    if (core->core_run_state != CORE_STOPPED) {
	PrintError("Error initializing VMX Core\n");
	return -1;
    }

    return 0;
}


int v3_deinit_vmx_vmcs(struct guest_info * core) {
    struct vmx_data * vmx_state = core->vmm_data;

    V3_FreePages((void *)(vmx_state->vmcs_ptr_phys), 1);
    V3_FreePages(V3_PAddr(vmx_state->msr_area), 1);

    V3_Free(vmx_state);

    return 0;
}



#ifdef V3_CONFIG_CHECKPOINT
/* 
 * JRL: This is broken
 */
int v3_vmx_save_core(struct guest_info * core, void * ctx){
    struct vmx_data * vmx_info = (struct vmx_data *)(core->vmm_data);

    // note that the vmcs pointer is an HPA, but we need an HVA
    if (v3_chkpt_save(ctx, "vmcs_data", PAGE_SIZE_4KB, 
		      V3_VAddr((void*) (vmx_info->vmcs_ptr_phys))) ==-1) {
	PrintError("Could not save vmcs data for VMX\n");
	return -1;
    }

    return 0;
}

int v3_vmx_load_core(struct guest_info * core, void * ctx){
    struct vmx_data * vmx_info = (struct vmx_data *)(core->vmm_data);
    struct cr0_32 * shadow_cr0;
    addr_t vmcs_page_paddr;  //HPA

    vmcs_page_paddr = (addr_t) V3_AllocPages(1);
    
    if (!vmcs_page_paddr) { 
	PrintError("Could not allocate space for a vmcs in VMX\n");
	return -1;
    }

    if (v3_chkpt_load(ctx, "vmcs_data", PAGE_SIZE_4KB, 
		      V3_VAddr((void *)vmcs_page_paddr)) == -1) { 
	PrintError("Could not load vmcs data for VMX\n");
	return -1;
    }

    vmcs_clear(vmx_info->vmcs_ptr_phys);

    // Probably need to delete the old one... 
    V3_FreePages((void*)(vmx_info->vmcs_ptr_phys),1);

    vmcs_load(vmcs_page_paddr);

    v3_vmx_save_vmcs(core);

    shadow_cr0 = (struct cr0_32 *)&(core->ctrl_regs.cr0);


    /* Get the CPU mode to set the guest_ia32e entry ctrl */

    if (core->shdw_pg_mode == SHADOW_PAGING) {
	if (v3_get_vm_mem_mode(core) == VIRTUAL_MEM) {
	    if (v3_activate_shadow_pt(core) == -1) {
		PrintError("Failed to activate shadow page tables\n");
		return -1;
	    }
	} else {
	    if (v3_activate_passthrough_pt(core) == -1) {
		PrintError("Failed to activate passthrough page tables\n");
		return -1;
	    }
	}
    }

    return 0;
}
#endif


void v3_flush_vmx_vm_core(struct guest_info * core) {
    struct vmx_data * vmx_info = (struct vmx_data *)(core->vmm_data);
    vmcs_clear(vmx_info->vmcs_ptr_phys);
    vmx_info->state = VMX_UNLAUNCHED;
}



static int update_irq_exit_state(struct guest_info * info) {
    struct vmx_exit_idt_vec_info idt_vec_info;

    check_vmcs_read(VMCS_IDT_VECTOR_INFO, &(idt_vec_info.value));

    if ((info->intr_core_state.irq_started == 1) && (idt_vec_info.valid == 0)) {
#ifdef V3_CONFIG_DEBUG_INTERRUPTS
        V3_Print("Calling v3_injecting_intr\n");
#endif
        info->intr_core_state.irq_started = 0;
        v3_injecting_intr(info, info->intr_core_state.irq_vector, V3_EXTERNAL_IRQ);
    }

    return 0;
}

static int update_irq_entry_state(struct guest_info * info) {
    struct vmx_exit_idt_vec_info idt_vec_info;
    struct vmcs_interrupt_state intr_core_state;
    struct vmx_data * vmx_info = (struct vmx_data *)(info->vmm_data);

    check_vmcs_read(VMCS_IDT_VECTOR_INFO, &(idt_vec_info.value));
    check_vmcs_read(VMCS_GUEST_INT_STATE, &(intr_core_state));

    /* Check for pending exceptions to inject */
    if (v3_excp_pending(info)) {
        struct vmx_entry_int_info int_info;
        int_info.value = 0;

        // In VMX, almost every exception is hardware
        // Software exceptions are pretty much only for breakpoint or overflow
        int_info.type = 3;
        int_info.vector = v3_get_excp_number(info);

        if (info->excp_state.excp_error_code_valid) {
            check_vmcs_write(VMCS_ENTRY_EXCP_ERR, info->excp_state.excp_error_code);
            int_info.error_code = 1;

#ifdef V3_CONFIG_DEBUG_INTERRUPTS
            V3_Print("Injecting exception %d with error code %x\n", 
                    int_info.vector, info->excp_state.excp_error_code);
#endif
        }

        int_info.valid = 1;
#ifdef V3_CONFIG_DEBUG_INTERRUPTS
        V3_Print("Injecting exception %d (EIP=%p)\n", int_info.vector, (void *)(addr_t)info->rip);
#endif
        check_vmcs_write(VMCS_ENTRY_INT_INFO, int_info.value);

        v3_injecting_excp(info, int_info.vector);

    } else if ((((struct rflags *)&(info->ctrl_regs.rflags))->intr == 1) && 
	       (intr_core_state.val == 0)) {
       
        if ((info->intr_core_state.irq_started == 1) && (idt_vec_info.valid == 1)) {

#ifdef V3_CONFIG_DEBUG_INTERRUPTS
            V3_Print("IRQ pending from previous injection\n");
#endif

            // Copy the IDT vectoring info over to reinject the old interrupt
            if (idt_vec_info.error_code == 1) {
                uint32_t err_code = 0;

                check_vmcs_read(VMCS_IDT_VECTOR_ERR, &err_code);
                check_vmcs_write(VMCS_ENTRY_EXCP_ERR, err_code);
            }

            idt_vec_info.undef = 0;
            check_vmcs_write(VMCS_ENTRY_INT_INFO, idt_vec_info.value);

        } else {
            struct vmx_entry_int_info ent_int;
            ent_int.value = 0;

            switch (v3_intr_pending(info)) {
                case V3_EXTERNAL_IRQ: {
                    info->intr_core_state.irq_vector = v3_get_intr(info); 
                    ent_int.vector = info->intr_core_state.irq_vector;
                    ent_int.type = 0;
                    ent_int.error_code = 0;
                    ent_int.valid = 1;

#ifdef V3_CONFIG_DEBUG_INTERRUPTS
                    V3_Print("Injecting Interrupt %d at exit %u(EIP=%p)\n", 
			       info->intr_core_state.irq_vector, 
			       (uint32_t)info->num_exits, 
			       (void *)(addr_t)info->rip);
#endif

                    check_vmcs_write(VMCS_ENTRY_INT_INFO, ent_int.value);
                    info->intr_core_state.irq_started = 1;

                    break;
                }
                case V3_NMI:
                    PrintDebug("Injecting NMI\n");

                    ent_int.type = 2;
                    ent_int.vector = 2;
                    ent_int.valid = 1;
                    check_vmcs_write(VMCS_ENTRY_INT_INFO, ent_int.value);

                    break;
                case V3_SOFTWARE_INTR:
                    PrintDebug("Injecting software interrupt\n");
                    ent_int.type = 4;

                    ent_int.valid = 1;
                    check_vmcs_write(VMCS_ENTRY_INT_INFO, ent_int.value);

		    break;
                case V3_VIRTUAL_IRQ:
                    // Not sure what to do here, Intel doesn't have virtual IRQs
                    // May be the same as external interrupts/IRQs

		    break;
                case V3_INVALID_INTR:
                default:
                    break;
            }
        }
    } else if ((v3_intr_pending(info)) && (vmx_info->pri_proc_ctrls.int_wndw_exit == 0)) {
        // Enable INTR window exiting so we know when IF=1
        uint32_t instr_len;

        check_vmcs_read(VMCS_EXIT_INSTR_LEN, &instr_len);

#ifdef V3_CONFIG_DEBUG_INTERRUPTS
        V3_Print("Enabling Interrupt-Window exiting: %d\n", instr_len);
#endif

        vmx_info->pri_proc_ctrls.int_wndw_exit = 1;
        check_vmcs_write(VMCS_PROC_CTRLS, vmx_info->pri_proc_ctrls.value);
    }


    return 0;
}



static struct vmx_exit_info exit_log[10];
static uint64_t rip_log[10];



static void print_exit_log(struct guest_info * info) {
    int cnt = info->num_exits % 10;
    int i = 0;
    

    V3_Print("\nExit Log (%d total exits):\n", (uint32_t)info->num_exits);

    for (i = 0; i < 10; i++) {
	struct vmx_exit_info * tmp = &exit_log[cnt];

	V3_Print("%d:\texit_reason = %p\n", i, (void *)(addr_t)tmp->exit_reason);
	V3_Print("\texit_qual = %p\n", (void *)tmp->exit_qual);
	V3_Print("\tint_info = %p\n", (void *)(addr_t)tmp->int_info);
	V3_Print("\tint_err = %p\n", (void *)(addr_t)tmp->int_err);
	V3_Print("\tinstr_info = %p\n", (void *)(addr_t)tmp->instr_info);
	V3_Print("\tguest_linear_addr= %p\n", (void *)(addr_t)tmp->guest_linear_addr);
	V3_Print("\tRIP = %p\n", (void *)rip_log[cnt]);


	cnt--;

	if (cnt == -1) {
	    cnt = 9;
	}

    }

}

int 
v3_vmx_config_tsc_virtualization(struct guest_info * info) {
    struct vmx_data * vmx_info = (struct vmx_data *)(info->vmm_data);

    if (info->time_state.flags & VM_TIME_TRAP_RDTSC) {
	if  (!vmx_info->pri_proc_ctrls.rdtsc_exit) {
	    vmx_info->pri_proc_ctrls.rdtsc_exit = 1;
	    check_vmcs_write(VMCS_PROC_CTRLS, vmx_info->pri_proc_ctrls.value);
	}
    } else {
        sint64_t tsc_offset;
        uint32_t tsc_offset_low, tsc_offset_high;

	if  (vmx_info->pri_proc_ctrls.rdtsc_exit) {
	    vmx_info->pri_proc_ctrls.rdtsc_exit = 0;
	    check_vmcs_write(VMCS_PROC_CTRLS, vmx_info->pri_proc_ctrls.value);
	}

	if (info->time_state.flags & VM_TIME_TSC_PASSTHROUGH) {
	    tsc_offset = 0;
	} else {
            tsc_offset = v3_tsc_host_offset(&info->time_state);
	}
        tsc_offset_high = (uint32_t)(( tsc_offset >> 32) & 0xffffffff);
        tsc_offset_low = (uint32_t)(tsc_offset & 0xffffffff);

        check_vmcs_write(VMCS_TSC_OFFSET_HIGH, tsc_offset_high);
        check_vmcs_write(VMCS_TSC_OFFSET, tsc_offset_low);
    }
    return 0;
}

/* 
 * CAUTION and DANGER!!! 
 * 
 * The VMCS CANNOT(!!) be accessed outside of the cli/sti calls inside this function
 * When exectuing a symbiotic call, the VMCS WILL be overwritten, so any dependencies 
 * on its contents will cause things to break. The contents at the time of the exit WILL 
 * change before the exit handler is executed.
 */
int v3_vmx_enter(struct guest_info * info) {
    int ret = 0;
    struct vmx_exit_info exit_info;
    struct vmx_data * vmx_info = (struct vmx_data *)(info->vmm_data);
    uint64_t guest_cycles = 0;

    // Conditionally yield the CPU if the timeslice has expired
    v3_yield_cond(info,-1);

    // Update timer devices late after being in the VM so that as much 
    // of the time in the VM is accounted for as possible. Also do it before
    // updating IRQ entry state so that any interrupts the timers raise get 
    // handled on the next VM entry.
    v3_advance_time(info, NULL);
    v3_update_timers(info);

    // disable global interrupts for vm state transition
    v3_disable_ints();

    if (vmcs_store() != vmx_info->vmcs_ptr_phys) {
	vmcs_clear(vmx_info->vmcs_ptr_phys);
	vmcs_load(vmx_info->vmcs_ptr_phys);
	vmx_info->state = VMX_UNLAUNCHED;
    }

    v3_vmx_restore_vmcs(info);


#ifdef V3_CONFIG_SYMCALL
    if (info->sym_core_state.symcall_state.sym_call_active == 0) {
	update_irq_entry_state(info);
    }
#else 
    update_irq_entry_state(info);
#endif

    {
	addr_t guest_cr3;
	vmcs_read(VMCS_GUEST_CR3, &guest_cr3);
	vmcs_write(VMCS_GUEST_CR3, guest_cr3);
    }


    // Perform last-minute time setup prior to entering the VM
    v3_vmx_config_tsc_virtualization(info);

    if (v3_update_vmcs_host_state(info)) {
	v3_enable_ints();
        PrintError("Could not write host state\n");
        return -1;
    }
    
    if (vmx_info->pin_ctrls.active_preempt_timer) {
	/* Preemption timer is active */
	uint32_t preempt_window = 0xffffffff;

	if (info->timeouts.timeout_active) {
	    preempt_window = info->timeouts.next_timeout;
	}
	
	check_vmcs_write(VMCS_PREEMPT_TIMER, preempt_window);
    }
   

    {	
	uint64_t entry_tsc = 0;
	uint64_t exit_tsc = 0;

	if (vmx_info->state == VMX_UNLAUNCHED) {
	    vmx_info->state = VMX_LAUNCHED;
	    rdtscll(entry_tsc);
	    ret = v3_vmx_launch(&(info->vm_regs), info, &(info->ctrl_regs));
	    rdtscll(exit_tsc);

	} else {
	    V3_ASSERT(vmx_info->state != VMX_UNLAUNCHED);
	    rdtscll(entry_tsc);
	    ret = v3_vmx_resume(&(info->vm_regs), info, &(info->ctrl_regs));
	    rdtscll(exit_tsc);
	}

	guest_cycles = exit_tsc - entry_tsc;	
    }

    //  PrintDebug("VMX Exit: ret=%d\n", ret);

    if (ret != VMX_SUCCESS) {
	uint32_t error = 0;
        vmcs_read(VMCS_INSTR_ERR, &error);

	v3_enable_ints();

	PrintError("VMENTRY Error: %d (launch_ret = %d)\n", error, ret);
	return -1;
    }


    info->num_exits++;

    /* If we have the preemption time, then use it to get more accurate guest time */
    if (vmx_info->pin_ctrls.active_preempt_timer) {
	uint32_t cycles_left = 0;
	check_vmcs_read(VMCS_PREEMPT_TIMER, &(cycles_left));

	if (info->timeouts.timeout_active) {
	    guest_cycles = info->timeouts.next_timeout - cycles_left;
	} else {
	    guest_cycles = 0xffffffff - cycles_left;
	}
    }

    // Immediate exit from VM time bookkeeping
    v3_advance_time(info, &guest_cycles);

    /* Update guest state */
    v3_vmx_save_vmcs(info);

    // info->cpl = info->segments.cs.selector & 0x3;

    info->mem_mode = v3_get_vm_mem_mode(info);
    info->cpu_mode = v3_get_vm_cpu_mode(info);



    check_vmcs_read(VMCS_EXIT_INSTR_LEN, &(exit_info.instr_len));
    check_vmcs_read(VMCS_EXIT_INSTR_INFO, &(exit_info.instr_info));
    check_vmcs_read(VMCS_EXIT_REASON, &(exit_info.exit_reason));
    check_vmcs_read(VMCS_EXIT_QUAL, &(exit_info.exit_qual));
    check_vmcs_read(VMCS_EXIT_INT_INFO, &(exit_info.int_info));
    check_vmcs_read(VMCS_EXIT_INT_ERR, &(exit_info.int_err));
    check_vmcs_read(VMCS_GUEST_LINEAR_ADDR, &(exit_info.guest_linear_addr));

    if (info->shdw_pg_mode == NESTED_PAGING) {
	check_vmcs_read(VMCS_GUEST_PHYS_ADDR, &(exit_info.ept_fault_addr));
    }

    //PrintDebug("VMX Exit taken, id-qual: %u-%lu\n", exit_info.exit_reason, exit_info.exit_qual);

    exit_log[info->num_exits % 10] = exit_info;
    rip_log[info->num_exits % 10] = get_addr_linear(info, info->rip, &(info->segments.cs));

#ifdef V3_CONFIG_SYMCALL
    if (info->sym_core_state.symcall_state.sym_call_active == 0) {
	update_irq_exit_state(info);
    }
#else
    update_irq_exit_state(info);
#endif

    if (exit_info.exit_reason == VMX_EXIT_INTR_WINDOW) {
	// This is a special case whose only job is to inject an interrupt
	vmcs_read(VMCS_PROC_CTRLS, &(vmx_info->pri_proc_ctrls.value));
        vmx_info->pri_proc_ctrls.int_wndw_exit = 0;
        vmcs_write(VMCS_PROC_CTRLS, vmx_info->pri_proc_ctrls.value);

#ifdef V3_CONFIG_DEBUG_INTERRUPTS
       V3_Print("Interrupts available again! (RIP=%llx)\n", info->rip);
#endif
    }


    // Lastly we check for an NMI exit, and reinject if so
    {
	struct vmx_basic_exit_info * basic_info = (struct vmx_basic_exit_info *)&(exit_info.exit_reason);

	if (basic_info->reason == VMX_EXIT_INFO_EXCEPTION_OR_NMI) {
	    if ((uint8_t)exit_info.int_info == 2) {
		asm("int $2");
	    }
	}
    }

    // reenable global interrupts after vm exit
    v3_enable_ints();

    // Conditionally yield the CPU if the timeslice has expired
    v3_yield_cond(info,-1);
    v3_advance_time(info, NULL);
    v3_update_timers(info);

    if (v3_handle_vmx_exit(info, &exit_info) == -1) {
	PrintError("Error in VMX exit handler (Exit reason=%x)\n", exit_info.exit_reason);
	return -1;
    }

    if (info->timeouts.timeout_active) {
	/* Check to see if any timeouts have expired */
	v3_handle_timeouts(info, guest_cycles);
    }

    return 0;
}


int v3_start_vmx_guest(struct guest_info * info) {

    PrintDebug("Starting VMX core %u\n", info->vcpu_id);

    if (info->vcpu_id == 0) {
	info->core_run_state = CORE_RUNNING;
    } else {

        PrintDebug("VMX core %u: Waiting for core initialization\n", info->vcpu_id);

        while (info->core_run_state == CORE_STOPPED) {

	    if (info->vm_info->run_state == VM_STOPPED) {
		// The VM was stopped before this core was initialized. 
		return 0;
	    }

            v3_yield(info,-1);
            //PrintDebug("VMX core %u: still waiting for INIT\n",info->vcpu_id);
        }
	
	PrintDebug("VMX core %u initialized\n", info->vcpu_id);

	// We'll be paranoid about race conditions here
	v3_wait_at_barrier(info);
    }


    PrintDebug("VMX core %u: I am starting at CS=0x%x (base=0x%p, limit=0x%x),  RIP=0x%p\n",
               info->vcpu_id, info->segments.cs.selector, (void *)(info->segments.cs.base),
               info->segments.cs.limit, (void *)(info->rip));


    PrintDebug("VMX core %u: Launching VMX VM on logical core %u\n", info->vcpu_id, info->pcpu_id);

    v3_start_time(info);

    while (1) {

	if (info->vm_info->run_state == VM_STOPPED) {
	    info->core_run_state = CORE_STOPPED;
	    break;
	}

	if (v3_vmx_enter(info) == -1) {

	    addr_t host_addr;
            addr_t linear_addr = 0;
            
            info->vm_info->run_state = VM_ERROR;
            
            V3_Print("VMX core %u: VMX ERROR!!\n", info->vcpu_id); 
            
            v3_print_guest_state(info);
            
            V3_Print("VMX core %u\n", info->vcpu_id); 

            linear_addr = get_addr_linear(info, info->rip, &(info->segments.cs));
            
            if (info->mem_mode == PHYSICAL_MEM) {
                v3_gpa_to_hva(info, linear_addr, &host_addr);
            } else if (info->mem_mode == VIRTUAL_MEM) {
                v3_gva_to_hva(info, linear_addr, &host_addr);
            }
            
            V3_Print("VMX core %u: Host Address of rip = 0x%p\n", info->vcpu_id, (void *)host_addr);
            
            V3_Print("VMX core %u: Instr (15 bytes) at %p:\n", info->vcpu_id, (void *)host_addr);
            v3_dump_mem((uint8_t *)host_addr, 15);
            
            v3_print_stack(info);


	    v3_print_vmcs();
	    print_exit_log(info);
	    return -1;
	}

	v3_wait_at_barrier(info);


	if (info->vm_info->run_state == VM_STOPPED) {
	    info->core_run_state = CORE_STOPPED;
	    break;
	}
/*
	if ((info->num_exits % 5000) == 0) {
	    V3_Print("VMX Exit number %d\n", (uint32_t)info->num_exits);
	}
*/

    }

    return 0;
}




#define VMX_FEATURE_CONTROL_MSR     0x0000003a
#define CPUID_VMX_FEATURES 0x00000005  /* LOCK and VMXON */
#define CPUID_1_ECX_VTXFLAG 0x00000020

int v3_is_vmx_capable() {
    v3_msr_t feature_msr;
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;

    v3_cpuid(0x1, &eax, &ebx, &ecx, &edx);

    PrintDebug("ECX: 0x%x\n", ecx);

    if (ecx & CPUID_1_ECX_VTXFLAG) {
        v3_get_msr(VMX_FEATURE_CONTROL_MSR, &(feature_msr.hi), &(feature_msr.lo));
	
        PrintDebug("MSRREGlow: 0x%.8x\n", feature_msr.lo);

        if ((feature_msr.lo & CPUID_VMX_FEATURES) != CPUID_VMX_FEATURES) {
            PrintDebug("VMX is locked -- enable in the BIOS\n");
            return 0;
        }

    } else {
        PrintDebug("VMX not supported on this cpu\n");
        return 0;
    }

    return 1;
}


int v3_reset_vmx_vm_core(struct guest_info * core, addr_t rip) {
    // init vmcs bios
    
    if ((core->shdw_pg_mode == NESTED_PAGING) && 
	(v3_mach_type == V3_VMX_EPT_UG_CPU)) {
	// easy 
        core->rip = 0;
	core->segments.cs.selector = rip << 8;
	core->segments.cs.limit = 0xffff;
	core->segments.cs.base = rip << 12;
    } else {
	core->vm_regs.rdx = core->vcpu_id;
	core->vm_regs.rbx = rip;
    }

    return 0;
}



void v3_init_vmx_cpu(int cpu_id) {
    addr_t vmx_on_region = 0;
    extern v3_cpu_arch_t v3_mach_type;
    extern v3_cpu_arch_t v3_cpu_types[];

    if (v3_mach_type == V3_INVALID_CPU) {
	if (v3_init_vmx_hw(&hw_info) == -1) {
	    PrintError("Could not initialize VMX hardware features on cpu %d\n", cpu_id);
	    return;
	}
    }

    enable_vmx();


    // Setup VMXON Region
    vmx_on_region = allocate_vmcs();


    if (vmx_on(vmx_on_region) == VMX_SUCCESS) {
        V3_Print("VMX Enabled\n");
	host_vmcs_ptrs[cpu_id] = vmx_on_region;
    } else {
        V3_Print("VMX already enabled\n");
	V3_FreePages((void *)vmx_on_region, 1);
    }

    PrintDebug("VMXON pointer: 0x%p\n", (void *)host_vmcs_ptrs[cpu_id]);    

    {
	struct vmx_sec_proc_ctrls sec_proc_ctrls;
	sec_proc_ctrls.value = v3_vmx_get_ctrl_features(&(hw_info.sec_proc_ctrls));
	
	if (sec_proc_ctrls.enable_ept == 0) {
	    V3_Print("VMX EPT (Nested) Paging not supported\n");
	    v3_cpu_types[cpu_id] = V3_VMX_CPU;
	} else if (sec_proc_ctrls.unrstrct_guest == 0) {
	    V3_Print("VMX EPT (Nested) Paging supported\n");
	    v3_cpu_types[cpu_id] = V3_VMX_EPT_CPU;
	} else {
	    V3_Print("VMX EPT (Nested) Paging + Unrestricted guest supported\n");
	    v3_cpu_types[cpu_id] = V3_VMX_EPT_UG_CPU;
	}
    }
    
}


void v3_deinit_vmx_cpu(int cpu_id) {
    extern v3_cpu_arch_t v3_cpu_types[];
    v3_cpu_types[cpu_id] = V3_INVALID_CPU;

    if (host_vmcs_ptrs[cpu_id] != 0) {
	V3_Print("Disabling VMX\n");

	if (vmx_off() != VMX_SUCCESS) {
	    PrintError("Error executing VMXOFF\n");
	}

	V3_FreePages((void *)host_vmcs_ptrs[cpu_id], 1);

	host_vmcs_ptrs[cpu_id] = 0;
    }
}
