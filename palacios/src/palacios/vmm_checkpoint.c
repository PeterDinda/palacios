/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2011, Madhav Suresh <madhav@u.northwestern.edu> 
 * Copyright (c) 2011, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Madhav Suresh <madhav@u.northwestern.edu>
 *	   Arefin Huq <fig@arefin.net>
 *
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>
#include <palacios/vmm_sprintf.h>
#include <palacios/vm_guest.h>
#include <palacios/svm.h>
#include <palacios/vmx.h>
#include <palacios/vmm_checkpoint.h>
#include <palacios/vmm_hashtable.h>
#include <palacios/vmm_direct_paging.h>

#include <palacios/vmm_dev_mgr.h>


static struct hashtable * store_table = NULL;

struct v3_chkpt;

typedef enum {SAVE, LOAD} chkpt_mode_t;

struct chkpt_interface {
    char name[128];
    void * (*open_chkpt)(char * url, chkpt_mode_t mode);
    int (*close_chkpt)(void * store_data);
    
    void * (*open_ctx)(void * store_data, void * parent_ctx, char * name);
    int (*close_ctx)(void * store_data, void * ctx);
    
    int (*save)(void * store_data, void * ctx, char * tag, uint64_t len, void * buf);
    int (*load)(void * store_data, void * ctx, char * tag, uint64_t len, void * buf);
};


struct v3_chkpt {
    struct v3_vm_info * vm;

    struct chkpt_interface * interface;

    void * store_data;
};




static uint_t store_hash_fn(addr_t key) {
    char * name = (char *)key;
    return v3_hash_buffer((uint8_t *)name, strlen(name));
}

static int store_eq_fn(addr_t key1, addr_t key2) {
    char * name1 = (char *)key1;
    char * name2 = (char *)key2;

    return (strcmp(name1, name2) == 0);
}



#include "vmm_chkpt_stores.h"


int V3_init_checkpoint() {
    extern struct chkpt_interface * __start__v3_chkpt_stores[];
    extern struct chkpt_interface * __stop__v3_chkpt_stores[];
    struct chkpt_interface ** tmp_store = __start__v3_chkpt_stores;
    int i = 0;

    store_table = v3_create_htable(0, store_hash_fn, store_eq_fn);

    while (tmp_store != __stop__v3_chkpt_stores) {
	V3_Print("Registering Checkpoint Backing Store (%s)\n", (*tmp_store)->name);

	if (v3_htable_search(store_table, (addr_t)((*tmp_store)->name))) {
	    PrintError("Multiple instances of Checkpoint backing Store (%s)\n", (*tmp_store)->name);
	    return -1;
	}

	if (v3_htable_insert(store_table, (addr_t)((*tmp_store)->name), (addr_t)(*tmp_store)) == 0) {
	    PrintError("Could not register Checkpoint backing store (%s)\n", (*tmp_store)->name);
	    return -1;
	}

	tmp_store = &(__start__v3_chkpt_stores[++i]);
    }

    return 0;
}

int V3_deinit_checkpoint() {
    v3_free_htable(store_table, 0, 0);
    return 0;
}


static char svm_chkpt_header[] = "v3vee palacios checkpoint version: x.x, SVM x.x";
static char vmx_chkpt_header[] = "v3vee palacios checkpoint version: x.x, VMX x.x";

static int chkpt_close(struct v3_chkpt * chkpt) {
    chkpt->interface->close_chkpt(chkpt->store_data);

    V3_Free(chkpt);

    return 0;
}


static struct v3_chkpt * chkpt_open(struct v3_vm_info * vm, char * store, char * url, chkpt_mode_t mode) {
    struct chkpt_interface * iface = NULL;
    struct v3_chkpt * chkpt = NULL;
    void * store_data = NULL;

    iface = (void *)v3_htable_search(store_table, (addr_t)store);
    
    if (iface == NULL) {
	V3_Print("Error: Could not locate Checkpoint interface for store (%s)\n", store);
	return NULL;
    }

    store_data = iface->open_chkpt(url, mode);

    if (store_data == NULL) {
	PrintError("Could not open url (%s) for backing store (%s)\n", url, store);
	return NULL;
    }


    chkpt = V3_Malloc(sizeof(struct v3_chkpt));

    if (!chkpt) {
	PrintError("Could not allocate checkpoint state\n");
	return NULL;
    }

    chkpt->interface = iface;
    chkpt->vm = vm;
    chkpt->store_data = store_data;
    
    return chkpt;
}

struct v3_chkpt_ctx * v3_chkpt_open_ctx(struct v3_chkpt * chkpt, struct v3_chkpt_ctx * parent, char * name) {
    struct v3_chkpt_ctx * ctx = V3_Malloc(sizeof(struct v3_chkpt_ctx));
    void * parent_store_ctx = NULL;

