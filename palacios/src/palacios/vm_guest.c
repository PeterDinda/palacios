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




#include <palacios/vm_guest.h>
#include <palacios/vmm_ctrl_regs.h>
#include <palacios/vmm.h>
#include <palacios/vmm_decoder.h>
#include <palacios/vmcb.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_lowlevel.h>
#include <palacios/vmm_sprintf.h>
#include <palacios/vmm_xed.h>
#include <palacios/vmm_direct_paging.h>
#include <palacios/vmm_barrier.h>


v3_cpu_mode_t v3_get_vm_cpu_mode(struct guest_info * info) {
    struct cr0_32 * cr0;
    struct efer_64 * efer;
    struct cr4_32 * cr4 = (struct cr4_32 *)&(info->ctrl_regs.cr4);
    struct v3_segment * cs = &(info->segments.cs);
    vmcb_saved_state_t * guest_state = GET_VMCB_SAVE_STATE_AREA((vmcb_t*)(info->vmm_data));

    if (info->shdw_pg_mode == SHADOW_PAGING) {
	cr0 = (struct cr0_32 *)&(info->shdw_pg_state.guest_cr0);
	efer = (struct efer_64 *)&(info->shdw_pg_state.guest_efer);
    } else if (info->shdw_pg_mode == NESTED_PAGING) {
	cr0 = (struct cr0_32 *)&(info->ctrl_regs.cr0);
	efer = (struct efer_64 *)&(guest_state->efer);
    } else {
	PrintError("Invalid Paging Mode...\n");
	V3_ASSERT(0);
	return -1;
    }

    if (cr0->pe == 0) {
	return REAL;
    } else if ((cr4->pae == 0) && (efer->lme == 0)) {
	return PROTECTED;
    } else if (efer->lme == 0) {
	return PROTECTED_PAE;
    } else if ((efer->lme == 1) && (cs->long_mode == 1)) {
	return LONG;
    } else {
	// What about LONG_16_COMPAT???
	return LONG_32_COMPAT;
    }
}

// Get address width in bytes
uint_t v3_get_addr_width(struct guest_info * info) {
    struct cr0_32 * cr0;
    struct cr4_32 * cr4 = (struct cr4_32 *)&(info->ctrl_regs.cr4);
    struct efer_64 * efer;
    struct v3_segment * cs = &(info->segments.cs);
    vmcb_saved_state_t * guest_state = GET_VMCB_SAVE_STATE_AREA((vmcb_t*)(info->vmm_data));

    if (info->shdw_pg_mode == SHADOW_PAGING) {
	cr0 = (struct cr0_32 *)&(info->shdw_pg_state.guest_cr0);
	efer = (struct efer_64 *)&(info->shdw_pg_state.guest_efer);
    } else if (info->shdw_pg_mode == NESTED_PAGING) {
	cr0 = (struct cr0_32 *)&(info->ctrl_regs.cr0);
	efer = (struct efer_64 *)&(guest_state->efer);
    } else {
	PrintError("Invalid Paging Mode...\n");
	V3_ASSERT(0);
	return -1;
    }

    if (cr0->pe == 0) {
	return 2;
    } else if ((cr4->pae == 0) && (efer->lme == 0)) {
	return 4;
    } else if (efer->lme == 0) {
	return 4;
    } else if ((efer->lme == 1) && (cs->long_mode == 1)) {
	return 8;
    } else {
	// What about LONG_16_COMPAT???
	return 4;
    }
}


static const uchar_t REAL_STR[] = "Real";
static const uchar_t PROTECTED_STR[] = "Protected";
static const uchar_t PROTECTED_PAE_STR[] = "Protected+PAE";
static const uchar_t LONG_STR[] = "Long";
static const uchar_t LONG_32_COMPAT_STR[] = "32bit Compat";
static const uchar_t LONG_16_COMPAT_STR[] = "16bit Compat";

const uchar_t * v3_cpu_mode_to_str(v3_cpu_mode_t mode) {
    switch (mode) {
	case REAL:
	    return REAL_STR;
	case PROTECTED:
	    return PROTECTED_STR;
	case PROTECTED_PAE:
	    return PROTECTED_PAE_STR;
	case LONG:
	    return LONG_STR;
	case LONG_32_COMPAT:
	    return LONG_32_COMPAT_STR;
	case LONG_16_COMPAT:
	    return LONG_16_COMPAT_STR;
	default:
	    return NULL;
    }
}

