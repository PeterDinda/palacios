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
#include <palacios/vm_guest_mem.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_intr.h>
#include <palacios/vmm_decoder.h>
#include <palacios/vmm_string.h>
#include <palacios/vmm_shadow_paging.h>
#include <palacios/vmm_extensions.h>
#include <palacios/vmm_intr.h>

#include <interfaces/syscall_hijack.h>
#include <interfaces/sw_intr.h>

#include "syscall_ref.h"

#ifndef V3_CONFIG_DEBUG_EXT_SYSCALL_HIJACK
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

#define MAX_CHARS 256
#ifndef max
    #define max(a, b) ( ((a) > (b)) ? (a) : (b) )
#endif

#define SYSCALL_INT_VECTOR 0x80


struct v3_syscall_hook {
    int (*handler)(struct guest_info * core, uint_t syscall_nr, void * priv_data);
    void * priv_data;
};

static struct v3_syscall_hook * syscall_hooks[512];


static int v3_syscall_handler (struct guest_info * core, uint8_t vector, void * priv_data) {
 
    uint_t syscall_nr = (uint_t) core->vm_regs.rax;
    int err = 0;

    struct v3_syscall_hook * hook = syscall_hooks[syscall_nr];
    if (hook == NULL) {
#ifdef V3_CONFIG_EXT_SYSCALL_PASSTHROUGH
        if (v3_hook_passthrough_syscall(core, syscall_nr) == -1) {
            PrintDebug("Error hooking passthrough syscall\n");
            return -1;
        }
        hook = syscall_hooks[syscall_nr];
#else
        return v3_raise_swintr(core, vector);
#endif
    }
    
    err = hook->handler(core, syscall_nr, hook->priv_data);
    if (err == -1) {
        PrintDebug("V3 Syscall Handler: Error in syscall hook\n");
        return -1;
    }

    return 0;
}


static int init_syscall_hijack (struct v3_vm_info * vm, v3_cfg_tree_t * cfg, void ** priv_data) {

    return 0;
}


static int init_syscall_hijack_core (struct guest_info * core, void * priv_data) {

    v3_hook_swintr(core, SYSCALL_INT_VECTOR, v3_syscall_handler, NULL);
    return 0;
}


static void print_arg (struct  guest_info * core, v3_reg_t reg, uint8_t argnum) {

    addr_t hva;
    int ret = 0;
    
    PrintDebug("\t ARG%d: INT - %ld\n", argnum, (long) reg);

    if (core->mem_mode == PHYSICAL_MEM) {
        ret = v3_gpa_to_hva(core, get_addr_linear(core, reg, &(core->segments.ds)), &hva);
    }
    else { 
        ret = v3_gva_to_hva(core, get_addr_linear(core, reg, &(core->segments.ds)), &hva);
    }

    PrintDebug("\t       STR - ");
    if (ret == -1) {
        PrintDebug("\n");
        return;
    }
        
    uint32_t c = max(MAX_CHARS, 4096 - (hva % 4096));
    int i = 0;
    for (; i < c && *((char*)(hva + i)) != 0; i++) {
        PrintDebug("%c", *((char*)(hva + i)));
    }
    PrintDebug("\n");
}


static void print_syscall (uint8_t is64, struct guest_info * core) {

    if (is64) {
        PrintDebug("Syscall #%ld: \"%s\"\n", (long)core->vm_regs.rax, get_linux_syscall_name64(core->vm_regs.rax));
    } else {
        PrintDebug("Syscall #%ld: \"%s\"\n", (long)core->vm_regs.rax, get_linux_syscall_name32(core->vm_regs.rax));
    }

    print_arg(core, core->vm_regs.rbx, 1);
    print_arg(core, core->vm_regs.rcx, 2);
    print_arg(core, core->vm_regs.rdx, 3);
}




static struct v3_extension_impl syscall_impl = {
    .name = "syscall_intercept",
    .init = init_syscall_hijack,
    .deinit = NULL,
    .core_init = init_syscall_hijack_core,
    .core_deinit = NULL,
    .on_entry = NULL,
    .on_exit = NULL
};

register_extension(&syscall_impl);




static inline struct v3_syscall_hook * get_syscall_hook (struct guest_info * core, uint_t syscall_nr) {
    return syscall_hooks[syscall_nr];
} 


int v3_hook_syscall (struct guest_info * core,
    uint_t syscall_nr,
    int (*handler)(struct guest_info * core, uint_t syscall_nr, void * priv_data),
    void * priv_data) 
{
    struct v3_syscall_hook * hook = (struct v3_syscall_hook *)V3_Malloc(sizeof(struct v3_syscall_hook));

    
    if (hook == NULL) {
        return -1;
    }

    if (get_syscall_hook(core, syscall_nr) != NULL) {
        PrintError("System Call #%d already hooked\n", syscall_nr);
        return -1;
    }

    hook->handler = handler;
    hook->priv_data = priv_data;

    syscall_hooks[syscall_nr] = hook;

    return 0;
}


static int passthrough_syscall_handler (struct guest_info * core, uint_t syscall_nr, void * priv_data) {
    print_syscall(0, core);
    return 0;
}


int v3_hook_passthrough_syscall (struct guest_info * core, uint_t syscall_nr) {
    
    int rc = v3_hook_syscall(core, syscall_nr, passthrough_syscall_handler, NULL);

    if (rc) {
        PrintError("failed to hook syscall 0x%x for passthrough (guest=0x%p)\n", syscall_nr, (void *)core);
        return -1;
    } else {
        PrintDebug("hooked syscall 0x%x for passthrough (guest=0x%p)\n", syscall_nr, (void *)core);
        return 0;
    }

    /* shouldn't get here */
    return 0;
}

/*
int v3_sysexecve_handler (struct guest_info * core, uint_t syscall_nr, void * priv_data) {
    addr_t hva, key;
    struct exec_hook * hook;
    int ret;
    
    ret = v3_gva_to_hva(core, get_addr_linear(core, (addr_t)core->vm_regs.rbx, &(core->segments.ds)), &hva);
    if (ret == -1) {
        PrintDebug("Error translating file path in sysexecve handler\n");
        return -1;
    }

    key = v3_hash_buffer((uchar_t*)hva, strlen((uchar_t*)hva));
    if ((hook = (struct exec_hook*)v3_htable_search(core->exec_hooks.bin_table, key)) != NULL) {
       if (hook->handler(core, NULL) == -1) {
            PrintDebug("Error handling execve hook\n");
            return -1;
        }
    } 
        
    return 0;
}

*/
