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
#include <palacios/vm_guest.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_extensions.h>
#include <palacios/vmm_decoder.h>
#include <palacios/vmm_types.h>
#include <palacios/vmm_hypercall.h>
#include <palacios/vmm_hashtable.h>
#include <palacios/vmm_util.h>
#include <palacios/vmm_mem_hook.h>
#include <palacios/vmcb.h>
#include <gears/privilege.h>
#include <gears/guard_mods.h>

#ifndef V3_CONFIG_DEBUG_EXT_GUARD_MODS
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


static struct v3_guarded_mods global_mods;

static struct v3_gm * current_gm;

/* generates an id for a module using
 * the TSC and a hash */
static inline void 
gen_mod_id (ullong_t * id) 
{
    ullong_t val, ret;
    rdtscll(val);
    ret = (ullong_t)v3_hash_long((ulong_t)val, sizeof(ullong_t)*8);
    PrintDebug(VM_NONE, VCORE_NONE,"GM: Generating new module ID: 0x%llx\n", ret);
    *id = ret;
}


static inline void 
free_mod (struct v3_gm * mod) 
{
    struct v3_priv * p = NULL;
    struct v3_priv * tmp = NULL;
    struct v3_entry_point * er = NULL;
    struct v3_entry_point * etmp = NULL;
    int i;

    /* free the privilege list */
    list_for_each_entry_safe(p, tmp, &(mod->priv_list), priv_node) {
        list_del(&(p->priv_node));
        V3_Free(p);
    }

    V3_Free(mod->name);
    V3_Free(mod->content_hash);

    list_for_each_entry_safe(er, etmp, &(mod->er_list), er_node) {
        list_del(&er->er_node);
    }

    for (i = 0; i < mod->num_entries; i++) {
        V3_Free(mod->entry_points[i].name);
    }

    V3_Free(mod->entry_points);
    V3_Free(mod);
}


static int 
add_privs_to_list (struct v3_vm_info * vm,
                   struct list_head * list, 
                   unsigned int nprivs,
                   char ** priv_array)
{
    struct v3_priv * priv = NULL;
    int i;

    for (i = 0 ; i < nprivs; i++) {
        if ((priv = v3_lookup_priv(vm, priv_array[i])) == NULL) {
            PrintError(VM_NONE, VCORE_NONE,"Guarded module requested non-existent privilege: %s\n", priv_array[i]);
            return -1;
        } else {
            list_add(&(priv->priv_node), list);
        }
    }
    
    return 0;
}


static uint_t 
mod_name_hash_fn (addr_t key)
{
    char * name = (char*)key;
    return v3_hash_buffer((uint8_t*)name, strlen(name));
}


static int 
mod_name_eq_fn (addr_t key1, addr_t key2)
{
    char * name1 = (char*)key1;
    char * name2 = (char*)key2;
    return (strcmp(name1, name2) == 0);
}


static uint_t 
mod_id_hash_fn (addr_t key) 
{
    return v3_hash_long(key, sizeof(ullong_t) * 8);
}


static int 
mod_id_eq_fn (addr_t key1, addr_t key2) 
{
    return (key1 == key2);
}


static uint_t 
entry_addr_hash_fn (addr_t key) 
{
    return v3_hash_long(key, sizeof(addr_t) * 8);
}


static int 
entry_addr_eq_fn (addr_t key1, addr_t key2) 
{
    return (key1 == key2);
}


/* GUARD INIT 
 *
 * This is invoked through a hypercall that has been instrumented in the
 * guarded module's routine denoted by the module_init macro.
 */
