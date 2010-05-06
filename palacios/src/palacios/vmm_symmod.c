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
#include <palacios/vmm_symmod.h>
#include <palacios/vmm_symbiotic.h>
#include <palacios/vm_guest.h>

static struct hashtable * master_mod_table = NULL;

/* 
 * This is a place holder to ensure that the _v3_modules section gets created 
 */
static struct {} null_mod  __attribute__((__used__))			\
    __attribute__((unused, __section__ ("_v3_modules"),			\
		   aligned(sizeof(addr_t))));

static uint_t mod_hash_fn(addr_t key) {
    char * name = (char *)key;
    return v3_hash_buffer((uchar_t *)name, strlen(name));
}

static int mod_eq_fn(addr_t key1, addr_t key2) {
    char * name1 = (char *)key1;
    char * name2 = (char *)key2;
    
    return (strcmp(name1, name2) == 0);
}



int V3_init_symmod() {
    extern struct v3_sym_module __start__v3_modules[];
    extern struct v3_sym_module __stop__v3_modules[];
    struct v3_sym_module * tmp_mod = __start__v3_modules;
    int i = 0;

    if (tmp_mod == __stop__v3_modules) {
	PrintDebug("No Symbiotic modules found\n");
	return 0;
    }

    master_mod_table = v3_create_htable(0, mod_hash_fn, mod_eq_fn);

    while (tmp_mod != __stop__v3_modules) {

	if (v3_htable_search(master_mod_table, (addr_t)(tmp_mod->name))) {
	    PrintError("Multiple instances of Module (%s)\n", tmp_mod->name);
	    return -1;
	}
	
	PrintDebug("Registering Symbiotic Module (%s)\n", tmp_mod->name);

	if (v3_htable_insert(master_mod_table, 
			     (addr_t)(tmp_mod->name),
			     (addr_t)(tmp_mod)) == 0) {
	    PrintError("Could not insert module %s to master list\n", tmp_mod->name);
	    return -1;
	}
	tmp_mod = &(__start__v3_modules[++i]);
    }
    
    return 0;
}

/* ***************** */
/* Linkage functions */
/* ***************** */


/* Data structure containing symbols exported via the symbiotic interface */
struct v3_symbol_def32 {
    uint32_t name_gva;
    uint32_t value;
} __attribute__((packed));

struct v3_symbol {
    char name[256];
    uint64_t linkage;

    struct list_head sym_node;
};



#include <palacios/vm_guest_mem.h>

static int symbol_hcall_handler(struct guest_info * core, hcall_id_t hcall_id, void * priv_data) {
    struct v3_symmod_state * symmod_state = &(core->vm_info->sym_vm_state.symmod_state);
    addr_t sym_start_gva = core->vm_regs.rbx;
    uint32_t sym_size = core->vm_regs.rcx;

    int i = 0;

    PrintError("Received SYMMOD symbol tables addr=%p, size=%d\n", (void *)sym_start_gva, sym_size);

    for (i = 0; i < sym_size; i++) {
	char * sym_name = NULL;
	struct v3_symbol_def32 * tmp_symbol = NULL;
	struct v3_symbol * new_symbol = NULL;
	addr_t sym_gva = sym_start_gva + (sizeof(struct v3_symbol_def32) * i);


	if (guest_va_to_host_va(core, sym_gva, (addr_t *)&(tmp_symbol)) == -1) {
	    PrintError("Could not locate symbiotic symbol definition\n");
	    continue;
	}
	
	if (guest_va_to_host_va(core, tmp_symbol->name_gva, (addr_t *)&(sym_name)) == -1) {
	    PrintError("Could not locate symbiotic symbol name\n");
	    continue;
	}
	
	PrintError("Symbiotic Symbol (%s) at %p\n", sym_name, (void *)(addr_t)tmp_symbol->value);
	
	new_symbol = (struct v3_symbol *)V3_Malloc(sizeof(struct v3_symbol));

	strncpy(new_symbol->name, sym_name, 256);
	new_symbol->linkage = tmp_symbol->value;

	list_add(&(new_symbol->sym_node), &(symmod_state->v3_sym_list));
    }

    return 0;
}


int v3_init_symmod_vm(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct v3_symmod_state * symmod_state = &(vm->sym_vm_state.symmod_state);
    struct v3_symspy_global_page * sym_page = v3_sym_get_symspy_vm(vm);

    sym_page->symmod_enabled = 1;

    v3_register_hypercall(vm, SYMMOD_SYMS_HCALL, symbol_hcall_handler, NULL);

    INIT_LIST_HEAD(&(symmod_state->v3_sym_list));

    V3_Print("Symmod initialized\n");

    return 0;
}



int v3_set_symmod_loader(struct v3_vm_info * vm, struct v3_symmod_loader_ops * ops, void * priv_data) {
    struct v3_symmod_state * symmod_state = &(vm->sym_vm_state.symmod_state);
   
    symmod_state->loader_ops = ops;
    symmod_state->loader_data = priv_data;
 
    return 0;
}





int v3_load_sym_module(struct v3_vm_info * vm, char * mod_name) {
    struct v3_symmod_state * symmod_state = &(vm->sym_vm_state.symmod_state);
    struct v3_sym_module * mod = (struct v3_sym_module *)v3_htable_search(master_mod_table, (addr_t)mod_name);

    if (!mod) {
	PrintError("Could not find module %s\n", mod_name);
	return -1;
    }

    PrintDebug("Loading Module (%s)\n", mod_name);

    return symmod_state->loader_ops->load_module(vm, mod, symmod_state->loader_data);
}




struct v3_sym_module * v3_get_sym_module(struct v3_vm_info * vm, char * name) {
    struct v3_sym_module * mod = (struct v3_sym_module *)v3_htable_search(master_mod_table, (addr_t)name);

    if (!mod) {
	PrintError("Could not find module %s\n", name);
	return NULL;
    }

    return mod;
}
