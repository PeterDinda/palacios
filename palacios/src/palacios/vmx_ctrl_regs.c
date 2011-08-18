
/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Andy Gocke <agocke@gmail.com>
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Andy Gocke <agocke@gmail.com>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmx_ctrl_regs.h>
#include <palacios/vmm.h>
#include <palacios/vmx_lowlevel.h>
#include <palacios/vmx.h>
#include <palacios/vmx_assist.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_direct_paging.h>
#include <palacios/vmm_ctrl_regs.h>

#ifndef V3_CONFIG_DEBUG_VMX
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

static v3_reg_t * get_reg_ptr(struct guest_info * info, struct vmx_exit_cr_qual * cr_qual);
static int handle_mov_to_cr0(struct guest_info * info, v3_reg_t * new_val, struct vmx_exit_info * exit_info);
static int handle_mov_to_cr3(struct guest_info * info, v3_reg_t * cr3_reg);
static int handle_mov_from_cr3(struct guest_info * info, v3_reg_t * cr3_reg);

int v3_vmx_handle_cr0_access(struct guest_info * info, struct vmx_exit_cr_qual * cr_qual, struct vmx_exit_info * exit_info) {

    if (cr_qual->access_type < 2) {
        v3_reg_t * reg = get_reg_ptr(info, cr_qual);
        
        if (cr_qual->access_type == 0) {

            if (handle_mov_to_cr0(info, reg, exit_info) != 0) {
                PrintError("Could not handle CR0 write\n");
                return -1;
            }
        } else {
            // Mov from cr
	    PrintError("Mov From CR0 not handled\n");
	    return -1;
        }

        return 0;
    }

    PrintError("Invalid CR0 Access type?? (type=%d)\n", cr_qual->access_type);
    return -1;
}

int v3_vmx_handle_cr3_access(struct guest_info * info, struct vmx_exit_cr_qual * cr_qual) {

    if (cr_qual->access_type < 2) {
        v3_reg_t * reg = get_reg_ptr(info, cr_qual);

        if (cr_qual->access_type == 0) {
            return handle_mov_to_cr3(info, reg);
        } else {
            return handle_mov_from_cr3(info, reg);
        }
    }

    PrintError("Invalid CR3 Access type?? (type=%d)\n", cr_qual->access_type);
    return -1;
}

int v3_vmx_handle_cr4_access(struct guest_info * info, struct vmx_exit_cr_qual * cr_qual) {
    if (cr_qual->access_type < 2) {

	if (cr_qual->access_type == 0) {
	    if (v3_handle_cr4_write(info) != 0) {
		PrintError("Could not handle CR4 write\n");
		return -1;
	    }
	    info->ctrl_regs.cr4 |= 0x2000; // no VMX allowed in guest, so mask CR4.VMXE
	} else {
	    if (v3_handle_cr4_read(info) != 0) {
		PrintError("Could not handle CR4 read\n");
		return -1;
	    }
	}

	return 0;
    }

    PrintError("Invalid CR4 Access type?? (type=%d)\n", cr_qual->access_type);
    return -1;
}

static int handle_mov_to_cr3(struct guest_info * info, v3_reg_t * cr3_reg) {

    if (info->shdw_pg_mode == SHADOW_PAGING) {

	/*
        PrintDebug("Old Guest CR3=%p, Old Shadow CR3=%p\n",
		   (void *)info->ctrl_regs.cr3,
		   (void *)info->shdw_pg_state.guest_cr3);
	*/

        if (info->cpu_mode == LONG) {
            info->shdw_pg_state.guest_cr3 = (uint64_t)*cr3_reg;
        } else {
            info->shdw_pg_state.guest_cr3 = (uint32_t)*cr3_reg;
        }


        if (v3_get_vm_mem_mode(info) == VIRTUAL_MEM) {
            if (v3_activate_shadow_pt(info) == -1) {
                PrintError("Failed to activate 32 bit shadow page table\n");
                return -1;
            }
        }
	/*
        PrintDebug("New guest CR3=%p, New shadow CR3=%p\n",
		   (void *)info->ctrl_regs.cr3,
		   (void *)info->shdw_pg_state.guest_cr3);
	*/
    } else if (info->shdw_pg_mode == NESTED_PAGING) {
        PrintError("Nested paging not available in VMX right now!\n");
        return -1;
    }



    return 0;
}

static int handle_mov_from_cr3(struct guest_info * info, v3_reg_t * cr3_reg) {


    if (info->shdw_pg_mode == SHADOW_PAGING) {

        if ((v3_get_vm_cpu_mode(info) == LONG) ||
	    (v3_get_vm_cpu_mode(info) == LONG_32_COMPAT)) {

            *cr3_reg = (uint64_t)info->shdw_pg_state.guest_cr3;
        } else {
            *cr3_reg = (uint32_t)info->shdw_pg_state.guest_cr3;
        }

    } else {
        PrintError("Unhandled paging mode\n");
        return -1;
    }


    return 0;
}