/* TODO: add text hash check here */
static int
guard_init (struct guest_info * core, unsigned int hcall_id, void * priv_data)
{
    ullong_t mod_id;
    struct v3_gm * gm;
    struct v3_guarded_mods * mods = &global_mods;
    struct v3_entry_point * er = NULL;
    struct v3_entry_point * tmp = NULL;
    struct v3_priv * priv = NULL;
    struct v3_priv * ptmp = NULL;

    /* the guarded module should provide its ID in RBX */
    mod_id = core->vm_regs.rbx;

    V3_Print(VM_NONE, VCORE_NONE, "Received init request from GM (id=0x%llx)\n", mod_id);

    /* check if the corresponding module exists */
    if ((gm = (struct v3_gm*)v3_htable_search(mods->mod_id_table, (addr_t)mod_id)) == 0) {
        PrintError(VM_NONE, VCORE_NONE,"Module (id=0x%llx) not found\n", mod_id);
        return -1;
    }

    PrintDebug(VM_NONE, VCORE_NONE,"GM: initializing guarded module %s\n", gm->name);

    /* infer load address */
    gm->load_addr = core->rip - HCALL_INSTR_LEN - gm->hcall_offset;
    
    PrintDebug(VM_NONE, VCORE_NONE,"\tNew GM load address: %p\n", (void*)gm->load_addr);

    list_for_each_entry_safe(er, tmp, &(gm->er_list), er_node) {

        /* fix up the dynamic address of the entry point */
        er->addr += gm->load_addr;

        /* check if this entry point already exists */
        if (v3_htable_search(mods->er_hash, er->addr)) {
            PrintError(VM_NONE, VCORE_NONE,"GM: Error! Entry point already exists!\n");
            return -1;
        }

        /* add the new entry point to the global hash */
        v3_htable_insert(mods->er_hash, er->addr, (addr_t)er);
    }

    PrintDebug(VM_NONE, VCORE_NONE,"GM: Raising privileges\n");

    
    /* module will come up in privileged mode */
    gm->state = PRIV;

    /* raise all of the module's privileges */
    list_for_each_entry_safe(priv, ptmp, &(gm->priv_list), priv_node) {
        if (priv->raise(core, priv->private_data) < 0) {
            PrintError(VM_NONE, VCORE_NONE,"GM: Problem raising privilege\n");
            return -1;
        }
    }

    /* get the entry RSP for integrity checks */
    addr_t hva;
    if (v3_gva_to_hva(core, 
        get_addr_linear(core, (addr_t)core->vm_regs.rsp, &(core->segments.ss)),
        &hva) < 0) {
            PrintError(VM_NONE, VCORE_NONE,"GM: Problem translating stack address\n");
            return -1;
    }
    gm->entry_rsp = hva;

    /* this is just to be consistent with other entry types, r11 isn't used in init */
    gm->r11_stack_callback[gm->callback_nesting] = core->vm_regs.r11;
    gm->callback_nesting++;

    PrintDebug(VM_NONE, VCORE_NONE,"GM: Guarded module initialized, set to PRIV\n");

    current_gm = gm;

    return 0;
}


/* BORDER IN CALL
 *
 * This is invoked when the kernel calls the module (through a callback function)
 *
 * TODO: the stack checking model here is crappy, but will be updated to only
 * look at frame pointers and return addresses. 
 */
static int 
border_in_call (struct guest_info * core, unsigned int hcall_id, void * priv_data)
{
    struct v3_guarded_mods * mods = &global_mods;
    struct v3_entry_point * er;
    struct v3_gm * gm = current_gm;
    struct v3_priv * priv = NULL;
    struct v3_priv * tmp = NULL;


    addr_t entry = core->rip - HCALL_INSTR_LEN;

    er = (struct v3_entry_point*)v3_htable_search(mods->er_hash, entry);
    if (!er) {
        PrintError(VM_NONE, VCORE_NONE,"Attempt to enter at an invalid entry point (%p)\n", (void*)entry);
    } else {

        gm = er->gm;

        /* if this is first entry, record rsp */
        if (gm->callback_nesting == 0) {
            addr_t hva;
            if (v3_gva_to_hva(core, 
               get_addr_linear(core, (addr_t)core->vm_regs.rsp, &core->segments.ss),
               &hva) < 0) {
                    PrintError(VM_NONE, VCORE_NONE,"GM: Problem translating stack address\n");
                    return -1;
            }
            gm->entry_rsp = hva;
        }

        if (gm->callback_nesting >= MAX_BORDER_NESTING) {
            PrintError(VM_NONE, VCORE_NONE,"GM: Too much nesting of border crossings\n");
            return -1;
        } else {
            gm->r11_stack_callback[gm->callback_nesting] = core->vm_regs.r11;
            gm->callback_nesting++;
        }

        if (gm->state == INIT) {
            PrintError(VM_NONE, VCORE_NONE,"ERROR: module reached guard entry without being initialized!\n");
            return 0;
        }

        gm->state = PRIV;

        PrintDebug(VM_NONE, VCORE_NONE,"Entry request at %p granted to GM (%s)\n", (void*)entry, gm->name);

        list_for_each_entry_safe(priv, tmp, &(gm->priv_list), priv_node) {

            if (priv->raise(core, priv->private_data) < 0) {
                PrintError(VM_NONE, VCORE_NONE,"GM: Problem raising privilege\n");
                return -1;
            }

        }

    }
    
    return 0;
}


