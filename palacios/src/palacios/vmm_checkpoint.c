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
#include <palacios/vmm_debug.h>

#include <palacios/vmm_dev_mgr.h>

#ifdef V3_CONFIG_LIVE_MIGRATION
#include <palacios/vmm_time.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_shadow_paging.h>
#endif

#ifndef V3_CONFIG_DEBUG_CHECKPOINT
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


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


    if (!ctx) { 
	PrintError("Unable to allocate context\n");
	return 0;
    }

    memset(ctx, 0, sizeof(struct v3_chkpt_ctx));

    ctx->chkpt = chkpt;
    ctx->parent = parent;

    if (parent) {
	parent_store_ctx = parent->store_ctx;
    }

    ctx->store_ctx = chkpt->interface->open_ctx(chkpt->store_data, parent_store_ctx, name);

    if (!(ctx->store_ctx)) {
	PrintError("Warning: opening underlying representation returned null\n");
    }

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
    
    return  chkpt->interface->save(chkpt->store_data, ctx->store_ctx, tag, len, buf);

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
    
    if (!ctx) { 
	PrintError("Unable to open context for memory load\n");
	return -1;
    }
		     
    if (v3_chkpt_load(ctx, "memory_img", vm->mem_size, guest_mem_base) == -1) {
	PrintError("Unable to load all of memory (requested=%llu bytes, result=%llu bytes\n",(uint64_t)(vm->mem_size),ret);
	v3_chkpt_close_ctx(ctx);
	return -1;
    }
    
    v3_chkpt_close_ctx(ctx);

    return 0;
}


static int save_memory(struct v3_vm_info * vm, struct v3_chkpt * chkpt) {
    void * guest_mem_base = NULL;
    void * ctx = NULL;
    uint64_t ret = 0;

    guest_mem_base = V3_VAddr((void *)vm->mem_map.base_region.host_addr);

    ctx = v3_chkpt_open_ctx(chkpt, NULL,"memory_img");

    if (!ctx) { 
	PrintError("Unable to open context to save memory\n");
	return -1;
    }

    if (v3_chkpt_save(ctx, "memory_img", vm->mem_size, guest_mem_base) == -1) {
	PrintError("Unable to load all of memory (requested=%llu, received=%llu)\n",(uint64_t)(vm->mem_size),ret);
	v3_chkpt_close_ctx(ctx);  
	return -1;
    }

    v3_chkpt_close_ctx(ctx);

    return 0;
}

#ifdef V3_CONFIG_LIVE_MIGRATION

struct mem_migration_state {
    struct v3_vm_info *vm;
    struct v3_bitmap  modified_pages; 
};

static int paging_callback(struct guest_info *core, 
			   struct v3_shdw_pg_event *event,
			   void      *priv_data)
{
    struct mem_migration_state *m = (struct mem_migration_state *)priv_data;
    
    if (event->event_type==SHADOW_PAGEFAULT &&
	event->event_order==SHADOW_PREIMPL &&
	event->error_code.write) { 
	addr_t gpa;
	if (!v3_gva_to_gpa(core,event->gva,&gpa)) {
	    // write to this page
	    v3_bitmap_set(&(m->modified_pages),gpa>>12);
	} else {
	    // no worries, this isn't physical memory
	}
    } else {
	// we don't care about other events
    }
    
    return 0;
}
	


static struct mem_migration_state *start_page_tracking(struct v3_vm_info *vm)
{
    struct mem_migration_state *m;
    int i;

    m = (struct mem_migration_state *)V3_Malloc(sizeof(struct mem_migration_state));

    if (!m) { 
	PrintError("Cannot allocate\n");
	return NULL;
    }

    m->vm=vm;
    
    if (v3_bitmap_init(&(m->modified_pages),vm->mem_size >> 12) == -1) { 
	PrintError("Failed to initialize modified_pages bit vector");
	V3_Free(m);
    }

    v3_register_shadow_paging_event_callback(vm,paging_callback,m);

    for (i=0;i<vm->num_cores;i++) {
	v3_invalidate_shadow_pts(&(vm->cores[i]));
    }
    
    // and now we should get callbacks as writes happen

    return m;
}

static void stop_page_tracking(struct mem_migration_state *m)
{
    v3_unregister_shadow_paging_event_callback(m->vm,paging_callback,m);
    
    v3_bitmap_deinit(&(m->modified_pages));
    
    V3_Free(m);
}

	    
		
							    


