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
#include <palacios/vm_guest.h>
#include <palacios/vmm_string.h>
#include <palacios/vmm_syscall_hijack.h>
#include <palacios/vmm_hashtable.h>
#include <palacios/vmm_execve_hook.h>



static int free_hook (struct guest_info * core, struct exec_hook * hook) {
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


int v3_init_exec_hooks (struct guest_info * core) {
    struct v3_exec_hooks * hooks = &(core->exec_hooks);

    INIT_LIST_HEAD(&(hooks->hook_list));

    hooks->bin_table = v3_create_htable(0, exec_hash_fn, exec_eq_fn);
    return 0;
}


int v3_deinit_exec_hooks (struct guest_info * core) {
    struct v3_exec_hooks * hooks = &(core->exec_hooks);
    struct exec_hook * hook = NULL;
    struct exec_hook * tmp = NULL;
    
    list_for_each_entry_safe(hook, tmp, &(hooks->hook_list), hook_node) {
        free_hook(core, hook);
    }

    v3_free_htable(hooks->bin_table, 0, 0);

    return 0;
}


int v3_hook_executable (struct guest_info * core, 
    const uchar_t * binfile,
    int (*handler)(struct guest_info * core, void * priv_data),
    void * priv_data) 
{
    struct exec_hook * hook = V3_Malloc(sizeof(struct exec_hook));
    struct v3_exec_hooks * hooks = &(core->exec_hooks);
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