v3_mem_mode_t v3_get_vm_mem_mode(struct guest_info * info) {
    struct cr0_32 * cr0;

    if (info->shdw_pg_mode == SHADOW_PAGING) {
	cr0 = (struct cr0_32 *)&(info->shdw_pg_state.guest_cr0);
    } else if (info->shdw_pg_mode == NESTED_PAGING) {
	cr0 = (struct cr0_32 *)&(info->ctrl_regs.cr0);
    } else {
	PrintError("Invalid Paging Mode...\n");
	V3_ASSERT(0);
	return -1;
    }

    if (cr0->pg == 0) {
	return PHYSICAL_MEM;
    } else {
	return VIRTUAL_MEM;
    }
}

static const uchar_t PHYS_MEM_STR[] = "Physical Memory";
static const uchar_t VIRT_MEM_STR[] = "Virtual Memory";

const uchar_t * v3_mem_mode_to_str(v3_mem_mode_t mode) {
    switch (mode) {
	case PHYSICAL_MEM:
	    return PHYS_MEM_STR;
	case VIRTUAL_MEM:
	    return VIRT_MEM_STR;
	default:
	    return NULL;
    }
}


void v3_print_segments(struct v3_segments * segs) {
    int i = 0;
    struct v3_segment * seg_ptr;

    seg_ptr=(struct v3_segment *)segs;
  
    char *seg_names[] = {"CS", "DS" , "ES", "FS", "GS", "SS" , "LDTR", "GDTR", "IDTR", "TR", NULL};
    V3_Print("Segments\n");

    for (i = 0; seg_names[i] != NULL; i++) {

	V3_Print("\t%s: Sel=%x, base=%p, limit=%x (long_mode=%d, db=%d)\n", seg_names[i], seg_ptr[i].selector, 
		   (void *)(addr_t)seg_ptr[i].base, seg_ptr[i].limit,
		   seg_ptr[i].long_mode, seg_ptr[i].db);

    }
}

//
// We don't handle those fancy 64 bit system segments...
//
int v3_translate_segment(struct guest_info * info, uint16_t selector, struct v3_segment * seg) {
    struct v3_segment * gdt = &(info->segments.gdtr);
    addr_t gdt_addr = 0;
    uint16_t seg_offset = (selector & ~0x7);
    addr_t seg_addr = 0;
    struct gen_segment * gen_seg = NULL;
    struct seg_selector sel;

    memset(seg, 0, sizeof(struct v3_segment));

    sel.value = selector;

    if (sel.ti == 1) {
	PrintError("LDT translations not supported\n");
	return -1;
    }

    if (v3_gva_to_hva(info, gdt->base, &gdt_addr) == -1) {
	PrintError("Unable to translate GDT address\n");
	return -1;
    }

    seg_addr = gdt_addr + seg_offset;
    gen_seg = (struct gen_segment *)seg_addr;

    //translate
    seg->selector = selector;

    seg->limit = gen_seg->limit_hi;
    seg->limit <<= 16;
    seg->limit += gen_seg->limit_lo;

    seg->base = gen_seg->base_hi;
    seg->base <<= 24;
    seg->base += gen_seg->base_lo;

    if (gen_seg->granularity == 1) {
	seg->limit <<= 12;
	seg->limit |= 0xfff;
    }

    seg->type = gen_seg->type;
    seg->system = gen_seg->system;
    seg->dpl = gen_seg->dpl;
    seg->present = gen_seg->present;
    seg->avail = gen_seg->avail;
    seg->long_mode = gen_seg->long_mode;
    seg->db = gen_seg->db;
    seg->granularity = gen_seg->granularity;
    
    return 0;
}




