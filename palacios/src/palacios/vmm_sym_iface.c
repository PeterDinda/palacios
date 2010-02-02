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
#include <palacios/vmm_msr.h>
#include <palacios/vmm_mem.h>
#include <palacios/vmm_hypercall.h>
#include <palacios/vm_guest.h>

#define SYM_PAGE_MSR 0x535

#define SYM_CPUID_NUM 0x90000000

// A succesfull symcall returns via the RET_HCALL, with the return values in registers
// A symcall error returns via the ERR_HCALL with the error code in rbx
#define SYM_CALL_RET_HCALL 0x535
#define SYM_CALL_ERR_HCALL 0x536


/* Notes: We use a combination of SYSCALL and SYSENTER Semantics 
 * SYSCALL just sets an EIP, CS/SS seg, and GS seg via swapgs
 * the RSP is loaded via the structure pointed to by GS
 * This is safe because it assumes that system calls are guaranteed to be made with an empty kernel stack.
 * We cannot make that assumption with a symcall, so we have to have our own stack area somewhere.
 * SYSTENTER does not really use the GS base MSRs, but we do to map to 64 bit kernels
 */

#define SYMCALL_RIP_MSR 0x536
#define SYMCALL_RSP_MSR 0x537
#define SYMCALL_CS_MSR  0x538
#define SYMCALL_GS_MSR  0x539
#define SYMCALL_FS_MSR  0x540

static int msr_read(uint_t msr, struct v3_msr * dst, void * priv_data) {
    struct guest_info * info = (struct guest_info *)priv_data;
    struct v3_sym_state * state = &(info->vm_info->sym_state);

    switch (msr) {
	case SYM_PAGE_MSR:
	    dst->value = state->guest_pg_addr;
	    break;
	default:
	    return -1;
    }

    return 0;
}

static int symcall_msr_read(uint_t msr, struct v3_msr * dst, void * priv_data) {
    struct guest_info * info = (struct guest_info *)priv_data;
    struct v3_symcall_state * state = &(info->vm_info->sym_state.symcalls[info->cpu_id]);

    switch (msr) {
	case SYMCALL_RIP_MSR:
	    dst->value = state->sym_call_rip;
	    break;
	case SYMCALL_RSP_MSR:
	    dst->value = state->sym_call_rsp;
	    break;
	case SYMCALL_CS_MSR:
	    dst->value = state->sym_call_cs;
	    break;
	case SYMCALL_GS_MSR:
	    dst->value = state->sym_call_gs;
	    break;
	case SYMCALL_FS_MSR:
	    dst->value = state->sym_call_fs;
	    break;
	default:
	    return -1;
    }

    return 0;
}

static int msr_write(uint_t msr, struct v3_msr src, void * priv_data) {
    struct guest_info * info = (struct guest_info *)priv_data;
    struct v3_sym_state * state = &(info->vm_info->sym_state);

    if (msr == SYM_PAGE_MSR) {
	PrintDebug("Symbiotic MSR write for page %p\n", (void *)(addr_t)src.value);

	if (state->active == 1) {
	    // unmap page
	    struct v3_shadow_region * old_reg = v3_get_shadow_region(info->vm_info, (addr_t)state->guest_pg_addr);

	    if (old_reg == NULL) {
		PrintError("Could not find previously active symbiotic page (%p)\n", (void *)(addr_t)state->guest_pg_addr);
		return -1;
	    }

	    v3_delete_shadow_region(info->vm_info, old_reg);
	}

	state->guest_pg_addr = src.value;
	state->guest_pg_addr &= ~0xfffLL;

	state->active = 1;

	// map page
	v3_add_shadow_mem(info->vm_info, (addr_t)state->guest_pg_addr, 
			  (addr_t)(state->guest_pg_addr + PAGE_SIZE_4KB - 1), 
			  state->sym_page_pa);
    } else {
	PrintError("Invalid Symbiotic MSR write (0x%x)\n", msr);
	return -1;
    }

    return 0;
}


static int symcall_msr_write(uint_t msr, struct v3_msr src, void * priv_data) {
    struct guest_info * info = (struct guest_info *)priv_data;
    struct v3_symcall_state * state = &(info->vm_info->sym_state.symcalls[info->cpu_id]);

    switch (msr) {
	case SYMCALL_RIP_MSR:
	    state->sym_call_rip = src.value;
	    break;
	case SYMCALL_RSP_MSR:
	    state->sym_call_rsp = src.value;
	    break;
	case SYMCALL_CS_MSR:
	    state->sym_call_cs = src.value;
	    break;
	case SYMCALL_GS_MSR:
	    state->sym_call_gs = src.value;
	    break;
	case SYMCALL_FS_MSR:
	    state->sym_call_fs = src.value;
	    break;
	default:
	    PrintError("Invalid Symbiotic MSR write (0x%x)\n", msr);
	    return -1;
    }
    return 0;
}