/*  BORDER-OUT RETURN
 * 
 * This is invoked when the module returns to the kernel, after being invoked through
 * a callback function.
 */
static int 
border_out_return (struct guest_info * core, unsigned int hcall_id, void * priv_data) 
{
    struct v3_gm * gm = current_gm;
    struct v3_priv * priv = NULL;
    struct v3_priv * tmp = NULL;

    gm->callback_nesting--;

    /* restore the guest's return address */
    core->vm_regs.r11 = gm->r11_stack_callback[gm->callback_nesting];
    gm->r11_stack_callback[gm->callback_nesting] = 0;

    /* Only lower privilege when we reach nesting 0 */
    if (gm->callback_nesting <= 0)  {

        PrintDebug(VM_NONE, VCORE_NONE,"GM: Granting exit request to GM (%s)\n", gm->name);
        list_for_each_entry_safe(priv, tmp, &(gm->priv_list), priv_node) {

            if (priv->lower(core, priv->private_data) < 0) {
                PrintError(VM_NONE, VCORE_NONE,"GM: Problem lowering privilege\n");
                return -1;
            }
        
        }

        gm->state = NO_PRIV;

    } else {
        PrintDebug(VM_NONE, VCORE_NONE,"Priv Lower requested, but still at nesting level (%d)\n", gm->callback_nesting);
    }

    return 0;
}


/* BORDER-OUT CALL
 *
 * This is invoked when the module calls out to the kernel, results in lowering of privilege
 *
 */
static int
border_out_call (struct guest_info * core, unsigned int hcall_id, void * priv_data) {

    struct v3_gm * gm = current_gm;
    struct v3_priv * priv = NULL;
    struct v3_priv * tmp = NULL;

    /* TODO: um...we seem to be getting a border out call in NO_PRIV state */
    if (1 || gm->state == PRIV || gm->state == INIT) {

        gm->r11_stack_kernel[gm->kernel_nesting] = core->vm_regs.r11;
        gm->kernel_nesting++;

        PrintDebug(VM_NONE, VCORE_NONE,"GM: Granting border out call request to GM (%s)\n", gm->name);
        list_for_each_entry_safe(priv, tmp, &(gm->priv_list), priv_node) {
            if (priv->lower(core, priv->private_data) < 0) {
                PrintError(VM_NONE, VCORE_NONE,"GM: Problem lowering privilege\n");
                return -1;
            }

        }

        if (v3_gva_to_hva(core, 
            get_addr_linear(core, (addr_t)core->vm_regs.rsp, &(core->segments.ss)),
            &gm->exit_rsp) < 0) {
            PrintError(VM_NONE, VCORE_NONE,"GM: border out, error translating rsp addr\n");
            return -1;
        }

        if (gm->entry_rsp && gm->exit_rsp && gm->state == PRIV && (gm->entry_rsp >= gm->exit_rsp)) {
            gm->stack_hash = v3_hash_buffer((uchar_t*)gm->exit_rsp, gm->entry_rsp - gm->exit_rsp);
        }

        gm->state = NO_PRIV;

    } else {
        PrintError(VM_NONE, VCORE_NONE,"GM: Trying to run border-out without privilege\n"); 
    }

    /* else, do nothing */
    return 0;
}


/* BORDER-IN RETURN
 * 
 * This is invoked when the kernel returns control to the module after a border-out call
 * Results in privilege raise
 *
 */
