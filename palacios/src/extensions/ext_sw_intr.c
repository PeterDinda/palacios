/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2011, Kyle C. Hale <kh@u.norhtwestern.edu>
 * Copyright (c) 2011, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>
#include <palacios/vmcb.h>
#include <palacios/vm_guest.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_decoder.h>
#include <palacios/vmm_extensions.h>
#include <palacios/vmm_intr.h>

#include <interfaces/sw_intr.h>

#ifndef V3_CONFIG_DEBUG_EXT_SW_INTERRUPTS
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


static int init_swintr_intercept (struct v3_vm_info * vm, v3_cfg_tree_t * cfg, void ** priv_data) {

    return 0;
}


static int init_swintr_intercept_core (struct guest_info * core, void * priv_data) {
    vmcb_t * vmcb = (vmcb_t*)core->vmm_data;
    vmcb_ctrl_t * ctrl_area = GET_VMCB_CTRL_AREA(vmcb);

    ctrl_area->instrs.INTn = 1;

    return 0;
}


struct v3_swintr_hook {
    int (*handler)(struct guest_info * core, uint8_t vector, void * priv_data);
    void * priv_data;
};


static struct v3_swintr_hook * swintr_hooks[256];


static inline struct v3_swintr_hook * get_swintr_hook (struct guest_info * core, uint8_t vector) {
    return swintr_hooks[vector];
}


static struct v3_extension_impl swintr_impl = {
    .name = "swintr_intercept",
    .init = init_swintr_intercept,
    .deinit = NULL,
    .core_init = init_swintr_intercept_core,
    .core_deinit = NULL,
    .on_entry = NULL,
    .on_exit = NULL
};


register_extension(&swintr_impl);


int v3_handle_swintr (struct guest_info * core) {

    int ret = 0;
    void * instr_ptr = NULL;
    struct x86_instr instr;

    if (core->mem_mode == PHYSICAL_MEM) { 
        ret = v3_gpa_to_hva(core, get_addr_linear(core, core->rip, &(core->segments.cs)), (addr_t *)&instr_ptr);
    } else { 
        ret = v3_gva_to_hva(core, get_addr_linear(core, core->rip, &(core->segments.cs)), (addr_t *)&instr_ptr);
    }   
    
    if (ret == -1) {
        PrintError("V3 SWintr Handler: Could not translate Instruction Address (%p)\n", (void *)core->rip);
        return -1; 
    }   

    if (v3_decode(core, (addr_t)instr_ptr, &instr) == -1) {
        PrintError("V3 SWintr Handler: Decoding Error\n");
        return -1; 
    }   

    uint8_t vector = instr.dst_operand.operand;

    struct v3_swintr_hook * hook = swintr_hooks[vector];
    if (hook == NULL) {
#ifdef V3_CONFIG_EXT_SWINTR_PASSTHROUGH
        if (v3_hook_passthrough_swintr(core, vector) == -1) {
            PrintDebug("V3 SWintr Handler: Error hooking passthrough swintr\n");
            return -1; 
        }
        hook = swintr_hooks[vector];
#else
        core->rip += instr.instr_length;
        return v3_raise_swintr(core, vector);
#endif
    }   

    ret = hook->handler(core, vector, NULL);
    if (ret == -1) {
        PrintDebug("V3 SWintr Handler: Error in swintr hook\n");
        return -1; 
    }   

    /* at some point we _may_ need to prioritize swints 
       so that they finish in time for the next
       instruction... */
    core->rip += instr.instr_length;
    return v3_raise_swintr(core, vector);
}



int v3_hook_swintr (struct guest_info * core,
        uint8_t vector,
        int (*handler)(struct guest_info * core, uint8_t vector, void * priv_data),
        void * priv_data)
{
    struct v3_swintr_hook * hook = (struct v3_swintr_hook*)V3_Malloc(sizeof(struct v3_swintr_hook));

    if (hook == NULL) {
        return -1;
    }

    if (get_swintr_hook(core, vector) != NULL) {
        PrintError("swint %d already hooked\n", vector);
        return -1;
    }
    
    hook->handler = handler;
    hook->priv_data = priv_data;

    swintr_hooks[vector] = hook;

    return 0;
}
    

static int passthrough_swintr_handler (struct guest_info * core, uint8_t vector, void * priv_data) {
    PrintDebug("[passthrough_swint_handler] INT vector=%d (guest=0x%p)\n",
        vector, (void*)core);
    return 0;
}


int v3_hook_passthrough_swintr (struct guest_info * core, uint8_t vector) {
    
    int rc = v3_hook_swintr(core, vector, passthrough_swintr_handler, NULL);
    
    if (rc) {
        PrintError("guest_swintr_injection: failed to hook swint 0x%x (guest=0x%p)\n", vector, (void*)core);
        return -1;
    } else {
        PrintDebug("guest_swintr_injection: hooked swint 0x%x (guest=0x%p)\n", vector, (void*)core);
        return 0;
    }
    
    /* shouldn't get here */
    return 0;
}


