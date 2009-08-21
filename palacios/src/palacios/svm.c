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

#include <palacios/vmm_profiler.h>

#include <palacios/vmm_direct_paging.h>

#include <palacios/vmm_ctrl_regs.h>
#include <palacios/vmm_config.h>
#include <palacios/svm_io.h>



// This is a global pointer to the host's VMCB
static void * host_vmcb = NULL;

extern void v3_stgi();
extern void v3_clgi();
//extern int v3_svm_launch(vmcb_t * vmcb, struct v3_gprs * vm_regs, uint64_t * fs, uint64_t * gs);
extern int v3_svm_launch(vmcb_t * vmcb, struct v3_gprs * vm_regs, vmcb_t * host_vmcb);


static vmcb_t * Allocate_VMCB() {
    vmcb_t * vmcb_page = (vmcb_t *)V3_VAddr(V3_AllocPages(1));

    memset(vmcb_page, 0, 4096);

    return vmcb_page;
}



static void Init_VMCB_BIOS(vmcb_t * vmcb, struct guest_info *vm_info) {
    vmcb_ctrl_t * ctrl_area = GET_VMCB_CTRL_AREA(vmcb);
    vmcb_saved_state_t * guest_state = GET_VMCB_SAVE_STATE_AREA(vmcb);
    uint_t i;


    //
    guest_state->rsp = 0x00;
    guest_state->rip = 0xfff0;


    guest_state->cpl = 0;

    guest_state->efer |= EFER_MSR_svm_enable;


    guest_state->rflags = 0x00000002; // The reserved bit is always 1
    ctrl_area->svm_instrs.VMRUN = 1;
    ctrl_area->svm_instrs.VMMCALL = 1;
    ctrl_area->svm_instrs.VMLOAD = 1;
    ctrl_area->svm_instrs.VMSAVE = 1;
    ctrl_area->svm_instrs.STGI = 1;
    ctrl_area->svm_instrs.CLGI = 1;
    ctrl_area->svm_instrs.SKINIT = 1;
    ctrl_area->svm_instrs.RDTSCP = 1;
    ctrl_area->svm_instrs.ICEBP = 1;
    ctrl_area->svm_instrs.WBINVD = 1;
    ctrl_area->svm_instrs.MONITOR = 1;
    ctrl_area->svm_instrs.MWAIT_always = 1;
    ctrl_area->svm_instrs.MWAIT_if_armed = 1;
    ctrl_area->instrs.INVLPGA = 1;


    ctrl_area->instrs.HLT = 1;
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
    ctrl_area->instrs.SMI = 1;
    ctrl_area->instrs.INIT = 1;
    ctrl_area->instrs.PAUSE = 1;
    ctrl_area->instrs.shutdown_evts = 1;

    vm_info->vm_regs.rdx = 0x00000f00;

    guest_state->cr0 = 0x60000010;


    guest_state->cs.selector = 0xf000;
    guest_state->cs.limit = 0xffff;
    guest_state->cs.base = 0x0000000f0000LL;
    guest_state->cs.attrib.raw = 0xf3;


    /* DEBUG FOR RETURN CODE */
    ctrl_area->exit_code = 1;


    struct vmcb_selector *segregs [] = {&(guest_state->ss), &(guest_state->ds), 
					&(guest_state->es), &(guest_state->fs), 
					&(guest_state->gs), NULL};

    for ( i = 0; segregs[i] != NULL; i++) {
	struct vmcb_selector * seg = segregs[i];
	
	seg->selector = 0x0000;
	//    seg->base = seg->selector << 4;
	seg->base = 0x00000000;
	seg->attrib.raw = 0xf3;
	seg->limit = ~0u;
    }

    guest_state->gdtr.limit = 0x0000ffff;
    guest_state->gdtr.base = 0x0000000000000000LL;
    guest_state->idtr.limit = 0x0000ffff;
    guest_state->idtr.base = 0x0000000000000000LL;

    guest_state->ldtr.selector = 0x0000;
    guest_state->ldtr.limit = 0x0000ffff;
    guest_state->ldtr.base = 0x0000000000000000LL;
    guest_state->tr.selector = 0x0000;
    guest_state->tr.limit = 0x0000ffff;
    guest_state->tr.base = 0x0000000000000000LL;


    guest_state->dr6 = 0x00000000ffff0ff0LL;
    guest_state->dr7 = 0x0000000000000400LL;


    v3_init_svm_io_map(vm_info);
    ctrl_area->IOPM_BASE_PA = (addr_t)V3_PAddr(vm_info->io_map.arch_data);
    ctrl_area->instrs.IOIO_PROT = 1;



    v3_init_svm_msr_map(vm_info);
    ctrl_area->MSRPM_BASE_PA = (addr_t)V3_PAddr(vm_info->msr_map.arch_data);
    ctrl_area->instrs.MSR_PROT = 1;



    PrintDebug("Exiting on interrupts\n");
    ctrl_area->guest_ctrl.V_INTR_MASKING = 1;
    ctrl_area->instrs.INTR = 1;


    if (vm_info->shdw_pg_mode == SHADOW_PAGING) {
	PrintDebug("Creating initial shadow page table\n");
	
	/* JRL: This is a performance killer, and a simplistic solution */
	/* We need to fix this */
	ctrl_area->TLB_CONTROL = 1;
	ctrl_area->guest_ASID = 1;
	
	
	if (v3_init_passthrough_pts(vm_info) == -1) {
	    PrintError("Could not initialize passthrough page tables\n");
	    return ;
	}


	vm_info->shdw_pg_state.guest_cr0 = 0x0000000000000010LL;
	PrintDebug("Created\n");
	
	guest_state->cr3 = vm_info->direct_map_pt;

	ctrl_area->cr_reads.cr0 = 1;
	ctrl_area->cr_writes.cr0 = 1;
	//ctrl_area->cr_reads.cr4 = 1;
	ctrl_area->cr_writes.cr4 = 1;
	ctrl_area->cr_reads.cr3 = 1;
	ctrl_area->cr_writes.cr3 = 1;

	v3_hook_msr(vm_info, EFER_MSR, 
		    &v3_handle_efer_read,
		    &v3_handle_efer_write, 
		    vm_info);

	ctrl_area->instrs.INVLPG = 1;

	ctrl_area->exceptions.pf = 1;

	guest_state->g_pat = 0x7040600070406ULL;

	guest_state->cr0 |= 0x80000000;

    } else if (vm_info->shdw_pg_mode == NESTED_PAGING) {
	// Flush the TLB on entries/exits
	ctrl_area->TLB_CONTROL = 1;
	ctrl_area->guest_ASID = 1;

	// Enable Nested Paging
	ctrl_area->NP_ENABLE = 1;

	PrintDebug("NP_Enable at 0x%p\n", (void *)&(ctrl_area->NP_ENABLE));

	// Set the Nested Page Table pointer
	if (v3_init_passthrough_pts(vm_info) == -1) {
	    PrintError("Could not initialize Nested page tables\n");
	    return ;
	}

	ctrl_area->N_CR3 = vm_info->direct_map_pt;

	guest_state->g_pat = 0x7040600070406ULL;
    }
}