//
// Returns
//  negative: error
//  zero: done with this round
static int save_inc_memory(struct v3_vm_info * vm, 
                           struct v3_bitmap * mod_pgs_to_send, 
                           struct v3_chkpt * chkpt) {
    int page_size_bytes = 1 << 12; // assuming 4k pages right now
    void * ctx = NULL;
    int i = 0; 
    void * guest_mem_base = NULL;
    int bitmap_num_bytes = (mod_pgs_to_send->num_bits / 8) 
                           + ((mod_pgs_to_send->num_bits % 8) > 0);

   
    guest_mem_base = V3_VAddr((void *)vm->mem_map.base_region.host_addr);
    
    PrintDebug("Saving incremental memory.\n");

    ctx = v3_chkpt_open_ctx(chkpt, NULL,"memory_bitmap_bits");

    if (!ctx) { 
	PrintError("Cannot open context for dirty memory bitmap\n");
	return -1;
    }
	

    if (v3_chkpt_save(ctx,
		      "memory_bitmap_bits",
		      bitmap_num_bytes,
		      mod_pgs_to_send->bits) == -1) {
	PrintError("Unable to write all of the dirty memory bitmap\n");
	v3_chkpt_close_ctx(ctx);
	return -1;
    }

    v3_chkpt_close_ctx(ctx);

    PrintDebug("Sent bitmap bits.\n");

    // Dirty memory pages are sent in bitmap order
    for (i = 0; i < mod_pgs_to_send->num_bits; i++) {
        if (v3_bitmap_check(mod_pgs_to_send, i)) {
           // PrintDebug("Sending memory page %d.\n",i);
            ctx = v3_chkpt_open_ctx(chkpt, NULL,"memory_page");
	    if (!ctx) { 
		PrintError("Unable to open context to send memory page\n");
		return -1;
	    }
            if (v3_chkpt_save(ctx, 
			      "memory_page", 
			      page_size_bytes,
			      guest_mem_base + (page_size_bytes * i)) == -1) {
		PrintError("Unable to send a memory page\n");
		v3_chkpt_close_ctx(ctx);
		return -1;
	    }
	    
            v3_chkpt_close_ctx(ctx);
        }
    } 
    
    return 0;
}


//
// returns:
//  negative: error
//  zero: ok, but not done
//  positive: ok, and also done
static int load_inc_memory(struct v3_vm_info * vm, 
                           struct v3_bitmap * mod_pgs,
                           struct v3_chkpt * chkpt) {
    int page_size_bytes = 1 << 12; // assuming 4k pages right now
    void * ctx = NULL;
    int i = 0; 
    void * guest_mem_base = NULL;
    bool empty_bitmap = true;
    int bitmap_num_bytes = (mod_pgs->num_bits / 8) 
                           + ((mod_pgs->num_bits % 8) > 0);


    guest_mem_base = V3_VAddr((void *)vm->mem_map.base_region.host_addr);

    ctx = v3_chkpt_open_ctx(chkpt, NULL,"memory_bitmap_bits");

    if (!ctx) { 
	PrintError("Cannot open context to receive memory bitmap\n");
	return -1;
    }

    if (v3_chkpt_load(ctx,
		      "memory_bitmap_bits",
		      bitmap_num_bytes,
		      mod_pgs->bits) == -1) {
	PrintError("Did not receive all of memory bitmap\n");
	v3_chkpt_close_ctx(ctx);
	return -1;
    }
    
    v3_chkpt_close_ctx(ctx);

    // Receive also follows bitmap order
    for (i = 0; i < mod_pgs->num_bits; i ++) {
        if (v3_bitmap_check(mod_pgs, i)) {
            PrintDebug("Loading page %d\n", i);
            empty_bitmap = false;
            ctx = v3_chkpt_open_ctx(chkpt, NULL,"memory_page");
	    if (!ctx) { 
		PrintError("Cannot open context to receive memory page\n");
		return -1;
	    }
	    
            if (v3_chkpt_load(ctx, 
			      "memory_page", 
			      page_size_bytes,
			      guest_mem_base + (page_size_bytes * i)) == -1) {
		PrintError("Did not receive all of memory page\n");
		v3_chkpt_close_ctx(ctx);
		return -1;
	    }
            v3_chkpt_close_ctx(ctx);
        }
    } 
    
    if (empty_bitmap) {
        // signal end of receiving pages
        PrintDebug("Finished receiving pages.\n");
	return 1;
    } else {
	// need to run again
	return 0;
    }

}

