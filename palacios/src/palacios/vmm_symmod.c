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
#include <palacios/vmm_list.h>

static struct hashtable * capsule_table = NULL;
static LIST_HEAD(capsule_list);


/* 
 * This is a place holder to ensure that the _v3_modules section gets created by gcc
 */
static struct {} null_mod  __attribute__((__used__))			\
    __attribute__((unused, __section__ ("_v3_capsules"),		\
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


struct capsule_def {
    char * name;
    addr_t start_addr;
    addr_t end_addr;
    uint32_t flags; // see 'struct v3_symmod_flags'
} __attribute__((packed));



int V3_init_symmod() {
    extern struct capsule_def __start__v3_capsules[];
    extern struct capsule_def __stop__v3_capsules[];
    struct capsule_def * tmp_def = __start__v3_capsules;
    int i = 0;

    if (tmp_def == __stop__v3_capsules) {
	PrintDebug("No Symbiotic capsules found\n");
	return 0;
    }

    capsule_table = v3_create_htable(0, mod_hash_fn, mod_eq_fn);

    while (tmp_def != __stop__v3_capsules) {
	struct v3_sym_capsule * capsule = NULL;

	if (v3_htable_search(capsule_table, (addr_t)(tmp_def->name))) {
	    PrintError("Multiple instances of Module (%s)\n", tmp_def->name);
	    return -1;
	}
	

	capsule = V3_Malloc(sizeof(struct v3_sym_capsule));
	memset(capsule, 0, sizeof(struct v3_sym_capsule));

	capsule->name = tmp_def->name;
	capsule->start_addr = (void *)(tmp_def->start_addr);
	capsule->size = tmp_def->end_addr - tmp_def->start_addr;
	capsule->flags = tmp_def->flags;
	
	

	if (capsule->type == V3_SYMMOD_MOD) {
	    // parse module
	    // capsule->capsule_data = v3_sym_module...
	    // capsule->guest_size = size of linked module
	} else if (capsule->type == V3_SYMMOD_LNX) {
	    capsule->guest_size = capsule->size;
	    capsule->capsule_data = NULL;
	} else {
	    return -1;
	}

	PrintDebug("Registering Symbiotic Module (%s)\n", tmp_def->name);

	if (v3_htable_insert(capsule_table, 
			     (addr_t)(tmp_def->name),
			     (addr_t)(capsule)) == 0) {
	    PrintError("Could not insert module %s to master list\n", tmp_def->name);
	    return -1;
	}

	list_add(&(capsule->node), &capsule_list);

	tmp_def = &(__start__v3_capsules[++i]);
    }
    
    return 0;
}

int V3_deinit_symmod() {
    v3_free_htable(capsule_table, 1, 0);    

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

struct v3_symbol_def64 {
    uint64_t name_gva;
    uint64_t value;
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


	if (v3_gva_to_hva(core, sym_gva, (addr_t *)&(tmp_symbol)) == -1) {
	    PrintError("Could not locate symbiotic symbol definition\n");
	    continue;
	}
	
	if (v3_gva_to_hva(core, tmp_symbol->name_gva, (addr_t *)&(sym_name)) == -1) {
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
    struct v3_sym_capsule * tmp_capsule = NULL;

    symmod_state->capsule_table = v3_create_htable(0, mod_hash_fn, mod_eq_fn);


    // Add modules to local hash table, should be keyed to config
    list_for_each_entry(tmp_capsule, &capsule_list, node) {
	V3_Print("Adding %s to local module table\n", tmp_capsule->name);
	if (v3_htable_insert(symmod_state->capsule_table, 
			     (addr_t)(tmp_capsule->name),
			     (addr_t)(tmp_capsule)) == 0) {
	    PrintError("Could not insert module %s to vm local list\n", tmp_capsule->name);
	    return -1;
	}
	symmod_state->num_avail_capsules++;
    }

    symmod_state->num_loaded_capsules = 0;
    sym_page->symmod_enabled = 1;

    v3_register_hypercall(vm, SYMMOD_SYMS_HCALL, symbol_hcall_handler, NULL);

    INIT_LIST_HEAD(&(symmod_state->v3_sym_list));

    V3_Print("Symmod initialized\n");

    return 0;
}

int v3_deinit_symmod_vm(struct v3_vm_info * vm) {
    struct v3_symmod_state * symmod_state = &(vm->sym_vm_state.symmod_state);
    struct v3_symbol * sym = NULL;
    struct v3_symbol * tmp_sym = NULL;

    v3_remove_hypercall(vm, SYMMOD_SYMS_HCALL);

    v3_free_htable(symmod_state->capsule_table, 0, 0);

    list_for_each_entry_safe(sym, tmp_sym, &(symmod_state->v3_sym_list), sym_node) {
	list_del(&(sym->sym_node));
	V3_Free(sym);
    }

    return 0;
}


int v3_set_symmod_loader(struct v3_vm_info * vm, struct v3_symmod_loader_ops * ops, void * priv_data) {
    struct v3_symmod_state * symmod_state = &(vm->sym_vm_state.symmod_state);
   
    symmod_state->loader_ops = ops;
    symmod_state->loader_data = priv_data;
 
    return 0;
}





int v3_load_sym_capsule(struct v3_vm_info * vm, char * name) {
    struct v3_symmod_state * symmod_state = &(vm->sym_vm_state.symmod_state);
    struct v3_sym_capsule * capsule = (struct v3_sym_capsule *)v3_htable_search(capsule_table, (addr_t)name);

    if (!capsule) {
	PrintError("Could not find capsule %s\n", name);
	return -1;
    }

    PrintDebug("Loading Capsule (%s)\n", name);

    return symmod_state->loader_ops->load_capsule(vm, capsule, symmod_state->loader_data);
}



struct list_head * v3_get_sym_capsule_list() {
    return &(capsule_list);
}


struct v3_sym_capsule * v3_get_sym_capsule(struct v3_vm_info * vm, char * name) {
    struct v3_sym_capsule * mod = (struct v3_sym_capsule *)v3_htable_search(capsule_table, (addr_t)name);

    if (!mod) {
	PrintError("Could not find module %s\n", name);
	return NULL;
    }

    return mod;
}
