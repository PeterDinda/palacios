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
#include <palacios/vmcb.h>


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


void v3_print_segments(struct guest_info * info) {
    struct v3_segments * segs = &(info->segments);
    int i = 0;
    struct v3_segment * seg_ptr;

    seg_ptr=(struct v3_segment *)segs;
  
    char *seg_names[] = {"CS", "DS" , "ES", "FS", "GS", "SS" , "LDTR", "GDTR", "IDTR", "TR", NULL};
    PrintDebug("Segments\n");

    for (i = 0; seg_names[i] != NULL; i++) {

	PrintDebug("\t%s: Sel=%x, base=%p, limit=%x (long_mode=%d, db=%d)\n", seg_names[i], seg_ptr[i].selector, 
		   (void *)(addr_t)seg_ptr[i].base, seg_ptr[i].limit,
		   seg_ptr[i].long_mode, seg_ptr[i].db);

    }

}


void v3_print_ctrl_regs(struct guest_info * info) {
    struct v3_ctrl_regs * regs = &(info->ctrl_regs);
    int i = 0;
    v3_reg_t * reg_ptr;
    char * reg_names[] = {"CR0", "CR2", "CR3", "CR4", "CR8", "FLAGS", NULL};
    vmcb_saved_state_t * guest_state = GET_VMCB_SAVE_STATE_AREA(info->vmm_data);

    reg_ptr= (v3_reg_t *)regs;

    PrintDebug("32 bit Ctrl Regs:\n");

    for (i = 0; reg_names[i] != NULL; i++) {
	PrintDebug("\t%s=0x%p\n", reg_names[i], (void *)(addr_t)reg_ptr[i]);  
    }

    PrintDebug("\tEFER=0x%p\n", (void*)(addr_t)(guest_state->efer));

}


#ifdef __V3_32BIT__
void v3_print_GPRs(struct guest_info * info) {
    struct v3_gprs * regs = &(info->vm_regs);
    int i = 0;
    v3_reg_t * reg_ptr;
    char * reg_names[] = { "RDI", "RSI", "RBP", "RSP", "RBX", "RDX", "RCX", "RAX", NULL};

    reg_ptr= (v3_reg_t *)regs;

    PrintDebug("32 bit GPRs:\n");

    for (i = 0; reg_names[i] != NULL; i++) {
	PrintDebug("\t%s=0x%p\n", reg_names[i], (void *)(addr_t)reg_ptr[i]);  
    }
}
#elif __V3_64BIT__
void v3_print_GPRs(struct guest_info * info) {
    struct v3_gprs * regs = &(info->vm_regs);
    int i = 0;
    v3_reg_t * reg_ptr;
    char * reg_names[] = { "RDI", "RSI", "RBP", "RSP", "RBX", "RDX", "RCX", "RAX", \
			   "R8", "R9", "R10", "R11", "R12", "R13", "R14", "R15", NULL};

    reg_ptr= (v3_reg_t *)regs;

    PrintDebug("64 bit GPRs:\n");

    for (i = 0; reg_names[i] != NULL; i++) {
	PrintDebug("\t%s=0x%p\n", reg_names[i], (void *)(addr_t)reg_ptr[i]);  
    }
}



#endif