static int
border_in_return (struct guest_info * core, unsigned int hcall_id, void * priv_data) {
    struct v3_guarded_mods * mods = &global_mods;
    struct v3_gm * gm = NULL;
    struct v3_priv * priv = NULL;
    struct v3_priv * tmp = NULL;
    struct v3_entry_point * er = NULL;

    addr_t entry = core->rip - HCALL_INSTR_LEN;
    if ((er = (struct v3_entry_point*)v3_htable_search(mods->er_hash, entry)) == 0) {
        PrintError(VM_NONE, VCORE_NONE,"Attempt to enter at an invalid entry point (%p)\n", (void*)entry);
    } else {
        gm = er->gm;

        /* restore the guest's return address */
        gm->kernel_nesting--;
        core->vm_regs.r11 = gm->r11_stack_kernel[gm->kernel_nesting];
        gm->r11_stack_kernel[gm->kernel_nesting] = 0;

        
        if (gm->state == NO_PRIV) {
            PrintDebug(VM_NONE, VCORE_NONE,"GM: Granting border in return request to GM (%s)\n", gm->name);
            list_for_each_entry_safe(priv, tmp, &(gm->priv_list), priv_node) {
                if (priv->raise(core, priv->private_data) < 0) {
                    PrintError(VM_NONE, VCORE_NONE,"GM: Problem raising privilege\n");
                    return -1;
                }
            }

            if (gm->entry_rsp && gm->exit_rsp && (gm->entry_rsp > gm->exit_rsp)) {
                if (v3_hash_buffer((uchar_t*)gm->exit_rsp, gm->entry_rsp - gm->exit_rsp) != gm->stack_hash) {
                }
            }

            gm->state = PRIV;
        } 
    }

    return 0;
}


static int 
init_gm (struct v3_vm_info * vm, v3_cfg_tree_t * cfg, void ** priv_data) 
{
    struct v3_guarded_mods * mods = &global_mods;
    
    *priv_data = mods;

    INIT_LIST_HEAD(&(mods->mod_list));

    if (!(mods->mod_name_table = v3_create_htable(0, mod_name_hash_fn, mod_name_eq_fn))) {
        PrintError(VM_NONE, VCORE_NONE,"GM: Could not create GM module name hashtable\n");
        return -1;
    }

    if (!(mods->mod_id_table = v3_create_htable(0, mod_id_hash_fn, mod_id_eq_fn))) {
        PrintError(VM_NONE, VCORE_NONE,"GM: Could not create GM module ID hashtable\n");
        return -1;
    }

    if (!(mods->er_hash = v3_create_htable(0, entry_addr_hash_fn, entry_addr_eq_fn))) {
        PrintError(VM_NONE, VCORE_NONE,"GM: Could not create GM entry point hashtable\n");
        return -1;
    }

    /* these hypercalls will be invoked with a module id (hash) in rax. */
    if (v3_register_hypercall(vm, V3_BIN_CALL_HCALL, border_in_call, mods) < 0    ||
        v3_register_hypercall(vm, V3_BOUT_RET_HCALL, border_out_return, mods) < 0 ||
        v3_register_hypercall(vm, V3_BIN_RET_HCALL, border_in_return, mods) < 0   ||
        v3_register_hypercall(vm, V3_BOUT_CALL_HCALL, border_out_call, mods) < 0  ||
        v3_register_hypercall(vm, V3_GUARD_INIT_HCALL, guard_init, mods) < 0) {
        PrintError(VM_NONE, VCORE_NONE,"GM: Problem registering hypercalls\n");
        return -1;
    }

    return 0;
}


static int 
deinit_gm (struct v3_vm_info * vm, void * priv_data) 
{
    struct v3_guarded_mods * mods = (struct v3_guarded_mods *)priv_data;
    struct v3_gm * mod = NULL;
    struct v3_gm * tmp = NULL;

    list_for_each_entry_safe(mod, tmp, &(mods->mod_list), mod_node) {
        free_mod(mod);
    }

    v3_free_htable(mods->mod_id_table, 0, 0);
    v3_free_htable(mods->mod_name_table, 0, 0);
    v3_free_htable(mods->er_hash, 0, 0);

    if (v3_remove_hypercall(vm, V3_BIN_CALL_HCALL) < 0 ||
        v3_remove_hypercall(vm, V3_BOUT_RET_HCALL) < 0  ||
        v3_remove_hypercall(vm, V3_BIN_RET_HCALL) < 0  ||
        v3_remove_hypercall(vm, V3_BOUT_CALL_HCALL) < 0  ||
        v3_remove_hypercall(vm, V3_GUARD_INIT_HCALL) < 0) {

        PrintError(VM_NONE, VCORE_NONE,"GM: Problem removing GM hypercalls\n");
        return -1;
    }

    return 0;
}