void v3_print_ctrl_regs(struct guest_info * info) {
    struct v3_ctrl_regs * regs = &(info->ctrl_regs);
    int i = 0;
    v3_reg_t * reg_ptr;
    char * reg_names[] = {"CR0", "CR2", "CR3", "CR4", "CR8", "FLAGS", NULL};
    vmcb_saved_state_t * guest_state = GET_VMCB_SAVE_STATE_AREA(info->vmm_data);

    reg_ptr = (v3_reg_t *)regs;

    V3_Print("32 bit Ctrl Regs:\n");

    for (i = 0; reg_names[i] != NULL; i++) {
	V3_Print("\t%s=0x%p (at %p)\n", reg_names[i], (void *)(addr_t)reg_ptr[i], &(reg_ptr[i]));  
    }

    V3_Print("\tEFER=0x%p\n", (void*)(addr_t)(guest_state->efer));

}

#if 0
static int safe_gva_to_hva(struct guest_info * info, addr_t linear_addr, addr_t * host_addr) {
    /* select the proper translation based on guest mode */
    if (info->mem_mode == PHYSICAL_MEM) {
    	if (v3_gpa_to_hva(info, linear_addr, host_addr) == -1) return -1;
    } else if (info->mem_mode == VIRTUAL_MEM) {
	if (v3_gva_to_hva(info, linear_addr, host_addr) == -1) return -1;
    }
    return 0;
}

static int v3_print_disassembly(struct guest_info * info) {
    int passed_rip = 0;
    addr_t rip, rip_linear, rip_host;

    /* we don't know where the instructions preceding RIP start, so we just take
     * a guess and hope the instruction stream synced up with our disassembly
     * some time before RIP; if it has not we correct RIP at that point
     */

    /* start disassembly 64 bytes before current RIP, continue 32 bytes after */
    rip = (addr_t) info->rip - 64;
    while ((int) (rip - info->rip) < 32) {
	V3_Print("disassembly step\n");

    	/* always print RIP, even if the instructions before were bad */
    	if (!passed_rip && rip >= info->rip) {
    	    if (rip != info->rip) {
    	    	V3_Print("***** bad disassembly up to this point *****\n");
    	    	rip = info->rip;
    	    }
    	    passed_rip = 1;
    	}

    	/* look up host virtual address for this instruction */
    	rip_linear = get_addr_linear(info, rip, &(info->segments.cs));
    	if (safe_gva_to_hva(info, rip_linear, &rip_host) < 0) {
    	    rip++;
    	    continue;
    	}

    	/* print disassembled instrcution (updates rip) */
    	if (v3_disasm(info, (void *) rip_host, &rip, rip == info->rip) < 0) {
    	    rip++;
    	    continue;
    	}

    }

    return 0;
}

#endif

void v3_print_guest_state(struct guest_info * info) {
    addr_t linear_addr = 0; 

    V3_Print("RIP: %p\n", (void *)(addr_t)(info->rip));
    linear_addr = get_addr_linear(info, info->rip, &(info->segments.cs));
    V3_Print("RIP Linear: %p\n", (void *)linear_addr);

    V3_Print("NumExits: %u\n", (uint32_t)info->num_exits);

    V3_Print("IRQ STATE: started=%d, pending=%d\n", 
	     info->intr_core_state.irq_started, 
	     info->intr_core_state.irq_pending);
    V3_Print("EXCP STATE: err_code_valid=%d, err_code=%x\n", 
	     info->excp_state.excp_error_code_valid, 
	     info->excp_state.excp_error_code);


    v3_print_segments(&(info->segments));
    v3_print_ctrl_regs(info);

    if (info->shdw_pg_mode == SHADOW_PAGING) {
	V3_Print("Shadow Paging Guest Registers:\n");
	V3_Print("\tGuest CR0=%p\n", (void *)(addr_t)(info->shdw_pg_state.guest_cr0));
	V3_Print("\tGuest CR3=%p\n", (void *)(addr_t)(info->shdw_pg_state.guest_cr3));
	V3_Print("\tGuest EFER=%p\n", (void *)(addr_t)(info->shdw_pg_state.guest_efer.value));
	// CR4
    }
    v3_print_GPRs(info);

    v3_print_mem_map(info->vm_info);

    v3_print_stack(info);

    //  v3_print_disassembly(info);
}