static int init_svm_guest(struct guest_info * info, struct v3_vm_config * config_ptr) {


    v3_pre_config_guest(info, config_ptr);

    PrintDebug("Allocating VMCB\n");
    info->vmm_data = (void*)Allocate_VMCB();

    PrintDebug("Initializing VMCB (addr=%p)\n", (void *)info->vmm_data);
    Init_VMCB_BIOS((vmcb_t*)(info->vmm_data), info);

    v3_post_config_guest(info, config_ptr);

    return 0;
}

static int start_svm_guest(struct guest_info *info) {
    //    vmcb_saved_state_t * guest_state = GET_VMCB_SAVE_STATE_AREA((vmcb_t*)(info->vmm_data));
    //  vmcb_ctrl_t * guest_ctrl = GET_VMCB_CTRL_AREA((vmcb_t*)(info->vmm_data));
    uint_t num_exits = 0;



    PrintDebug("Launching SVM VM (vmcb=%p)\n", (void *)info->vmm_data);
    //PrintDebugVMCB((vmcb_t*)(info->vmm_data));
    
    info->run_state = VM_RUNNING;
    rdtscll(info->yield_start_cycle);


    while (1) {
	ullong_t tmp_tsc;
	
	// Conditionally yield the CPU if the timeslice has expired
	v3_yield_cond(info);

	/*
	  PrintDebug("SVM Entry to CS=%p  rip=%p...\n", 
	  (void *)(addr_t)info->segments.cs.base, 
	  (void *)(addr_t)info->rip);
	*/

	// disable global interrupts for vm state transition
	v3_clgi();



	rdtscll(info->time_state.cached_host_tsc);
	//    guest_ctrl->TSC_OFFSET = info->time_state.guest_tsc - info->time_state.cached_host_tsc;
	
	v3_svm_launch((vmcb_t*)V3_PAddr(info->vmm_data), &(info->vm_regs), (vmcb_t *)host_vmcb);
	
	rdtscll(tmp_tsc);

	
	//PrintDebug("SVM Returned\n");

	// reenable global interrupts after vm exit
	v3_stgi();


	// Conditionally yield the CPU if the timeslice has expired
	v3_yield_cond(info);


	v3_update_time(info, tmp_tsc - info->time_state.cached_host_tsc);
	num_exits++;
	
	if ((num_exits % 5000) == 0) {
	    PrintDebug("SVM Exit number %d\n", num_exits);

#ifdef CONFIG_PROFILE_VMM
	    if (info->enable_profiler) {
		v3_print_profile(info);
	    }
#endif
	}

	if (v3_handle_svm_exit(info) != 0) {
	    vmcb_ctrl_t * guest_ctrl = GET_VMCB_CTRL_AREA((vmcb_t*)(info->vmm_data));
	    addr_t host_addr;
	    addr_t linear_addr = 0;
	    
	    info->run_state = VM_ERROR;
	    
	    PrintDebug("SVM ERROR!!\n"); 
      
	    v3_print_guest_state(info);

	    PrintDebug("SVM Exit Code: %p\n", (void *)(addr_t)guest_ctrl->exit_code); 
      
	    PrintDebug("exit_info1 low = 0x%.8x\n", *(uint_t*)&(guest_ctrl->exit_info1));
	    PrintDebug("exit_info1 high = 0x%.8x\n", *(uint_t *)(((uchar_t *)&(guest_ctrl->exit_info1)) + 4));
      
	    PrintDebug("exit_info2 low = 0x%.8x\n", *(uint_t*)&(guest_ctrl->exit_info2));
	    PrintDebug("exit_info2 high = 0x%.8x\n", *(uint_t *)(((uchar_t *)&(guest_ctrl->exit_info2)) + 4));
      
	    linear_addr = get_addr_linear(info, info->rip, &(info->segments.cs));

	    if (info->mem_mode == PHYSICAL_MEM) {
		guest_pa_to_host_va(info, linear_addr, &host_addr);
	    } else if (info->mem_mode == VIRTUAL_MEM) {
		guest_va_to_host_va(info, linear_addr, &host_addr);
	    }

	    PrintDebug("Host Address of rip = 0x%p\n", (void *)host_addr);

	    PrintDebug("Instr (15 bytes) at %p:\n", (void *)host_addr);
	    v3_dump_mem((uint8_t *)host_addr, 15);

	    break;
	}
    }
    return 0;
}