static int handle_mov_to_cr0(struct guest_info * info, v3_reg_t * new_cr0, struct vmx_exit_info * exit_info) {
    struct cr0_32 * guest_cr0 = (struct cr0_32 *)&(info->ctrl_regs.cr0);
    struct cr0_32 * shdw_cr0 = (struct cr0_32 *)&(info->shdw_pg_state.guest_cr0);
    struct cr0_32 * new_shdw_cr0 = (struct cr0_32 *)new_cr0;
    struct vmx_data * vmx_info = (struct vmx_data *)info->vmm_data;
    uint_t paging_transition = 0;

    /*
      PrintDebug("Old shadow CR0: 0x%x, New shadow CR0: 0x%x\n",
      (uint32_t)info->shdw_pg_state.guest_cr0, (uint32_t)*new_cr0);
    */

    if (new_shdw_cr0->pe != shdw_cr0->pe) {
	/*
	  PrintDebug("Guest CR0: 0x%x\n", *(uint32_t *)guest_cr0);
	  PrintDebug("Old shadow CR0: 0x%x\n", *(uint32_t *)shdw_cr0);
	  PrintDebug("New shadow CR0: 0x%x\n", *(uint32_t *)new_shdw_cr0);
	*/

        if (v3_vmxassist_ctx_switch(info) != 0) {
            PrintError("Unable to execute VMXASSIST context switch!\n");
            return -1;
        }
	
        if (vmx_info->assist_state == VMXASSIST_ENABLED) {
            PrintDebug("Loading VMXASSIST at RIP: %p\n", (void *)(addr_t)info->rip);
        } else {
            PrintDebug("Leaving VMXASSIST and entering protected mode at RIP: %p\n",
		       (void *)(addr_t)info->rip);
        }

	// PE switches modify the RIP directly, so we clear the instr_len field to avoid catastrophe
	exit_info->instr_len = 0;

	//	v3_vmx_restore_vmcs(info);
	//      v3_print_vmcs(info);

    } else {

	if (new_shdw_cr0->pg != shdw_cr0->pg) {
	    paging_transition = 1;
	}
	
	// The shadow always reflects the new value
	*shdw_cr0 = *new_shdw_cr0;
	
	// We don't care about most of the flags, so lets go for it 
	// and set them to the guest values
	*guest_cr0 = *shdw_cr0;
	
	// Except PG, PE, and NE, which are always set
	guest_cr0->pe = 1;
	guest_cr0->pg = 1;
	guest_cr0->ne = 1;
	
	if ((paging_transition)) {
	    // Paging transition
	    
	    if (v3_get_vm_mem_mode(info) == VIRTUAL_MEM) {
		struct efer_64 * vm_efer = (struct efer_64 *)&(info->shdw_pg_state.guest_efer);
		struct efer_64 * hw_efer = (struct efer_64 *)&(info->ctrl_regs.efer);
		
		if (vm_efer->lme) {
		    //     PrintDebug("Enabling long mode\n");
		    
		    hw_efer->lma = 1;
		    hw_efer->lme = 1;
		    
		    vmx_info->entry_ctrls.guest_ia32e = 1;
		}
		
		//            PrintDebug("Activating Shadow Page tables\n");
		
		if (info->shdw_pg_mode == SHADOW_PAGING) {
		    if (v3_activate_shadow_pt(info) == -1) {
			PrintError("Failed to activate shadow page tables\n");
			return -1;
		    }
		}
		
	    } else {

		if (info->shdw_pg_mode == SHADOW_PAGING) {
		    if (v3_activate_passthrough_pt(info) == -1) {
			PrintError("Failed to activate passthrough page tables\n");
			return -1;
		    }
		} else {
		    // This is hideous... Let's hope that the 1to1 page table has not been nuked...
		    info->ctrl_regs.cr3 = VMXASSIST_1to1_PT;
		}
	    }
	}
    }

    return 0;
}

static v3_reg_t * get_reg_ptr(struct guest_info * info, struct vmx_exit_cr_qual * cr_qual) {
    v3_reg_t * reg = NULL;

    switch (cr_qual->gpr) {
	case 0:
	    reg = &(info->vm_regs.rax);
	    break;
	case 1:
	    reg = &(info->vm_regs.rcx);
	    break;
	case 2:
	    reg = &(info->vm_regs.rdx);
	    break;
	case 3:
	    reg = &(info->vm_regs.rbx);
	    break;
	case 4:
	    reg = &(info->vm_regs.rsp);
	    break;
	case 5:
	    reg = &(info->vm_regs.rbp);
	    break;
	case 6:
	    reg = &(info->vm_regs.rsi);
	    break;
	case 7:
	    reg = &(info->vm_regs.rdi);
	    break;
	case 8:
	    reg = &(info->vm_regs.r8);
	    break;
	case 9:
	    reg = &(info->vm_regs.r9);
	    break;
	case 10:
	    reg = &(info->vm_regs.r10);
	    break;
	case 11:
	    reg = &(info->vm_regs.r11);
	    break;
	case 12:
	    reg = &(info->vm_regs.r11);
	    break;
	case 13:
	    reg = &(info->vm_regs.r13);
	    break;
	case 14:
	    reg = &(info->vm_regs.r14);
	    break;
	case 15:
	    reg = &(info->vm_regs.r15);
	    break;
    }

    return reg;
}


