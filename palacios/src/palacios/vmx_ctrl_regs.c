
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

static int handle_mov_to_cr0(struct guest_info * info, v3_reg_t new_val);

int v3_vmx_handle_cr0_write(struct guest_info * info, v3_reg_t new_val) {
    return handle_mov_to_cr0(info, new_val);
}

static int handle_mov_to_cr0(struct guest_info * info, v3_reg_t new_val) {
    PrintDebug("CR0 RIP: %p\n", (void *)info->rip);

    struct cr0_32 * guest_cr0 = (struct cr0_32 *)&(info->ctrl_regs.cr0);
    struct cr0_32 * new_cr0 = (struct cr0_32 *)&new_val;
    struct cr0_32 * shadow_cr0 = (struct cr0_32 *)&(info->shdw_pg_state.guest_cr0);

    // PG and PE are always enabled for VMX

    // Check if this is a paging transition
    PrintDebug("Old CR0: 0x%x\n", *(uint32_t *)guest_cr0);
    PrintDebug("Old shadow CR0: 0x%x\n", *(uint32_t *)shadow_cr0);
    PrintDebug("New CR0: 0x%x\n", *(uint32_t *)new_cr0);
            
    if ( new_cr0->pe ) {

        if (v3_vmxassist_ctx_switch(info) != 0) {
            PrintError("Unable to execute VMXASSIST context switch!\n");
            return -1;
        }

        ((struct vmx_data *)info->vmm_data)->state = VMXASSIST_DISABLED;

        PrintDebug("New Shadow: 0x%x\n", *(uint32_t *)shadow_cr0);
        PrintDebug("mem_mode: %s\n", v3_mem_mode_to_str(v3_get_vm_mem_mode(info))); 

        return 0;
    }

    return -1;
}