/* Checks machine SVM capability */
/* Implemented from: AMD Arch Manual 3, sect 15.4 */ 
int v3_is_svm_capable() {
    // Dinda
    uint_t vm_cr_low = 0, vm_cr_high = 0;
    addr_t eax = 0, ebx = 0, ecx = 0, edx = 0;

    v3_cpuid(CPUID_EXT_FEATURE_IDS, &eax, &ebx, &ecx, &edx);
  
    PrintDebug("CPUID_EXT_FEATURE_IDS_ecx=%p\n", (void *)ecx);

    if ((ecx & CPUID_EXT_FEATURE_IDS_ecx_svm_avail) == 0) {
      PrintDebug("SVM Not Available\n");
      return 0;
    }  else {
	v3_get_msr(SVM_VM_CR_MSR, &vm_cr_high, &vm_cr_low);
	
	PrintDebug("SVM_VM_CR_MSR = 0x%x 0x%x\n", vm_cr_high, vm_cr_low);
	
	if ((vm_cr_low & SVM_VM_CR_MSR_svmdis) == 1) {
	    PrintDebug("SVM is available but is disabled.\n");
	    
	    v3_cpuid(CPUID_SVM_REV_AND_FEATURE_IDS, &eax, &ebx, &ecx, &edx);
	    
	    PrintDebug("CPUID_SVM_REV_AND_FEATURE_IDS_edx=%p\n", (void *)edx);
	    
	    if ((edx & CPUID_SVM_REV_AND_FEATURE_IDS_edx_svml) == 0) {
		PrintDebug("SVM BIOS Disabled, not unlockable\n");
	    } else {
		PrintDebug("SVM is locked with a key\n");
	    }
	    return 0;

	} else {
	    PrintDebug("SVM is available and  enabled.\n");

	    v3_cpuid(CPUID_SVM_REV_AND_FEATURE_IDS, &eax, &ebx, &ecx, &edx);
	    PrintDebug("CPUID_SVM_REV_AND_FEATURE_IDS_eax=%p\n", (void *)eax);
	    PrintDebug("CPUID_SVM_REV_AND_FEATURE_IDS_ebx=%p\n", (void *)ebx);
	    PrintDebug("CPUID_SVM_REV_AND_FEATURE_IDS_ecx=%p\n", (void *)ecx);
	    PrintDebug("CPUID_SVM_REV_AND_FEATURE_IDS_edx=%p\n", (void *)edx);


	    if ((edx & CPUID_SVM_REV_AND_FEATURE_IDS_edx_np) == 0) {
		PrintDebug("SVM Nested Paging not supported\n");
	    } else {
		PrintDebug("SVM Nested Paging supported\n");
	    }

	    return 1;
	}
    }
}