    memset(ctx, 0, sizeof(struct v3_chkpt_ctx));

    ctx->chkpt = chkpt;
    ctx->parent = parent;

    if (parent) {
	parent_store_ctx = parent->store_ctx;
    }

    ctx->store_ctx = chkpt->interface->open_ctx(chkpt->store_data, parent_store_ctx, name);

    return ctx;
}

int v3_chkpt_close_ctx(struct v3_chkpt_ctx * ctx) {
    struct v3_chkpt * chkpt = ctx->chkpt;
    int ret = 0;

    ret = chkpt->interface->close_ctx(chkpt->store_data, ctx->store_ctx);

    V3_Free(ctx);

    return ret;
}





int v3_chkpt_save(struct v3_chkpt_ctx * ctx, char * tag, uint64_t len, void * buf) {
    struct v3_chkpt * chkpt = ctx->chkpt;    
    return chkpt->interface->save(chkpt->store_data, ctx->store_ctx, tag, len, buf);
}


int v3_chkpt_load(struct v3_chkpt_ctx * ctx, char * tag, uint64_t len, void * buf) {
    struct v3_chkpt * chkpt = ctx->chkpt;    
    return chkpt->interface->load(chkpt->store_data, ctx->store_ctx, tag, len, buf);
}



static int load_memory(struct v3_vm_info * vm, struct v3_chkpt * chkpt) {

    void * guest_mem_base = NULL;
    void * ctx = NULL;
    uint64_t ret = 0;

    guest_mem_base = V3_VAddr((void *)vm->mem_map.base_region.host_addr);

    ctx = v3_chkpt_open_ctx(chkpt, NULL, "memory_img");

    ret = v3_chkpt_load(ctx, "memory_img", vm->mem_size, guest_mem_base);
    v3_chkpt_close_ctx(ctx);

    return ret;
}


static int save_memory(struct v3_vm_info * vm, struct v3_chkpt * chkpt) {
    void * guest_mem_base = NULL;
    void * ctx = NULL;
    uint64_t ret = 0;

    guest_mem_base = V3_VAddr((void *)vm->mem_map.base_region.host_addr);

    ctx = v3_chkpt_open_ctx(chkpt, NULL,"memory_img");


    ret = v3_chkpt_save(ctx, "memory_img", vm->mem_size, guest_mem_base);
    v3_chkpt_close_ctx(ctx);

    return ret;
}

int save_header(struct v3_vm_info * vm, struct v3_chkpt * chkpt) {
    v3_cpu_arch_t cpu_type = v3_get_cpu_type(V3_Get_CPU());
    void * ctx = NULL;
    
    ctx = v3_chkpt_open_ctx(chkpt, NULL, "header");

    switch (cpu_type) {
	case V3_SVM_CPU:
	case V3_SVM_REV3_CPU: {
	    v3_chkpt_save(ctx, "header", strlen(svm_chkpt_header), svm_chkpt_header);
	    break;
	}
	case V3_VMX_CPU:
	case V3_VMX_EPT_CPU:
	case V3_VMX_EPT_UG_CPU: {
	    v3_chkpt_save(ctx, "header", strlen(vmx_chkpt_header), vmx_chkpt_header);
	    break;
	}
	default:
	    PrintError("checkpoint not supported on this architecture\n");
	    v3_chkpt_close_ctx(ctx);
	    return -1;
    }

    v3_chkpt_close_ctx(ctx);
	    
    return 0;
}

static int load_header(struct v3_vm_info * vm, struct v3_chkpt * chkpt) {
    v3_cpu_arch_t cpu_type = v3_get_cpu_type(V3_Get_CPU());
    void * ctx = NULL;
    
    ctx = v3_chkpt_open_ctx(chkpt, NULL, "header");

    switch (cpu_type) {
	case V3_SVM_CPU:
	case V3_SVM_REV3_CPU: {
	    char header[strlen(svm_chkpt_header) + 1];
	 
	    v3_chkpt_load(ctx, "header", strlen(svm_chkpt_header), header);

	    break;
	}
	case V3_VMX_CPU:
	case V3_VMX_EPT_CPU:
	case V3_VMX_EPT_UG_CPU: {
	    char header[strlen(vmx_chkpt_header) + 1];
	    
	    v3_chkpt_load(ctx, "header", strlen(vmx_chkpt_header), header);
	    
	    break;
	}
	default:
	    PrintError("checkpoint not supported on this architecture\n");
	    v3_chkpt_close_ctx(ctx);
	    return -1;
    }

    v3_chkpt_close_ctx(ctx);

    return 0;
}