void v3_print_guest_state_all(struct v3_vm_info * vm) {
    int i = 0;

    V3_Print("VM Core states for %s\n", vm->name);

    for (i = 0; i < 80; i++) {
	V3_Print("-");
    }

    for (i = 0; i < vm->num_cores; i++) {
	v3_print_guest_state(&vm->cores[i]);  
    }
    
    for (i = 0; i < 80; i++) {
	V3_Print("-");
    }

    V3_Print("\n");    
}

static void print_real_mode_stack(struct guest_info *info)
{
    uint16_t ss;
    uint16_t sp;
    addr_t   addr;
    addr_t   host_addr;
    int      i;


    ss = info->segments.ss.selector & 0xffff;
    sp = info->vm_regs.rsp & 0xffff;
    addr = (((uint32_t)ss)<<4) + sp;

   
    V3_Print("Real Mode Stack starting at 0x%x:0x%x (0x%p):\n",ss,sp,(void*)addr);

    if (info->mem_mode!=PHYSICAL_MEM) {
	PrintError("Cannot print real mode stack when virtual memory active\n");
	return;
    }
	
    for (i=0;i<=24;i++,sp+=2) { 
	// note that it's correct for this to wrap around
	addr = (((uint32_t)ss)<<4) + sp;
	if (v3_gpa_to_hva(info,addr,&host_addr)) { 
	    PrintError("Could not translate physical stack address 0x%p\n",(void*)addr);
	    return;
	}
	V3_Print("\t0x%.4x\n",*((uint16_t*)host_addr));
    }
}
	



void v3_print_stack(struct guest_info * info) {
    addr_t linear_addr = 0;
    addr_t host_addr = 0;
    int i = 0;
    v3_cpu_mode_t cpu_mode = v3_get_vm_cpu_mode(info);


    if (cpu_mode==REAL) { 
	print_real_mode_stack(info);
	return;
    }

    // protected mode, 32 or 64 bit

    linear_addr = get_addr_linear(info, info->vm_regs.rsp, &(info->segments.ss));
 
    V3_Print("Stack at %p:\n", (void *)linear_addr);
   
    if (info->mem_mode == PHYSICAL_MEM) {
	if (v3_gpa_to_hva(info, linear_addr, &host_addr) == -1) {
	    PrintError("Could not translate Stack address\n");
	    return;
	}
    } else if (info->mem_mode == VIRTUAL_MEM) {
	if (v3_gva_to_hva(info, linear_addr, &host_addr) == -1) {
	    PrintError("Could not translate Virtual Stack address\n");
	    return;
	}
    }
    
    V3_Print("Host Address of rsp = 0x%p\n", (void *)host_addr);
 
    // We start i at one because the current stack pointer points to an unused stack element
    for (i = 0; i <= 24; i++) {
	if (cpu_mode == LONG) {
	    V3_Print("\t%p\n", (void *)*(addr_t *)(host_addr + (i * 8)));
	} else {
	    // 32 bit stacks...
	    V3_Print("\t%.8x\n", *(uint32_t *)(host_addr + (i * 4)));
	}
    }

}    

#ifdef __V3_32BIT__

void v3_print_GPRs(struct guest_info * info) {
    struct v3_gprs * regs = &(info->vm_regs);
    int i = 0;
    v3_reg_t * reg_ptr;
    char * reg_names[] = { "RDI", "RSI", "RBP", "RSP", "RBX", "RDX", "RCX", "RAX", NULL};

    reg_ptr = (v3_reg_t *)regs;

    V3_Print("32 bit GPRs:\n");

    for (i = 0; reg_names[i] != NULL; i++) {
	V3_Print("\t%s=0x%p (at %p)\n", reg_names[i], (void *)(addr_t)reg_ptr[i], &(reg_ptr[i]));  
    }
}

#elif __V3_64BIT__

void v3_print_GPRs(struct guest_info * info) {
    struct v3_gprs * regs = &(info->vm_regs);
    int i = 0;
    v3_reg_t * reg_ptr;
    char * reg_names[] = { "RDI", "RSI", "RBP", "RSP", "RBX", "RDX", "RCX", "RAX", \
			   "R8", "R9", "R10", "R11", "R12", "R13", "R14", "R15", NULL};

    reg_ptr = (v3_reg_t *)regs;

    V3_Print("64 bit GPRs:\n");

    for (i = 0; reg_names[i] != NULL; i++) {
	V3_Print("\t%s=0x%p (at %p)\n", reg_names[i], (void *)(addr_t)reg_ptr[i], &(reg_ptr[i]));  
    }
}