static int has_svm_nested_paging() {
    addr_t eax = 0, ebx = 0, ecx = 0, edx = 0;

    v3_cpuid(CPUID_SVM_REV_AND_FEATURE_IDS, &eax, &ebx, &ecx, &edx);

    //PrintDebug("CPUID_EXT_FEATURE_IDS_edx=0x%x\n", edx);

    if ((edx & CPUID_SVM_REV_AND_FEATURE_IDS_edx_np) == 0) {
	PrintDebug("SVM Nested Paging not supported\n");
	return 0;
    } else {
	PrintDebug("SVM Nested Paging supported\n");
	return 1;
    }
}



void v3_init_SVM(struct v3_ctrl_ops * vmm_ops) {
    reg_ex_t msr;
    extern v3_cpu_arch_t v3_cpu_type;

    // Enable SVM on the CPU
    v3_get_msr(EFER_MSR, &(msr.e_reg.high), &(msr.e_reg.low));
    msr.e_reg.low |= EFER_MSR_svm_enable;
    v3_set_msr(EFER_MSR, 0, msr.e_reg.low);

    PrintDebug("SVM Enabled\n");

    // Setup the host state save area
    host_vmcb = V3_AllocPages(4);

    /* 64-BIT-ISSUE */
    //  msr.e_reg.high = 0;
    //msr.e_reg.low = (uint_t)host_vmcb;
    msr.r_reg = (addr_t)host_vmcb;

    PrintDebug("Host State being saved at %p\n", (void *)(addr_t)host_vmcb);
    v3_set_msr(SVM_VM_HSAVE_PA_MSR, msr.e_reg.high, msr.e_reg.low);




    if (has_svm_nested_paging() == 1) {
	v3_cpu_type = V3_SVM_REV3_CPU;
    } else {
	v3_cpu_type = V3_SVM_CPU;
    }

    // Setup the SVM specific vmm operations
    vmm_ops->init_guest = &init_svm_guest;
    vmm_ops->start_guest = &start_svm_guest;
    vmm_ops->has_nested_paging = &has_svm_nested_paging;

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
			  : "c"(host_vmcb), "0"(0), "1"(0), "2"(0), "3"(0)
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
			      : "c"(host_vmcb), "0"(0), "1"(0), "2"(0), "3"(0)
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


