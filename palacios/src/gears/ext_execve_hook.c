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
#include <palacios/vmm_string.h>
#include <palacios/vmm_hashtable.h>
#include <palacios/vmm_extensions.h>
#include <palacios/vmm_decoder.h>
#include <palacios/vm_guest.h>
#include <palacios/vm_guest_mem.h>

#include <gears/syscall_hijack.h>
#include <gears/execve_hook.h>
#include <gears/syscall_ref.h>

#ifdef V3_CONFIG_EXT_CODE_INJECT
#include <gears/code_inject.h>
#endif

static struct v3_exec_hooks exec_hooks;

static int free_hook (struct v3_vm_info * vm, struct exec_hook * hook) {
    list_del(&(hook->hook_node));
    V3_Free(hook);
    return 0;
}
    
static uint_t exec_hash_fn (addr_t key) {
    return v3_hash_long(key, sizeof(void *) * 8);
}


static int exec_eq_fn (addr_t key1, addr_t key2) {
    return (key1 == key2);
}


static int init_exec_hooks (struct v3_vm_info * vm, v3_cfg_tree_t * cfg, void ** priv_data) {

    return 0;
}

static int init_exec_hooks_core (struct guest_info * core, void * priv_data) {
    struct v3_exec_hooks * hooks = &exec_hooks;
    INIT_LIST_HEAD(&(hooks->hook_list));
	hooks->bin_table = v3_create_htable(0, exec_hash_fn, exec_eq_fn);

    if (hooks->bin_table == NULL) {
        PrintError("Problem creating execve hash table\n");
        return -1;
    }

    if (core->cpu_mode == LONG || core->cpu_mode == LONG_32_COMPAT) {
        PrintDebug("Hooking execve 64\n");
        v3_hook_syscall(core, SYS64_EXECVE, v3_execve_handler, NULL);
    } else {
        PrintDebug("Hooking execve, cpu mode: %x\n", core->cpu_mode);
        v3_hook_syscall(core, SYS32_EXECVE, v3_execve_handler, NULL);
    }
    return 0;
}

static int deinit_exec_hooks_core (struct guest_info * core, void * priv_data) {
    struct v3_exec_hooks * hooks = &exec_hooks;
    struct exec_hook * hook = NULL;
    struct exec_hook * tmp = NULL;
    
    list_for_each_entry_safe(hook, tmp, &(hooks->hook_list), hook_node) {
        free_hook(core->vm_info, hook);
    }

    v3_free_htable(hooks->bin_table, 0, 0);

    return 0;
}


int v3_hook_executable (struct v3_vm_info * vm, 
    const uchar_t * binfile,
    int (*handler)(struct guest_info * core, void * priv_data),
    void * priv_data) 
{
    struct exec_hook * hook = V3_Malloc(sizeof(struct exec_hook));
    struct v3_exec_hooks * hooks = &exec_hooks;
    addr_t key;
    
    memset(hook, 0, sizeof(struct exec_hook));
    
    hook->handler = handler;
    hook->priv_data = priv_data;
    
    // we hash the name of the file to produce a key
    key = v3_hash_buffer((uchar_t*)binfile, strlen(binfile));

    v3_htable_insert(hooks->bin_table, key, (addr_t)hook);
    list_add(&(hook->hook_node), &(hooks->hook_list));

    return 0;
}


int v3_unhook_executable (struct v3_vm_info * vm, const uchar_t * binfile) {
    struct exec_hook * hook;
    struct v3_exec_hooks * hooks = &exec_hooks;
    addr_t key;

    key = v3_hash_buffer((uchar_t*)binfile, strlen((uchar_t*)binfile));
    if ((hook = (struct exec_hook*)v3_htable_search(hooks->bin_table, key)) != NULL) {
        free_hook(vm, hook);
    } else {
        PrintError("Could not unhook executable '%s'\n", binfile);
        return -1;
    }

    if (v3_htable_remove(hooks->bin_table, key, 0) == (addr_t)NULL) {
        PrintError("Error trying to remove key from htable: v3_unhook_executable\n");
        return -1;
    }

    return 0;
}

static struct v3_extension_impl execve_impl = {
	.name = "execve_intercept",
	.init = init_exec_hooks,
	.deinit = NULL,
	.core_init = init_exec_hooks_core,
	.core_deinit = deinit_exec_hooks_core,
	.on_entry = NULL,
	.on_exit = NULL
};

register_extension(&execve_impl);


int v3_execve_handler (struct guest_info * core, uint_t syscall_nr, void * priv_data) {
    addr_t hva, key;
	struct v3_exec_hooks * hooks = &exec_hooks;
    struct exec_hook * hook;
    int ret;

    
    // TODO: make sure this returns immediately if we're not booted up already
    if (core->mem_mode == PHYSICAL_MEM) {
        ret = v3_gpa_to_hva(core, get_addr_linear(core, (addr_t)core->vm_regs.rbx, &(core->segments.ds)), &hva);
    } else {
        ret = v3_gva_to_hva(core, get_addr_linear(core, (addr_t)core->vm_regs.rbx, &(core->segments.ds)), &hva);      
    }

    if (ret == -1) {
        PrintError("Error translating file path in sysexecve handler\n");
        return 0;
    }

    key = v3_hash_buffer((uchar_t*)hva, strlen((uchar_t*)hva));
    if ((hook = (struct exec_hook*)v3_htable_search(hooks->bin_table, key)) != NULL) {
        
        ret = hook->handler(core, hook->priv_data);
       if (ret == -1) {
            PrintDebug("Error handling execve hook\n");
            return -1;
       }

#ifdef V3_CONFIG_EXT_CODE_INJECT
        if (ret == E_NEED_PF) {
            return E_NEED_PF;
        }
#endif
    } 

    return 0;
}