#endif


#include <palacios/vmcs.h>
#include <palacios/vmcb.h>
static int info_hcall(struct guest_info * core, uint_t hcall_id, void * priv_data) {
    extern v3_cpu_arch_t v3_mach_type;
    int cpu_valid = 0;

    V3_Print("************** Guest State ************\n");
    v3_print_guest_state(core);
    
    // init SVM/VMX
#ifdef V3_CONFIG_SVM
    if ((v3_mach_type == V3_SVM_CPU) || (v3_mach_type == V3_SVM_REV3_CPU)) {
	cpu_valid = 1;
	PrintDebugVMCB((vmcb_t *)(core->vmm_data));
    }
#endif
#ifdef V3_CONFIG_VMX
    if ((v3_mach_type == V3_VMX_CPU) || (v3_mach_type == V3_VMX_EPT_CPU) || (v3_mach_type == V3_VMX_EPT_UG_CPU)) {
	cpu_valid = 1;
	v3_print_vmcs();
    }
#endif
    if (!cpu_valid) {
	PrintError("Invalid CPU Type 0x%x\n", v3_mach_type);
	return -1;
    }
    

    return 0;
}


#ifdef V3_CONFIG_SVM
#include <palacios/svm.h>
#include <palacios/svm_io.h>
#include <palacios/svm_msr.h>
#endif

#ifdef V3_CONFIG_VMX
#include <palacios/vmx.h>
#include <palacios/vmx_io.h>
#include <palacios/vmx_msr.h>
#endif


int v3_init_vm(struct v3_vm_info * vm) {
    extern v3_cpu_arch_t v3_mach_type;



#ifdef V3_CONFIG_TELEMETRY
    v3_init_telemetry(vm);
#endif

    v3_init_hypercall_map(vm);
    v3_init_io_map(vm);
    v3_init_msr_map(vm);
    v3_init_cpuid_map(vm);
    v3_init_host_events(vm);
    v3_init_intr_routers(vm);
    v3_init_ext_manager(vm);

    v3_init_barrier(vm);

    // Initialize the memory map
    if (v3_init_mem_map(vm) == -1) {
	PrintError("Could not initialize shadow map\n");
	return -1;
    }

    v3_init_mem_hooks(vm);

    if (v3_init_shdw_impl(vm) == -1) {
	PrintError("VM initialization error in shadow implementaion\n");
	return -1;
    }


    v3_init_time_vm(vm);


#ifdef V3_CONFIG_SYMBIOTIC
    v3_init_symbiotic_vm(vm);
#endif

    v3_init_dev_mgr(vm);


    // init SVM/VMX
    switch (v3_mach_type) {
#ifdef V3_CONFIG_SVM
	case V3_SVM_CPU:
	case V3_SVM_REV3_CPU:
	    v3_init_svm_io_map(vm);
	    v3_init_svm_msr_map(vm);
	    break;
#endif
#ifdef V3_CONFIG_VMX
	case V3_VMX_CPU:
	case V3_VMX_EPT_CPU:
	case V3_VMX_EPT_UG_CPU:
	    v3_init_vmx_io_map(vm);
	    v3_init_vmx_msr_map(vm);
	    break;
#endif
	default:
	    PrintError("Invalid CPU Type 0x%x\n", v3_mach_type);
	    return -1;
    }
    
    v3_register_hypercall(vm, GUEST_INFO_HCALL, info_hcall, NULL);

    V3_Print("GUEST_INFO_HCALL=%x\n", GUEST_INFO_HCALL);

    return 0;
}