static int load_core(struct guest_info * info, struct v3_chkpt * chkpt) {
    v3_cpu_arch_t cpu_type = v3_get_cpu_type(V3_Get_CPU());
    void * ctx = NULL;
    char key_name[16];
    memset(key_name, 0, 16);

    snprintf(key_name, 16, "guest_info%d", info->vcpu_id);

    ctx = v3_chkpt_open_ctx(chkpt, NULL, key_name);

    v3_chkpt_load_64(ctx, "RIP", &(info->rip));

    V3_CHKPT_STD_LOAD(ctx, info->vm_regs);

    V3_CHKPT_STD_LOAD(ctx, info->ctrl_regs.cr0);
    V3_CHKPT_STD_LOAD(ctx, info->ctrl_regs.cr2);
    V3_CHKPT_STD_LOAD(ctx, info->ctrl_regs.cr4);
    V3_CHKPT_STD_LOAD(ctx, info->ctrl_regs.cr8);
    V3_CHKPT_STD_LOAD(ctx, info->ctrl_regs.rflags);
    V3_CHKPT_STD_LOAD(ctx, info->ctrl_regs.efer);

    V3_CHKPT_STD_LOAD(ctx, info->dbg_regs);
    V3_CHKPT_STD_LOAD(ctx, info->segments);
    V3_CHKPT_STD_LOAD(ctx, info->shdw_pg_state.guest_cr3);
    V3_CHKPT_STD_LOAD(ctx, info->shdw_pg_state.guest_cr0);
    V3_CHKPT_STD_LOAD(ctx, info->shdw_pg_state.guest_efer);
    v3_chkpt_close_ctx(ctx);

    PrintDebug("Finished reading guest_info information\n");

    info->cpu_mode = v3_get_vm_cpu_mode(info);
    info->mem_mode = v3_get_vm_mem_mode(info);

    if (info->shdw_pg_mode == SHADOW_PAGING) {
	if (v3_get_vm_mem_mode(info) == VIRTUAL_MEM) {
	    if (v3_activate_shadow_pt(info) == -1) {
		PrintError("Failed to activate shadow page tables\n");
		return -1;
	    }
	} else {
	    if (v3_activate_passthrough_pt(info) == -1) {
		PrintError("Failed to activate passthrough page tables\n");
		return -1;
	    }
	}
    }


    switch (cpu_type) {
	case V3_SVM_CPU:
	case V3_SVM_REV3_CPU: {
	    char key_name[16];

	    snprintf(key_name, 16, "vmcb_data%d", info->vcpu_id);
	    ctx = v3_chkpt_open_ctx(chkpt, NULL, key_name);
	    
	    if (v3_svm_load_core(info, ctx) == -1) {
		PrintError("Failed to patch core %d\n", info->vcpu_id);
		return -1;
	    }

	    v3_chkpt_close_ctx(ctx);

	    break;
	}
	case V3_VMX_CPU:
	case V3_VMX_EPT_CPU:
	case V3_VMX_EPT_UG_CPU: {
	    char key_name[16];

	    snprintf(key_name, 16, "vmcs_data%d", info->vcpu_id);
	    ctx = v3_chkpt_open_ctx(chkpt, NULL, key_name);
	    
	    if (v3_vmx_load_core(info, ctx) < 0) {
		PrintError("VMX checkpoint failed\n");
		return -1;
	    }

	    v3_chkpt_close_ctx(ctx);

	    break;
	}
	default:
	    PrintError("Invalid CPU Type (%d)\n", cpu_type);
	    return -1;
    }

    v3_print_guest_state(info);

    return 0;
}