#endif

int save_header(struct v3_vm_info * vm, struct v3_chkpt * chkpt) {
    extern v3_cpu_arch_t v3_mach_type;
    void * ctx = NULL;
    
    ctx = v3_chkpt_open_ctx(chkpt, NULL, "header");
    if (!ctx) { 
	PrintError("Cannot open context to save header\n");
	return -1;
    }

    switch (v3_mach_type) {
	case V3_SVM_CPU:
	case V3_SVM_REV3_CPU: {
	    if (v3_chkpt_save(ctx, "header", strlen(svm_chkpt_header), svm_chkpt_header) == -1) { 
		PrintError("Could not save all of SVM header\n");
		v3_chkpt_close_ctx(ctx);
		return -1;
	    }
	    break;
	}
	case V3_VMX_CPU:
	case V3_VMX_EPT_CPU:
	case V3_VMX_EPT_UG_CPU: {
	    if (v3_chkpt_save(ctx, "header", strlen(vmx_chkpt_header), vmx_chkpt_header) == -1) { 
		PrintError("Could not save all of VMX header\n");
		v3_chkpt_close_ctx(ctx);
		return -1;
	    }
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
    extern v3_cpu_arch_t v3_mach_type;
    void * ctx = NULL;
    
    ctx = v3_chkpt_open_ctx(chkpt, NULL, "header");

    switch (v3_mach_type) {
	case V3_SVM_CPU:
	case V3_SVM_REV3_CPU: {
	    char header[strlen(svm_chkpt_header) + 1];
	 
	    if (v3_chkpt_load(ctx, "header", strlen(svm_chkpt_header), header) == -1) {
		PrintError("Could not load all of SVM header\n");
		v3_chkpt_close_ctx(ctx);
		return -1;
	    }
	    
	    header[strlen(svm_chkpt_header)] = 0;

	    break;
	}
	case V3_VMX_CPU:
	case V3_VMX_EPT_CPU:
	case V3_VMX_EPT_UG_CPU: {
	    char header[strlen(vmx_chkpt_header) + 1];
	    
	    if (v3_chkpt_load(ctx, "header", strlen(vmx_chkpt_header), header) == -1) {
		PrintError("Could not load all of VMX header\n");
		v3_chkpt_close_ctx(ctx);
		return -1;
	    }
	    
	    header[strlen(vmx_chkpt_header)] = 0;
	    
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
    extern v3_cpu_arch_t v3_mach_type;
    void * ctx = NULL;
    char key_name[16];

    memset(key_name, 0, 16);

    snprintf(key_name, 16, "guest_info%d", info->vcpu_id);

    ctx = v3_chkpt_open_ctx(chkpt, NULL, key_name);

    if (!ctx) { 
	PrintError("Could not open context to load core\n");
	return -1;
    }

    // These really need to have error checking

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


    switch (v3_mach_type) {
	case V3_SVM_CPU:
	case V3_SVM_REV3_CPU: {
	    char key_name[16];

	    snprintf(key_name, 16, "vmcb_data%d", info->vcpu_id);
	    ctx = v3_chkpt_open_ctx(chkpt, NULL, key_name);

	    if (!ctx) { 
		PrintError("Could not open context to load SVM core\n");
		return -1;
	    }
	    
	    if (v3_svm_load_core(info, ctx) == -1) {
		PrintError("Failed to patch core %d\n", info->vcpu_id);
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

	    snprintf(key_name, 16, "vmcs_data%d", info->vcpu_id);

	    ctx = v3_chkpt_open_ctx(chkpt, NULL, key_name);

	    if (!ctx) { 
		PrintError("Could not open context to load VMX core\n");
		return -1;
	    }
	    
	    if (v3_vmx_load_core(info, ctx) < 0) {
		PrintError("VMX checkpoint failed\n");
		v3_chkpt_close_ctx(ctx);
		return -1;
	    }

	    v3_chkpt_close_ctx(ctx);

	    break;
	}
	default:
	    PrintError("Invalid CPU Type (%d)\n", v3_mach_type);
	    return -1;
    }

    v3_print_guest_state(info);

    return 0;
}


static int save_core(struct guest_info * info, struct v3_chkpt * chkpt) {
    extern v3_cpu_arch_t v3_mach_type;
    void * ctx = NULL;
    char key_name[16];

    memset(key_name, 0, 16);

    v3_print_guest_state(info);


    snprintf(key_name, 16, "guest_info%d", info->vcpu_id);

    ctx = v3_chkpt_open_ctx(chkpt, NULL, key_name);
    
    if (!ctx) { 
	PrintError("Unable to open context to save core\n");
	return -1;
    }


    // Error checking of all this needs to happen
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
    switch (v3_mach_type) {
	case V3_SVM_CPU:
	case V3_SVM_REV3_CPU: {
	    char key_name[16];
	    void * ctx = NULL;
	    
	    snprintf(key_name, 16, "vmcb_data%d", info->vcpu_id);
	    
	    ctx = v3_chkpt_open_ctx(chkpt, NULL, key_name);

	    if (!ctx) { 
		PrintError("Could not open context to store SVM core\n");
		return -1;
	    }
	    
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
	    
	    if (!ctx) { 
		PrintError("Could not open context to store VMX core\n");
		return -1;
	    }

	    if (v3_vmx_save_core(info, ctx) == -1) {
		PrintError("VMX checkpoint failed\n");
		v3_chkpt_close_ctx(ctx);
		return -1;
	    }

	    v3_chkpt_close_ctx(ctx);

	    break;
	}
	default:
	    PrintError("Invalid CPU Type (%d)\n", v3_mach_type);
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
	PrintError("Error creating checkpoint store for url %s\n",url);
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


#ifdef V3_CONFIG_LIVE_MIGRATION

#define MOD_THRESHOLD   200  // pages below which we declare victory
#define ITER_THRESHOLD  32   // iters below which we declare victory



int v3_chkpt_send_vm(struct v3_vm_info * vm, char * store, char * url) {
    struct v3_chkpt * chkpt = NULL;
    int ret = 0;;
    int iter = 0;
    bool last_modpage_iteration=false;
    struct v3_bitmap modified_pages_to_send;
    uint64_t start_time;
    uint64_t stop_time;
    int num_mod_pages=0;
    struct mem_migration_state *mm_state;
    int i;

    // Currently will work only for shadow paging
    for (i=0;i<vm->num_cores;i++) { 
	if (vm->cores[i].shdw_pg_mode!=SHADOW_PAGING) { 
	    PrintError("Cannot currently handle nested paging\n");
	    return -1;
	}
    }
    
    
    chkpt = chkpt_open(vm, store, url, SAVE);
    
    if (chkpt == NULL) {
	PrintError("Error creating checkpoint store\n");
	chkpt_close(chkpt);
	return -1;
    }
    
    // In a send, the memory is copied incrementally first,
    // followed by the remainder of the state
    
    if (v3_bitmap_init(&modified_pages_to_send,
		       vm->mem_size>>12 // number of pages in main region
		       ) == -1) {
        PrintError("Could not intialize bitmap.\n");
        return -1;
    }

    // 0. Initialize bitmap to all 1s
    for (i=0; i < modified_pages_to_send.num_bits; i++) {
        v3_bitmap_set(&modified_pages_to_send,i);
    }

    iter = 0;
    while (!last_modpage_iteration) {
        PrintDebug("Modified memory page iteration %d\n",i++);
        
        start_time = v3_get_host_time(&(vm->cores[0].time_state));
        
	// We will pause the VM for a short while
	// so that we can collect the set of changed pages
        if (v3_pause_vm(vm) == -1) {
            PrintError("Could not pause VM\n");
            ret = -1;
            goto out;
        }
        
	if (iter==0) { 
	    // special case, we already have the pages to send (all of them)
	    // they are already in modified_pages_to_send
	} else {
	    // normally, we are in the middle of a round
	    // We need to copy from the current tracking bitmap
	    // to our send bitmap
	    v3_bitmap_copy(&modified_pages_to_send,&(mm_state->modified_pages));
	    // and now we need to remove our tracking
	    stop_page_tracking(mm_state);
	}

	// are we done? (note that we are still paused)
        num_mod_pages = v3_bitmap_count(&modified_pages_to_send);
	if (num_mod_pages<MOD_THRESHOLD || iter>ITER_THRESHOLD) {
	    // we are done, so we will not restart page tracking
	    // the vm is paused, and so we should be able
	    // to just send the data
            PrintDebug("Last modified memory page iteration.\n");
            last_modpage_iteration = true;
	} else {
	    // we are not done, so we will restart page tracking
	    // to prepare for a second round of pages
	    // we will resume the VM as this happens
	    if (!(mm_state=start_page_tracking(vm))) { 
		PrintError("Error enabling page tracking.\n");
		ret = -1;
		goto out;
	    }
            if (v3_continue_vm(vm) == -1) {
                PrintError("Error resuming the VM\n");
		stop_page_tracking(mm_state);
                ret = -1;
                goto out;
            }
	    
            stop_time = v3_get_host_time(&(vm->cores[0].time_state));
            PrintDebug("num_mod_pages=%d\ndowntime=%llu\n",num_mod_pages,stop_time-start_time);
        }
	

	// At this point, we are either paused and about to copy
	// the last chunk, or we are running, and will copy the last
	// round in parallel with current execution
	if (num_mod_pages>0) { 
	    if (save_inc_memory(vm, &modified_pages_to_send, chkpt) == -1) {
		PrintError("Error sending incremental memory.\n");
		ret = -1;
		goto out;
	    }
	} // we don't want to copy an empty bitmap here
	
	iter++;
    }        
    
    if (v3_bitmap_reset(&modified_pages_to_send) == -1) {
        PrintError("Error reseting bitmap.\n");
        ret = -1;
        goto out;
    }    
    
    // send bitmap of 0s to signal end of modpages
    if (save_inc_memory(vm, &modified_pages_to_send, chkpt) == -1) {
        PrintError("Error sending incremental memory.\n");
        ret = -1;
        goto out;
    }
    
    // save the non-memory state
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
    
    stop_time = v3_get_host_time(&(vm->cores[0].time_state));
    PrintDebug("num_mod_pages=%d\ndowntime=%llu\n",num_mod_pages,stop_time-start_time);
    PrintDebug("Done sending VM!\n"); 
 out:
    v3_bitmap_deinit(&modified_pages_to_send);
    chkpt_close(chkpt);
    
    return ret;

}

int v3_chkpt_receive_vm(struct v3_vm_info * vm, char * store, char * url) {
    struct v3_chkpt * chkpt = NULL;
    int i = 0;
    int ret = 0;
    struct v3_bitmap mod_pgs;
 
    // Currently will work only for shadow paging
    for (i=0;i<vm->num_cores;i++) { 
	if (vm->cores[i].shdw_pg_mode!=SHADOW_PAGING) { 
	    PrintError("Cannot currently handle nested paging\n");
	    return -1;
	}
    }
    
    chkpt = chkpt_open(vm, store, url, LOAD);
    
    if (chkpt == NULL) {
	PrintError("Error creating checkpoint store\n");
	chkpt_close(chkpt);
	return -1;
    }
    
    if (v3_bitmap_init(&mod_pgs,vm->mem_size>>12) == -1) {
	chkpt_close(chkpt);
        PrintError("Could not intialize bitmap.\n");
        return -1;
    }
    
    /* If this guest is running we need to block it while the checkpoint occurs */
    if (vm->run_state == VM_RUNNING) {
	while (v3_raise_barrier(vm, NULL) == -1);
    }
    
    i = 0;
    while(true) {
        // 1. Receive copy of bitmap
        // 2. Receive pages
        PrintDebug("Memory page iteration %d\n",i++);
        int retval = load_inc_memory(vm, &mod_pgs, chkpt);
        if (retval == 1) {
            // end of receiving memory pages
            break;        
        } else if (retval == -1) {
            PrintError("Error receiving incremental memory.\n");
            ret = -1;
            goto out;
	}
    }        
    
    if ((ret = v3_load_vm_devices(vm, chkpt)) == -1) {
	PrintError("Unable to load devices\n");
	ret = -1;
	goto out;
    }
    
    
    if ((ret = load_header(vm, chkpt)) == -1) {
	PrintError("Unable to load header\n");
	ret = -1;
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
    if (ret==-1) { 
	PrintError("Unable to receive VM\n");
    } else {
	PrintDebug("Done receving the VM\n");
    }
	
	
    /* Resume the guest if it was running and we didn't just trash the state*/
    if (vm->run_state == VM_RUNNING) { 
	if (ret == -1) {
	    PrintError("VM was previously running.  It is now borked.  Pausing it. \n");
	    vm->run_state = VM_STOPPED;
	}
	    
	/* We check the run state of the VM after every barrier 
	   So this will immediately halt the VM 
	*/
	v3_lower_barrier(vm);
    } 
    
    v3_bitmap_deinit(&mod_pgs);
    chkpt_close(chkpt);

    return ret;
}

#endif