static int cpuid_fn(struct guest_info * info, uint32_t cpuid, 
		    uint32_t * eax, uint32_t * ebx,
		    uint32_t * ecx, uint32_t * edx,
		    void * private_data) {
    extern v3_cpu_arch_t v3_cpu_types[];

    *eax = *(uint32_t *)"V3V";

    if ((v3_cpu_types[info->cpu_id] == V3_SVM_CPU) || 
	(v3_cpu_types[info->cpu_id] == V3_SVM_REV3_CPU)) {
	*ebx = *(uint32_t *)"SVM";
    } else if ((v3_cpu_types[info->cpu_id] == V3_VMX_CPU) || 
	       (v3_cpu_types[info->cpu_id] == V3_VMX_EPT_CPU)) {
	*ebx = *(uint32_t *)"VMX";
    }


    return 0;
}
    

static int sym_call_ret(struct guest_info * info, uint_t hcall_id, void * private_data);
static int sym_call_err(struct guest_info * info, uint_t hcall_id, void * private_data);



int v3_init_sym_iface(struct v3_vm_info * vm) {
    struct v3_sym_state * state = &(vm->sym_state);
    memset(state, 0, sizeof(struct v3_sym_state));

    state->sym_page_pa = (addr_t)V3_AllocPages(1);
    state->sym_page = (struct v3_sym_interface *)V3_VAddr((void *)state->sym_page_pa);
    memset(state->sym_page, 0, PAGE_SIZE_4KB);

    
    memcpy(&(state->sym_page->magic), "V3V", 3);

    v3_hook_msr(vm, SYM_PAGE_MSR, msr_read, msr_write, info);

    v3_hook_cpuid(vm, SYM_CPUID_NUM, cpuid_fn, info);

    v3_hook_msr(vm, SYMCALL_RIP_MSR, symcall_msr_read, msr_write, info);
    v3_hook_msr(vm, SYMCALL_RSP_MSR, symcall_msr_read, msr_write, info);
    v3_hook_msr(vm, SYMCALL_CS_MSR, symcall_msr_read, msr_write, info);
    v3_hook_msr(vm, SYMCALL_GS_MSR, symcall_msr_read, msr_write, info);
    v3_hook_msr(vm, SYMCALL_FS_MSR, symcall_msr_read, msr_write, info);

    v3_register_hypercall(vm, SYM_CALL_RET_HCALL, sym_call_ret, NULL);
    v3_register_hypercall(vm, SYM_CALL_ERR_HCALL, sym_call_err, NULL);

    return 0;
}

int v3_sym_map_pci_passthrough(struct v3_vm_info * vm, uint_t bus, uint_t dev, uint_t fn) {
    struct v3_sym_state * state = &(vm->sym_state);
    uint_t dev_index = (bus << 8) + (dev << 3) + fn;
    uint_t major = dev_index / 8;
    uint_t minor = dev_index % 8;

    if (bus > 3) {
	PrintError("Invalid PCI bus %d\n", bus);
	return -1;
    }

    PrintDebug("Setting passthrough pci map for index=%d\n", dev_index);

    state->sym_page->pci_pt_map[major] |= 0x1 << minor;

    PrintDebug("pt_map entry=%x\n",   state->sym_page->pci_pt_map[major]);

    PrintDebug("pt map vmm addr=%p\n", state->sym_page->pci_pt_map);

    return 0;
}

int v3_sym_unmap_pci_passthrough(struct v3_vm_info * vm, uint_t bus, uint_t dev, uint_t fn) {
    struct v3_sym_state * state = &(vm->sym_state);
    uint_t dev_index = (bus << 8) + (dev << 3) + fn;
    uint_t major = dev_index / 8;
    uint_t minor = dev_index % 8;

    if (bus > 3) {
	PrintError("Invalid PCI bus %d\n", bus);
	return -1;
    }

    state->sym_page->pci_pt_map[major] &= ~(0x1 << minor);

    return 0;
}


static int sym_call_err(struct guest_info * info, uint_t hcall_id, void * private_data) {
    struct v3_symcall_state * state = (struct v3_symcall_state *)&(info->sym_state.symcalls[info->cpu_id]);

    PrintError("sym call error\n");

    state->sym_call_errno = (int)info->vm_regs.rbx;
    v3_print_guest_state(info);
    v3_print_mem_map(info);

    // clear sym flags
    state->sym_call_error = 1;
    state->sym_call_returned = 1;

    return -1;
}