/* returns 0 on error, module id on success */
unsigned long long
v3_register_gm  (void *  vm, 
                 char *  name,
                 char *  hash,
                 unsigned int hc_off,
                 unsigned int size,
                 unsigned int nentries,
                 unsigned int nprivs,
                 char ** priv_array,
                 void * private_data, 
                 void * entry_points)
{
    struct v3_guarded_mods * mods = &global_mods;
    struct v3_gm * mod;
    ullong_t mod_id;
    int i;
    char * tmp;

    if (!name) {
        PrintError(VM_NONE, VCORE_NONE,"Invalid module name\n");
        return 0;
    }

    if (v3_htable_search(mods->mod_name_table, (addr_t)name)) {
        PrintError(VM_NONE, VCORE_NONE,"Multiple instances of guarded module (%s)\n", name);
        return 0;
    }

    /* generate a unique identifier for this module */
    gen_mod_id(&mod_id);  

    mod = (struct v3_gm *)V3_Malloc(sizeof(struct v3_gm));
    if (!mod) {
        PrintError(VM_NONE, VCORE_NONE,"Problem allocating guarded module\n");
        return 0;
    }

    memset(mod, 0, sizeof(struct v3_gm));

    V3_Print(VM_NONE, VCORE_NONE, "V3 GM Registration: received name (%s)\n", name);
    mod->name = V3_Malloc(strlen(name)+1);
    if (!mod->name) {
        PrintError(VM_NONE, VCORE_NONE,"Problem allocating space for mod name\n");
        return -1;
    }

    V3_Print(VM_NONE, VCORE_NONE, "V3 GM Registration: received hash (%s)\n", hash);
    mod->content_hash = V3_Malloc(strlen(hash)+1);
    if (!mod->content_hash) {
        PrintError(VM_NONE, VCORE_NONE,"Problem allocating space for content hash\n");
        return -1;
    }
    strcpy(mod->name, name);
    strcpy(mod->content_hash, hash);

    mod->hcall_offset     = hc_off;
    mod->text_size        = size;
    mod->num_entries      = nentries;
    mod->private_data     = private_data;
    mod->id               = mod_id;
    mod->callback_nesting = 0;
    mod->kernel_nesting   = 0;

    INIT_LIST_HEAD(&(mod->priv_list));
    if (add_privs_to_list(vm, &(mod->priv_list), nprivs, priv_array) == -1) {
        PrintError(VM_NONE, VCORE_NONE,"Could not add privileges to guarded module\n");
        return -1;
    }

    mod->state      = INIT;
    mod->stack_hash = 0;

    /* setup the valid entry points */
    INIT_LIST_HEAD(&(mod->er_list));

    mod->entry_points = V3_Malloc(sizeof(struct v3_entry_point)*mod->num_entries);

    if (!mod->entry_points) {
        PrintError(VM_NONE, VCORE_NONE,"Problem allocating entry point array\n");
        return -1;
    }

    memcpy(mod->entry_points, entry_points, sizeof(struct v3_entry_point)*mod->num_entries);
    
    /* entries are added to the list, but can't be hashed yet since we're using
     * the load address as a key, but we haven't fixed these addresses up yet */
    for (i = 0; i < mod->num_entries; i++) {
        tmp = V3_Malloc(strlen(mod->entry_points[i].name)+1);
        if (!tmp) {
            PrintError(VM_NONE, VCORE_NONE,"Problem allocating space for name locally\n");
            return -1;
        }
        strcpy(tmp, mod->entry_points[i].name);
        mod->entry_points[i].name = tmp;
        mod->entry_points[i].gm   = mod;
        list_add(&(mod->entry_points[i].er_node), &(mod->er_list));
    }

    //v3_htable_insert(mods->mod_id_table, mod_id, (addr_t)mod);
    // TODO: this will change the content hash of the module code segment, fix!
    v3_htable_insert(mods->mod_id_table, (addr_t)0xa3aeea3aeebadbad, (addr_t)mod);
    v3_htable_insert(mods->mod_name_table, (addr_t)name, (addr_t)mod);
    list_add(&(mod->mod_node), &(mods->mod_list));
    return mod_id;
}


static struct v3_extension_impl guard_mods_impl = {
	.name = "guard_mods",
	.vm_init = init_gm,
	.vm_deinit = deinit_gm,
	.core_init = NULL,
	.core_deinit = NULL,
	.on_entry = NULL,
	.on_exit = NULL
};

register_extension(&guard_mods_impl);