static int save_core(struct guest_info * info, struct v3_chkpt * chkpt) {
    v3_cpu_arch_t cpu_type = v3_get_cpu_type(V3_Get_CPU());
    void * ctx = NULL;
    char key_name[16];

    memset(key_name, 0, 16);

    v3_print_guest_state(info);


    snprintf(key_name, 16, "guest_info%d", info->vcpu_id);

    ctx = v3_chkpt_open_ctx(chkpt, NULL, key_name);

    v3_chkpt_save_64(ctx, "RIP", &(info->rip));

    V3_CHKPT_STD_SAVE(ctx, info->vm_regs);

    V3_CHKPT_STD_SAVE(ctx, info->ctrl_regs.cr0);
    V3_CHKPT_STD_SAVE(ctx, info->ctrl_regs.cr2);
    V3_CHKPT_STD_SAVE(ctx, info->ctrl_regs.cr4);
    V3_CHKPT_STD_SAVE(ctx, info->ctrl_regs.cr8);
    V3_CHKPT_STD_SAVE(ctx, info->ctrl_regs.rflags);
    V3_CHKPT_STD_SAVE(ctx, info->ctrl_regs.efer);

    V3_CHKPT_STD_SAVE(ctx, info->dbg_regs);
    V3_CHKPT_STD_SAVE(ctx, info->segments);
    V3_CHKPT_STD_SAVE(ctx, info->shdw_pg_state.guest_cr3);
    V3_CHKPT_STD_SAVE(ctx, info->shdw_pg_state.guest_cr0);
    V3_CHKPT_STD_SAVE(ctx, info->shdw_pg_state.guest_efer);

    v3_chkpt_close_ctx(ctx);

    //Architechture specific code
    switch (cpu_type) {
	case V3_SVM_CPU:
	case V3_SVM_REV3_CPU: {
	    char key_name[16];
	    void * ctx = NULL;
	    
	    snprintf(key_name, 16, "vmcb_data%d", info->vcpu_id);
	    
	    ctx = v3_chkpt_open_ctx(chkpt, NULL, key_name);
	    
	    if (v3_svm_save_core(info, ctx) == -1) {
		PrintError("VMCB Unable to be written\n");
		v3_chkpt_close_ctx(ctx);
		return -1;
	    }
	    
	    v3_chkpt_close_ctx(ctx);
	    break;
	}
	case V3_VMX_CPU:
	case V3_VMX_EPT_CPU:
	case V3_VMX_EPT_UG_CPU: {
	    char key_name[16];
	    void * ctx = NULL;

	    snprintf(key_name, 16, "vmcs_data%d", info->vcpu_id);
	    
	    ctx = v3_chkpt_open_ctx(chkpt, NULL, key_name);

	    if (v3_vmx_save_core(info, ctx) == -1) {
		PrintError("VMX checkpoint failed\n");
		v3_chkpt_close_ctx(ctx);
		return -1;
	    }

	    v3_chkpt_close_ctx(ctx);

	    break;
	}
	default:
	    PrintError("Invalid CPU Type (%d)\n", cpu_type);
	    return -1;
    }
    
    return 0;
}


int v3_chkpt_save_vm(struct v3_vm_info * vm, char * store, char * url) {
    struct v3_chkpt * chkpt = NULL;
    int ret = 0;;
    int i = 0;
    
    chkpt = chkpt_open(vm, store, url, SAVE);

    if (chkpt == NULL) {
	PrintError("Error creating checkpoint store\n");
	return -1;
    }

    /* If this guest is running we need to block it while the checkpoint occurs */
    if (vm->run_state == VM_RUNNING) {
	while (v3_raise_barrier(vm, NULL) == -1);
    }

    if ((ret = save_memory(vm, chkpt)) == -1) {
	PrintError("Unable to save memory\n");
	goto out;
    }
    
    
    if ((ret = v3_save_vm_devices(vm, chkpt)) == -1) {
	PrintError("Unable to save devices\n");
	goto out;
    }
    

    if ((ret = save_header(vm, chkpt)) == -1) {
	PrintError("Unable to save header\n");
	goto out;
    }
    
    for (i = 0; i < vm->num_cores; i++){
	if ((ret = save_core(&(vm->cores[i]), chkpt)) == -1) {
	    PrintError("chkpt of core %d failed\n", i);
	    goto out;
	}
    }
    
 out:
    
    /* Resume the guest if it was running */
    if (vm->run_state == VM_RUNNING) {
	v3_lower_barrier(vm);
    }

    chkpt_close(chkpt);

    return ret;

}

int v3_chkpt_load_vm(struct v3_vm_info * vm, char * store, char * url) {
    struct v3_chkpt * chkpt = NULL;
    int i = 0;
    int ret = 0;
    
    chkpt = chkpt_open(vm, store, url, LOAD);

    if (chkpt == NULL) {
	PrintError("Error creating checkpoint store\n");
	return -1;
    }

    /* If this guest is running we need to block it while the checkpoint occurs */
    if (vm->run_state == VM_RUNNING) {
	while (v3_raise_barrier(vm, NULL) == -1);
    }

    if ((ret = load_memory(vm, chkpt)) == -1) {
	PrintError("Unable to save memory\n");
	goto out;
    }


    if ((ret = v3_load_vm_devices(vm, chkpt)) == -1) {
	PrintError("Unable to load devies\n");
	goto out;
    }


    if ((ret = load_header(vm, chkpt)) == -1) {
	PrintError("Unable to load header\n");
	goto out;
    }

    //per core cloning
    for (i = 0; i < vm->num_cores; i++) {
	if ((ret = load_core(&(vm->cores[i]), chkpt)) == -1) {
	    PrintError("Error loading core state (core=%d)\n", i);
	    goto out;
	}
    }

 out:

    /* Resume the guest if it was running and we didn't just trash the state*/
    if (vm->run_state == VM_RUNNING) {
    
	if (ret == -1) {
	    vm->run_state = VM_STOPPED;
	}

	/* We check the run state of the VM after every barrier 
	   So this will immediately halt the VM 
	*/
	v3_lower_barrier(vm);
    }

    chkpt_close(chkpt);

    return ret;
}



