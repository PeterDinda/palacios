/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2012, Kyle C. Hale <kh@u.norhtwestern.edu>
 * Copyright (c) 2012, The V3VEE Project <http://www.v3vee.org> 
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

#include <gears/privilege.h>

#ifndef V3_CONFIG_DEBUG_EXT_PRIV
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

static struct v3_privileges * global_privs;

static uint_t 
priv_hash_fn (addr_t key) 
{
    return v3_hash_buffer((uint8_t*)key, strlen((char*)key));
}


static int 
priv_eq_fn (addr_t key1, addr_t key2) 
{
    return (strcmp((char*)key1, (char*)key2) == 0);
}


static int 
init_priv (struct v3_vm_info * vm, v3_cfg_tree_t * cfg, void ** priv_data) 
{
    struct v3_privileges * p = V3_Malloc(sizeof(struct v3_privileges));

    global_privs = p;

    if (!p) {
        PrintError(VM_NONE, VCORE_NONE,"Priv: Problem allocating privileges\n");
        return -1;
    }

    INIT_LIST_HEAD(&(p->priv_list));

    *priv_data = p;

    if (!(p->priv_table = v3_create_htable(0, priv_hash_fn, priv_eq_fn))) {
        PrintError(VM_NONE, VCORE_NONE,"Priv: Problem creating privilege hash table\n");
        return -1;
    }

    return 0;
}


static int 
deinit_priv (struct v3_vm_info * vm, void * priv_data) 
{
    struct v3_privileges * privs = (struct v3_privileges *)priv_data;
    struct v3_priv * p = NULL;
    struct v3_priv * tmp = NULL;

    list_for_each_entry_safe(p, tmp, &(privs->priv_list), priv_node) {
        list_del(&(p->priv_node));
        V3_Free(p);
    }

    v3_free_htable(privs->priv_table, 0, 0);

    return 0;
}


struct v3_priv * 
v3_lookup_priv (struct v3_vm_info * vm, const char * name) 
{
    struct v3_privileges * p = global_privs;
    return ((struct v3_priv*)v3_htable_search(p->priv_table, (addr_t)name));
}


static int 
deinit_all_privs (struct guest_info * core, 
                     void * priv_data,
                     void * core_data) 
{
    struct v3_privileges * p = global_privs;
    struct v3_priv * priv = NULL;
    struct v3_priv * tmp = NULL;

    list_for_each_entry_safe(priv, tmp, &p->priv_list, priv_node) {
        if (priv->deinit(core, priv->private_data) < 0) {
            PrintError(VM_NONE, VCORE_NONE,"Problem deiniting privilege on core (%s)\n", priv->name);
        }
    }

    return 0;
}


int 
v3_core_raise_all_privs (struct guest_info * core)
{
    struct v3_privileges * p = global_privs;
    struct v3_priv * priv = NULL;
    struct v3_priv * tmp = NULL;

    list_for_each_entry_safe(priv, tmp, &p->priv_list, priv_node) {
    
        if (priv->raise(core, priv->private_data) < 0) {
            PrintError(VM_NONE, VCORE_NONE,"Priv: Problem lowering privilege on core (%s)\n", priv->name);
            return -1;
        }

    }

    return 0;
}


int 
v3_core_lower_all_privs (struct guest_info * core) 
{
    struct v3_privileges * p = global_privs;
    struct v3_priv * priv = NULL;
    struct v3_priv * tmp = NULL;

    list_for_each_entry_safe(priv, tmp, &p->priv_list, priv_node) {
        
        if (priv->lower(core, priv->private_data) < 0) {
            PrintError(VM_NONE, VCORE_NONE,"Priv: Problem lowering privilege on core (%s)\n", priv->name);
            return -1;
        }

    }

    return 0;
}


int 
v3_bind_privilege (struct guest_info * core,
                   const char * priv_name,
                   int (*init)   (struct guest_info * core, void ** private_data),
                   int (*lower)  (struct guest_info * core, void * private_data),
                   int (*raise)  (struct guest_info * core, void * private_data),
                   int (*deinit) (struct guest_info * core, void * private_data),
                   void * priv_data)
{
    struct v3_privileges * p = global_privs;
    struct v3_priv * priv = v3_lookup_priv(core->vm_info, priv_name);
    if (priv) {
        PrintError(VM_NONE, VCORE_NONE,"Priv: Privilege (%s) already exists\n", priv_name);
        return -1;
    }

    priv = (struct v3_priv *)V3_Malloc(sizeof(struct v3_priv));
    
    if (!priv) {
        PrintError(VM_NONE, VCORE_NONE,"Priv: Problem allocating privilege\n");
        return -1;
    }

    PrintDebug(VM_NONE, VCORE_NONE,"Priv: Binding privilege (%s)\n", priv_name);

    priv->init         = init;
    priv->lower        = lower;
    priv->raise        = raise;
    priv->deinit       = deinit;
    priv->private_data = priv_data;

    if (v3_htable_insert(p->priv_table, (addr_t)priv_name, (addr_t)priv) == 0) {
        PrintError(VM_NONE, VCORE_NONE,"Priv: Could not register privilege (%s)\n", priv_name);
        return -1;
    }

    if (priv->init(core, &priv->private_data) < 0) {
        PrintError(VM_NONE, VCORE_NONE,"Priv: Could not initialize privilege (%s)\n", priv_name);
        return -1;
    }

    V3_Print(VM_NONE, VCORE_NONE, "Privilege (%s) bound and initialized successfully\n", priv_name);

    return 0;
}
                            


static struct v3_extension_impl priv_impl = {
	.name = "privilege",
	.vm_init = init_priv,
	.vm_deinit = deinit_priv,
	.core_init = NULL,
	.core_deinit = deinit_all_privs,
	.on_entry = NULL,
	.on_exit = NULL
};

register_extension(&priv_impl);