int v3_free_vm_internal(struct v3_vm_info * vm) {
    extern v3_cpu_arch_t v3_mach_type;

    v3_remove_hypercall(vm, GUEST_INFO_HCALL);



#ifdef V3_CONFIG_SYMBIOTIC
    v3_deinit_symbiotic_vm(vm);
#endif

    // init SVM/VMX
    switch (v3_mach_type) {
#ifdef V3_CONFIG_SVM
	case V3_SVM_CPU:
	case V3_SVM_REV3_CPU:
	    v3_deinit_svm_io_map(vm);
	    v3_deinit_svm_msr_map(vm);
	    break;
#endif
#ifdef V3_CONFIG_VMX
	case V3_VMX_CPU:
	case V3_VMX_EPT_CPU:
	case V3_VMX_EPT_UG_CPU:
	    v3_deinit_vmx_io_map(vm);
	    v3_deinit_vmx_msr_map(vm);
	    break;
#endif
	default:
	    PrintError("Invalid CPU Type 0x%x\n", v3_mach_type);
	    return -1;
    }

    v3_deinit_dev_mgr(vm);

    v3_deinit_time_vm(vm);

    v3_deinit_mem_hooks(vm);
    v3_delete_mem_map(vm);
    v3_deinit_shdw_impl(vm);

    v3_deinit_intr_routers(vm);
    v3_deinit_host_events(vm);

    v3_deinit_barrier(vm);

    v3_deinit_cpuid_map(vm);
    v3_deinit_msr_map(vm);
    v3_deinit_io_map(vm);
    v3_deinit_hypercall_map(vm);

#ifdef V3_CONFIG_TELEMETRY
    v3_deinit_telemetry(vm);
#endif



    return 0;
}


int v3_init_core(struct guest_info * core) {
    extern v3_cpu_arch_t v3_mach_type;
    struct v3_vm_info * vm = core->vm_info;



    /*
     * Initialize the subsystem data strutures
     */
#ifdef V3_CONFIG_TELEMETRY
    v3_init_core_telemetry(core);
#endif

    if (core->shdw_pg_mode == SHADOW_PAGING) {
	v3_init_shdw_pg_state(core);
    }

    v3_init_time_core(core);
    v3_init_intr_controllers(core);
    v3_init_exception_state(core);

    v3_init_decoder(core);


#ifdef V3_CONFIG_SYMBIOTIC
    v3_init_symbiotic_core(core);
#endif

    // init SVM/VMX


    switch (v3_mach_type) {
#ifdef V3_CONFIG_SVM
	case V3_SVM_CPU:
	case V3_SVM_REV3_CPU:
	    if (v3_init_svm_vmcb(core, vm->vm_class) == -1) {
		PrintError("Error in SVM initialization\n");
		return -1;
	    }
	    break;
#endif
#ifdef V3_CONFIG_VMX
	case V3_VMX_CPU:
	case V3_VMX_EPT_CPU:
	case V3_VMX_EPT_UG_CPU:
	    if (v3_init_vmx_vmcs(core, vm->vm_class) == -1) {
		PrintError("Error in VMX initialization\n");
		return -1;
	    }
	    break;
#endif
	default:
	    PrintError("Invalid CPU Type 0x%x\n", v3_mach_type);
	    return -1;
    }

    return 0;
}



int v3_free_core(struct guest_info * core) {
    extern v3_cpu_arch_t v3_mach_type;

    
#ifdef V3_CONFIG_SYMBIOTIC
    v3_deinit_symbiotic_core(core);
#endif

    v3_deinit_decoder(core);

    v3_deinit_intr_controllers(core);
    v3_deinit_time_core(core);

    if (core->shdw_pg_mode == SHADOW_PAGING) {
	v3_deinit_shdw_pg_state(core);
    }

    v3_free_passthrough_pts(core);

#ifdef V3_CONFIG_TELEMETRY
    v3_deinit_core_telemetry(core);
#endif

    switch (v3_mach_type) {
#ifdef V3_CONFIG_SVM
	case V3_SVM_CPU:
	case V3_SVM_REV3_CPU:
	    if (v3_deinit_svm_vmcb(core) == -1) {
		PrintError("Error in SVM initialization\n");
		return -1;
	    }
	    break;
#endif
#ifdef V3_CONFIG_VMX
	case V3_VMX_CPU:
	case V3_VMX_EPT_CPU:
	case V3_VMX_EPT_UG_CPU:
	    if (v3_deinit_vmx_vmcs(core) == -1) {
		PrintError("Error in VMX initialization\n");
		return -1;
	    }
	    break;
#endif
	default:
	    PrintError("Invalid CPU Type 0x%x\n", v3_mach_type);
	    return -1;
    }

    return 0;
}