static int sym_call_ret(struct guest_info * info, uint_t hcall_id, void * private_data) {
    struct v3_symcall_state * state = (struct v3_symcall_state *)&(info->vm_info->sym_state.symcalls[info->cpu_id]);

    //    PrintError("Return from sym call (ID=%x)\n", hcall_id);
    //   v3_print_guest_state(info);

    state->sym_call_returned = 1;

    return 0;
}

static int execute_symcall(struct guest_info * info) {
    struct v3_symcall_state * state = (struct v3_symcall_state *)&(info->vm_info->sym_state.symcalls[info->cpu_id]);

    while (state->sym_call_returned == 0) {
	if (v3_vm_enter(info) == -1) {
	    PrintError("Error in Sym call\n");
	    return -1;
	}
    }

    return 0;
}


int v3_sym_call(struct guest_info * info, 
		uint64_t call_num, sym_arg_t * arg0, 
		sym_arg_t * arg1, sym_arg_t * arg2,
		sym_arg_t * arg3, sym_arg_t * arg4) {
    struct v3_sym_state * sym_state = (struct v3_sym_state *)&(info->vm_info->sym_sate);
    struct v3_symcall_state * state = (struct v3_symcall_state *)&(sym_state->symcalls[info->cpu_id]);
    struct v3_sym_context * old_ctx = (struct v3_sym_context *)&(state->old_ctx);
    struct v3_segment sym_cs;
    struct v3_segment sym_ss;
    uint64_t trash_args[5] = { [0 ... 4] = 0 };

    //   PrintDebug("Making Sym call\n");
    //    v3_print_guest_state(info);

    if ((sym_state->sym_page->sym_call_enabled == 0) ||
	(state->sym_call_active == 1)) {
	return -1;
    }
    
    if (!arg0) arg0 = &trash_args[0];
    if (!arg1) arg1 = &trash_args[1];
    if (!arg2) arg2 = &trash_args[2];
    if (!arg3) arg3 = &trash_args[3];
    if (!arg4) arg4 = &trash_args[4];

    // Save the old context
    memcpy(&(old_ctx->vm_regs), &(info->vm_regs), sizeof(struct v3_gprs));
    memcpy(&(old_ctx->cs), &(info->segments.cs), sizeof(struct v3_segment));
    memcpy(&(old_ctx->ss), &(info->segments.ss), sizeof(struct v3_segment));
    old_ctx->gs_base = info->segments.gs.base;
    old_ctx->fs_base = info->segments.fs.base;
    old_ctx->rip = info->rip;
    old_ctx->cpl = info->cpl;
    old_ctx->flags = info->ctrl_regs.rflags;

    // Setup the sym call context
    info->rip = state->sym_call_rip;
    info->vm_regs.rsp = state->sym_call_rsp; // old contest rsp is saved in vm_regs

    v3_translate_segment(info, state->sym_call_cs, &sym_cs);
    memcpy(&(info->segments.cs), &sym_cs, sizeof(struct v3_segment));
 
    v3_translate_segment(info, state->sym_call_cs + 8, &sym_ss);
    memcpy(&(info->segments.ss), &sym_ss, sizeof(struct v3_segment));

    info->segments.gs.base = state->sym_call_gs;
    info->segments.fs.base = state->sym_call_fs;
    info->cpl = 0;

    info->vm_regs.rax = call_num;
    info->vm_regs.rbx = *arg0;
    info->vm_regs.rcx = *arg1;
    info->vm_regs.rdx = *arg2;
    info->vm_regs.rsi = *arg3;
    info->vm_regs.rdi = *arg4;

    // Mark sym call as active
    state->sym_call_active = 1;
    state->sym_call_returned = 0;

    //    PrintDebug("Sym state\n");
    //  v3_print_guest_state(info);

    // Do the sym call entry
    if (execute_symcall(info) == -1) {
	PrintError("SYMCALL error\n");
	return -1;
    }

    // clear sym flags
    state->sym_call_active = 0;

    *arg0 = info->vm_regs.rbx;
    *arg1 = info->vm_regs.rcx;
    *arg2 = info->vm_regs.rdx;
    *arg3 = info->vm_regs.rsi;
    *arg4 = info->vm_regs.rdi;

    // restore guest state
    memcpy(&(info->vm_regs), &(old_ctx->vm_regs), sizeof(struct v3_gprs));
    memcpy(&(info->segments.cs), &(old_ctx->cs), sizeof(struct v3_segment));
    memcpy(&(info->segments.ss), &(old_ctx->ss), sizeof(struct v3_segment));
    info->segments.gs.base = old_ctx->gs_base;
    info->segments.fs.base = old_ctx->fs_base;
    info->rip = old_ctx->rip;
    info->cpl = old_ctx->cpl;
    info->ctrl_regs.rflags = old_ctx->flags;



    //    PrintError("restoring guest state\n");
    //    v3_print_guest_state(info);

    return 0;
}


