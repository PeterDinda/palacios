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
 *         Peter Dinda <pdinda@northwestern.edu> (store interface changes)
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
  // Opening a checkpoint should return a pointer to the internal representation
  // of the checkpoint in the store.  This will be passed back
  // as "store_data".  Return NULL if the context cannot be opened
  void * (*open_chkpt)(char * url, chkpt_mode_t mode);
  // Closing the checkpoint should return -1 on failure, 0 on success
  int    (*close_chkpt)(void * store_data);
  
  // Opening a context on the checkpoint with a given name should return
  // a pointer to an internal representation of the context.  This pointer
  // is then passed back as "ctx". 
  // We will open only a single context at a time.  
  void * (*open_ctx)(void * store_data, char *name);
  // Closing the context should return -1 on failure, 0 on success
  int    (*close_ctx)(void * store_data, void * ctx);
  
  // Save and load include a tagged data buffer.  These are 
  // "all or nothing" writes and reads.  
  // return -1 on failure, and 0 on success
  // 
  int (*save)(void * store_data, void * ctx, char * tag, uint64_t len, void * buf);
  int (*load)(void * store_data, void * ctx, char * tag, uint64_t len, void * buf);
};


struct v3_chkpt {
  struct v3_vm_info * vm;
  
  struct v3_chkpt_ctx *current_ctx;
  
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
	V3_Print(VM_NONE, VCORE_NONE, "Registering Checkpoint Backing Store (%s)\n", (*tmp_store)->name);

	if (v3_htable_search(store_table, (addr_t)((*tmp_store)->name))) {
	    PrintError(VM_NONE, VCORE_NONE, "Multiple instances of Checkpoint backing Store (%s)\n", (*tmp_store)->name);
	    return -1;
	}

	if (v3_htable_insert(store_table, (addr_t)((*tmp_store)->name), (addr_t)(*tmp_store)) == 0) {
	    PrintError(VM_NONE, VCORE_NONE, "Could not register Checkpoint backing store (%s)\n", (*tmp_store)->name);
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
  if (chkpt) { 
    int rc;

    rc = chkpt->interface->close_chkpt(chkpt->store_data);

    V3_Free(chkpt);

    if (rc!=0) { 
      PrintError(VM_NONE, VCORE_NONE, "Internal store failed to close valid checkpoint\n");
      return -1;
    } else {
      return 0;
    }
  } else {
    PrintError(VM_NONE, VCORE_NONE, "Attempt to close null checkpoint\n");
    return -1;
  }
}


static struct v3_chkpt * chkpt_open(struct v3_vm_info * vm, char * store, char * url, chkpt_mode_t mode) {
    struct chkpt_interface * iface = NULL;
    struct v3_chkpt * chkpt = NULL;
    void * store_data = NULL;

    iface = (void *)v3_htable_search(store_table, (addr_t)store);
    
    if (iface == NULL) {
	V3_Print(vm, VCORE_NONE, "Error: Could not locate Checkpoint interface for store (%s)\n", store);
	return NULL;
    }

    store_data = iface->open_chkpt(url, mode);

    if (store_data == NULL) {
	PrintError(vm, VCORE_NONE, "Could not open url (%s) for backing store (%s)\n", url, store);
	return NULL;
    }


    chkpt = V3_Malloc(sizeof(struct v3_chkpt));
    
    if (!chkpt) {
	PrintError(vm, VCORE_NONE, "Could not allocate checkpoint state, closing checkpoint\n");
	iface->close_chkpt(store_data);
	return NULL;
    }

    memset(chkpt,0,sizeof(struct v3_chkpt));

    chkpt->interface = iface;
    chkpt->vm = vm;
    chkpt->store_data = store_data;
    chkpt->current_ctx = NULL;
    
    return chkpt;
}

struct v3_chkpt_ctx * v3_chkpt_open_ctx(struct v3_chkpt * chkpt, char * name) {
  struct v3_chkpt_ctx * ctx;

  if (chkpt->current_ctx) { 
    PrintError(VM_NONE, VCORE_NONE, "Attempt to open context %s before old context has been closed\n", name);
    return NULL;
  }

  ctx = V3_Malloc(sizeof(struct v3_chkpt_ctx));

  if (!ctx) { 
    PrintError(VM_NONE, VCORE_NONE, "Unable to allocate context\n");
    return 0;
  }
  
  memset(ctx, 0, sizeof(struct v3_chkpt_ctx));
  
  ctx->chkpt = chkpt;
  ctx->store_ctx = chkpt->interface->open_ctx(chkpt->store_data, name);

  if (!(ctx->store_ctx)) {
    PrintError(VM_NONE, VCORE_NONE, "Underlying store failed to open context %s\n",name);
    V3_Free(ctx);
    return NULL;
  }

  chkpt->current_ctx = ctx;

  return ctx;
}

int v3_chkpt_close_ctx(struct v3_chkpt_ctx * ctx) {
    struct v3_chkpt * chkpt = ctx->chkpt;
    int ret = 0;

    if (chkpt->current_ctx != ctx) { 
      PrintError(VM_NONE, VCORE_NONE, "Attempt to close a context that is not the current context on the store\n");
      return -1;
    }

    ret = chkpt->interface->close_ctx(chkpt->store_data, ctx->store_ctx);

    if (ret) { 
      PrintError(VM_NONE, VCORE_NONE, "Failed to close context on store, closing device-independent context anyway - bad\n");
      ret = -1;
    }

    chkpt->current_ctx=NULL;

    V3_Free(ctx);

    return ret;
}





int v3_chkpt_save(struct v3_chkpt_ctx * ctx, char * tag, uint64_t len, void * buf) {
    struct v3_chkpt * chkpt = ctx->chkpt;    
    int rc;

    if (!ctx) { 
      PrintError(VM_NONE, VCORE_NONE, "Attempt to save tag %s on null context\n",tag);
      return -1;
    }

    if (chkpt->current_ctx != ctx) { 
      PrintError(VM_NONE, VCORE_NONE, "Attempt to save on context that is not the current context for the store\n");
      return -1;
    }

    rc = chkpt->interface->save(chkpt->store_data, ctx->store_ctx, tag , len, buf);

    if (rc) { 
      PrintError(VM_NONE, VCORE_NONE, "Underlying store failed to save tag %s on valid context\n",tag);
      return -1;
    } else {
      return 0;
    }
}


int v3_chkpt_load(struct v3_chkpt_ctx * ctx, char * tag, uint64_t len, void * buf) {
    struct v3_chkpt * chkpt = ctx->chkpt;    
    int rc;

    if (!ctx) { 
      PrintError(VM_NONE, VCORE_NONE, "Attempt to load tag %s from null context\n",tag);
      return -1;
    }
    
    if (chkpt->current_ctx != ctx) { 
      PrintError(VM_NONE, VCORE_NONE, "Attempt to load from context that is not the current context for the store\n");
      return -1;
    }

    rc = chkpt->interface->load(chkpt->store_data, ctx->store_ctx, tag, len, buf);

    if (rc) { 
      PrintError(VM_NONE, VCORE_NONE, "Underlying store failed to load tag %s from valid context\n",tag);
      return -1;
    } else {
      return 0;
    }
}



static int load_memory(struct v3_vm_info * vm, struct v3_chkpt * chkpt) {

    void * guest_mem_base = NULL;
    void * ctx = NULL;
    uint64_t ret = 0;
    uint64_t saved_mem_block_size;
    uint32_t saved_num_base_regions;
    char buf[128];
    int i;
    extern uint64_t v3_mem_block_size;

    ctx = v3_chkpt_open_ctx(chkpt, "memory_img");
    
    if (!ctx) { 
	PrintError(vm, VCORE_NONE, "Unable to open context for memory load\n");
	return -1;
    }
		     
    if (V3_CHKPT_LOAD(ctx, "region_size",saved_mem_block_size)) { 
	PrintError(vm, VCORE_NONE, "Unable to load memory region size\n");
	return -1;
    }
    
    if (V3_CHKPT_LOAD(ctx, "num_regions",saved_num_base_regions)) {
	PrintError(vm, VCORE_NONE, "Unable to load number of regions\n");
	return -1;
    }

    if (saved_mem_block_size != v3_mem_block_size) { 
	PrintError(vm, VCORE_NONE, "Unable to load as memory block size differs\n");
	return -1;
    } // support will eventually be added for this

    if (saved_num_base_regions != vm->mem_map.num_base_regions) { 
	PrintError(vm, VCORE_NONE, "Unable to laod as number of base regions differs\n");
	return -1;
    } // support will eventually be added for this

    for (i=0;i<vm->mem_map.num_base_regions;i++) {
	guest_mem_base = V3_VAddr((void *)vm->mem_map.base_regions[i].host_addr);
	sprintf(buf,"memory_img%d",i);
	if (v3_chkpt_load(ctx, buf, v3_mem_block_size, guest_mem_base)) {
	    PrintError(vm, VCORE_NONE, "Unable to load all of memory (region %d) (requested=%llu bytes, result=%llu bytes\n",i,(uint64_t)(vm->mem_size),ret);
	    v3_chkpt_close_ctx(ctx);
	    return -1;
	}
    }
    
    v3_chkpt_close_ctx(ctx);

    return 0;
}


static int save_memory(struct v3_vm_info * vm, struct v3_chkpt * chkpt) {
    void * guest_mem_base = NULL;
    void * ctx = NULL;
    char buf[128]; // region name
    uint64_t ret = 0;
    extern uint64_t v3_mem_block_size;
    int i;


    ctx = v3_chkpt_open_ctx(chkpt, "memory_img");

    if (!ctx) { 
	PrintError(vm, VCORE_NONE, "Unable to open context to save memory\n");
	return -1;
    }

    if (V3_CHKPT_SAVE(ctx, "region_size",v3_mem_block_size)) { 
	PrintError(vm, VCORE_NONE, "Unable to save memory region size\n");
	return -1;
    }

    if (V3_CHKPT_SAVE(ctx, "num_regions",vm->mem_map.num_base_regions)) {
	PrintError(vm, VCORE_NONE, "Unable to save number of regions\n");
	return -1;
    }

    for (i=0;i<vm->mem_map.num_base_regions;i++) {
	guest_mem_base = V3_VAddr((void *)vm->mem_map.base_regions[i].host_addr);
	sprintf(buf,"memory_img%d",i);
	if (v3_chkpt_save(ctx, buf, v3_mem_block_size, guest_mem_base)) {
	    PrintError(vm, VCORE_NONE, "Unable to save all of memory (region %d) (requested=%llu, received=%llu)\n",i,(uint64_t)(vm->mem_size),ret);
	    v3_chkpt_close_ctx(ctx);  
	    return -1;
	}
    }

    v3_chkpt_close_ctx(ctx);

    return 0;
}

#ifdef V3_CONFIG_LIVE_MIGRATION

struct mem_migration_state {
    struct v3_vm_info *vm;
    struct v3_bitmap  modified_pages; 
};

static int shadow_paging_callback(struct guest_info *core, 
				  struct v3_shdw_pg_event *event,
				  void      *priv_data)
{
    struct mem_migration_state *m = (struct mem_migration_state *)priv_data;
    
    if (event->event_type==SHADOW_PAGEFAULT &&
	event->event_order==SHADOW_PREIMPL &&
	event->error_code.write) { // Note, assumes VTLB behavior where we will see the write even if preceded by a read
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


/*
static int nested_paging_callback(struct guest_info *core, 
				  struct v3_nested_pg_event *event,
				  void      *priv_data)
{
    struct mem_migration_state *m = (struct mem_migration_state *)priv_data;
    
    if (event->event_type==NESTED_PAGEFAULT &&
	event->event_order==NESTED_PREIMPL &&
	event->error_code.write) { // Assumes we will see a write after reads
	if (event->gpa<core->vm_info->mem_size) { 
	  v3_bitmap_set(&(m->modified_pages),(event->gpa)>>12);
	} else {
	  // no worries, this isn't physical memory
	}
    } else {
      // we don't care about other events
    }
    
    return 0;
}
*/	


static struct mem_migration_state *start_page_tracking(struct v3_vm_info *vm)
{
    struct mem_migration_state *m;
    int i;

    m = (struct mem_migration_state *)V3_Malloc(sizeof(struct mem_migration_state));

    if (!m) { 
	PrintError(vm, VCORE_NONE, "Cannot allocate\n");
	return NULL;
    }

    m->vm=vm;
    
    if (v3_bitmap_init(&(m->modified_pages),vm->mem_size >> 12) == -1) { 
	PrintError(vm, VCORE_NONE, "Failed to initialize modified_pages bit vector");
	V3_Free(m);
    }

    // We assume that the migrator has already verified that all cores are
    // using the identical model (shadow or nested)
    // This must not change over the execution of the migration

    if (vm->cores[0].shdw_pg_mode==SHADOW_PAGING) { 
      v3_register_shadow_paging_event_callback(vm,shadow_paging_callback,m);

      for (i=0;i<vm->num_cores;i++) {
	v3_invalidate_shadow_pts(&(vm->cores[i]));
      }
    } else if (vm->cores[0].shdw_pg_mode==NESTED_PAGING) { 
      //v3_register_nested_paging_event_callback(vm,nested_paging_callback,m);
      
      for (i=0;i<vm->num_cores;i++) {
	//v3_invalidate_nested_addr_range(&(vm->cores[i]),0,vm->mem_size-1);
      }
    } else {
      PrintError(vm, VCORE_NONE, "Unsupported paging mode\n");
      v3_bitmap_deinit(&(m->modified_pages));
      V3_Free(m);
      return 0;
    }
    
    // and now we should get callbacks as writes happen

    return m;
}

static void stop_page_tracking(struct mem_migration_state *m)
{
  if (m->vm->cores[0].shdw_pg_mode==SHADOW_PAGING) { 
    v3_unregister_shadow_paging_event_callback(m->vm,shadow_paging_callback,m);
  } else {
    //v3_unregister_nested_paging_event_callback(m->vm,nested_paging_callback,m);
  }
    
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
    int bitmap_num_bytes = (mod_pgs_to_send->num_bits / 8) 
                           + ((mod_pgs_to_send->num_bits % 8) > 0);

   
    PrintDebug(vm, VCORE_NONE, "Saving incremental memory.\n");

    ctx = v3_chkpt_open_ctx(chkpt,"memory_bitmap_bits");

    if (!ctx) { 
	PrintError(vm, VCORE_NONE, "Cannot open context for dirty memory bitmap\n");
	return -1;
    }
	

    if (v3_chkpt_save(ctx,
		      "memory_bitmap_bits",
		      bitmap_num_bytes,
		      mod_pgs_to_send->bits)) {
	PrintError(vm, VCORE_NONE, "Unable to write all of the dirty memory bitmap\n");
	v3_chkpt_close_ctx(ctx);
	return -1;
    }

    v3_chkpt_close_ctx(ctx);

    PrintDebug(vm, VCORE_NONE, "Sent bitmap bits.\n");

    // Dirty memory pages are sent in bitmap order
    for (i = 0; i < mod_pgs_to_send->num_bits; i++) {
        if (v3_bitmap_check(mod_pgs_to_send, i)) {
	    struct v3_mem_region *region = v3_get_base_region(vm,page_size_bytes * i);
	    if (!region) { 
		PrintError(vm, VCORE_NONE, "Failed to find base region for page %d\n",i);
		return -1;
	    }
	    // PrintDebug(vm, VCORE_NONE, "Sending memory page %d.\n",i);
            ctx = v3_chkpt_open_ctx(chkpt, "memory_page");
	    if (!ctx) { 
		PrintError(vm, VCORE_NONE, "Unable to open context to send memory page\n");
		return -1;
	    }
            if (v3_chkpt_save(ctx, 
			      "memory_page", 
			      page_size_bytes,
			      (void*)(region->host_addr + page_size_bytes * i - region->guest_start))) {
		PrintError(vm, VCORE_NONE, "Unable to send a memory page\n");
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
    bool empty_bitmap = true;
    int bitmap_num_bytes = (mod_pgs->num_bits / 8) 
                           + ((mod_pgs->num_bits % 8) > 0);


    ctx = v3_chkpt_open_ctx(chkpt, "memory_bitmap_bits");

    if (!ctx) { 
	PrintError(vm, VCORE_NONE, "Cannot open context to receive memory bitmap\n");
	return -1;
    }

    if (v3_chkpt_load(ctx,
		      "memory_bitmap_bits",
		      bitmap_num_bytes,
		      mod_pgs->bits)) {
	PrintError(vm, VCORE_NONE, "Did not receive all of memory bitmap\n");
	v3_chkpt_close_ctx(ctx);
	return -1;
    }
    
    v3_chkpt_close_ctx(ctx);

    // Receive also follows bitmap order
    for (i = 0; i < mod_pgs->num_bits; i ++) {
        if (v3_bitmap_check(mod_pgs, i)) {
	    struct v3_mem_region *region = v3_get_base_region(vm,page_size_bytes * i);
	    if (!region) { 
		PrintError(vm, VCORE_NONE, "Failed to find base region for page %d\n",i);
		return -1;
	    }
            //PrintDebug(vm, VCORE_NONE, "Loading page %d\n", i);
            empty_bitmap = false;
            ctx = v3_chkpt_open_ctx(chkpt, "memory_page");
	    if (!ctx) { 
		PrintError(vm, VCORE_NONE, "Cannot open context to receive memory page\n");
		return -1;
	    }
	    
            if (v3_chkpt_load(ctx, 
			      "memory_page", 
			      page_size_bytes,
			      (void*)(region->host_addr + page_size_bytes * i - region->guest_start))) {
		PrintError(vm, VCORE_NONE, "Did not receive all of memory page\n");
		v3_chkpt_close_ctx(ctx);
		return -1;
	    }
            v3_chkpt_close_ctx(ctx);
        }
    } 
    
    if (empty_bitmap) {
        // signal end of receiving pages
        PrintDebug(vm, VCORE_NONE, "Finished receiving pages.\n");
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
    
    ctx = v3_chkpt_open_ctx(chkpt, "header");
    if (!ctx) { 
	PrintError(vm, VCORE_NONE, "Cannot open context to save header\n");
	return -1;
    }

    switch (v3_mach_type) {
	case V3_SVM_CPU:
	case V3_SVM_REV3_CPU: {
	    if (v3_chkpt_save(ctx, "header", strlen(svm_chkpt_header), svm_chkpt_header)) { 
		PrintError(vm, VCORE_NONE, "Could not save all of SVM header\n");
		v3_chkpt_close_ctx(ctx);
		return -1;
	    }
	    break;
	}
	case V3_VMX_CPU:
	case V3_VMX_EPT_CPU:
	case V3_VMX_EPT_UG_CPU: {
	    if (v3_chkpt_save(ctx, "header", strlen(vmx_chkpt_header), vmx_chkpt_header)) { 
		PrintError(vm, VCORE_NONE, "Could not save all of VMX header\n");
		v3_chkpt_close_ctx(ctx);
		return -1;
	    }
	    break;
	}
	default:
	    PrintError(vm, VCORE_NONE, "checkpoint not supported on this architecture\n");
	    v3_chkpt_close_ctx(ctx);
	    return -1;
    }

    v3_chkpt_close_ctx(ctx);
	    
    return 0;
}

static int load_header(struct v3_vm_info * vm, struct v3_chkpt * chkpt) {
    extern v3_cpu_arch_t v3_mach_type;
    void * ctx = NULL;
    
    ctx = v3_chkpt_open_ctx(chkpt, "header");

    if (!ctx) { 
	PrintError(vm, VCORE_NONE, "Cannot open context to load header\n");
        return -1;
    }

    switch (v3_mach_type) {
	case V3_SVM_CPU:
	case V3_SVM_REV3_CPU: {
	    char header[strlen(svm_chkpt_header) + 1];
	 
	    if (v3_chkpt_load(ctx, "header", strlen(svm_chkpt_header), header)) {
		PrintError(vm, VCORE_NONE, "Could not load all of SVM header\n");
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
	    
	    if (v3_chkpt_load(ctx, "header", strlen(vmx_chkpt_header), header)) {
		PrintError(vm, VCORE_NONE, "Could not load all of VMX header\n");
		v3_chkpt_close_ctx(ctx);
		return -1;
	    }
	    
	    header[strlen(vmx_chkpt_header)] = 0;
	    
	    break;
	}
	default:
	    PrintError(vm, VCORE_NONE, "checkpoint not supported on this architecture\n");
	    v3_chkpt_close_ctx(ctx);
	    return -1;
    }
    
    v3_chkpt_close_ctx(ctx);
    
    return 0;
}


static int load_core(struct guest_info * info, struct v3_chkpt * chkpt, v3_chkpt_options_t opts) {
    extern v3_cpu_arch_t v3_mach_type;
    void * ctx = NULL;
    char key_name[16];
    v3_reg_t tempreg;

    PrintDebug(info->vm_info, info, "Loading core\n");

    memset(key_name, 0, 16);

    snprintf(key_name, 16, "guest_info%d", info->vcpu_id);

    ctx = v3_chkpt_open_ctx(chkpt, key_name);

    if (!ctx) { 
	PrintError(info->vm_info, info, "Could not open context to load core\n");
	goto loadfailout;
    }
    
    // Run state is needed to determine when AP cores need
    // to be immediately run after resume
    V3_CHKPT_LOAD(ctx,"run_state",info->core_run_state,loadfailout);

    V3_CHKPT_LOAD(ctx, "RIP", info->rip, loadfailout);
    
    // GPRs
    V3_CHKPT_LOAD(ctx,"RDI",info->vm_regs.rdi, loadfailout); 
    V3_CHKPT_LOAD(ctx,"RSI",info->vm_regs.rsi, loadfailout); 
    V3_CHKPT_LOAD(ctx,"RBP",info->vm_regs.rbp, loadfailout); 
    V3_CHKPT_LOAD(ctx,"RSP",info->vm_regs.rsp, loadfailout); 
    V3_CHKPT_LOAD(ctx,"RBX",info->vm_regs.rbx, loadfailout); 
    V3_CHKPT_LOAD(ctx,"RDX",info->vm_regs.rdx, loadfailout); 
    V3_CHKPT_LOAD(ctx,"RCX",info->vm_regs.rcx, loadfailout); 
    V3_CHKPT_LOAD(ctx,"RAX",info->vm_regs.rax, loadfailout);
    V3_CHKPT_LOAD(ctx,"R8",info->vm_regs.r8, loadfailout);
    V3_CHKPT_LOAD(ctx,"R9",info->vm_regs.r9, loadfailout);
    V3_CHKPT_LOAD(ctx,"R10",info->vm_regs.r10, loadfailout);
    V3_CHKPT_LOAD(ctx,"R11",info->vm_regs.r11, loadfailout);
    V3_CHKPT_LOAD(ctx,"R12",info->vm_regs.r12, loadfailout);
    V3_CHKPT_LOAD(ctx,"R13",info->vm_regs.r13, loadfailout);
    V3_CHKPT_LOAD(ctx,"R14",info->vm_regs.r14, loadfailout);
    V3_CHKPT_LOAD(ctx,"R15",info->vm_regs.r15, loadfailout);

    // Control registers
    V3_CHKPT_LOAD(ctx, "CR0", info->ctrl_regs.cr0, loadfailout);
    // there is no CR1
    V3_CHKPT_LOAD(ctx, "CR2", info->ctrl_regs.cr2, loadfailout);
    V3_CHKPT_LOAD(ctx, "CR3", info->ctrl_regs.cr3, loadfailout);
    V3_CHKPT_LOAD(ctx, "CR4", info->ctrl_regs.cr4, loadfailout);
    // There are no CR5,6,7
    // CR8 is derived from apic_tpr
    tempreg = (info->ctrl_regs.apic_tpr >> 4) & 0xf;
    V3_CHKPT_LOAD(ctx, "CR8", tempreg, loadfailout);
    V3_CHKPT_LOAD(ctx, "APIC_TPR", info->ctrl_regs.apic_tpr, loadfailout);
    V3_CHKPT_LOAD(ctx, "RFLAGS", info->ctrl_regs.rflags, loadfailout);
    V3_CHKPT_LOAD(ctx, "EFER", info->ctrl_regs.efer, loadfailout);

    // Debug registers
    V3_CHKPT_LOAD(ctx, "DR0", info->dbg_regs.dr0, loadfailout);
    V3_CHKPT_LOAD(ctx, "DR1", info->dbg_regs.dr1, loadfailout);
    V3_CHKPT_LOAD(ctx, "DR2", info->dbg_regs.dr2, loadfailout);
    V3_CHKPT_LOAD(ctx, "DR3", info->dbg_regs.dr3, loadfailout);
    // there is no DR4 or DR5
    V3_CHKPT_LOAD(ctx, "DR6", info->dbg_regs.dr6, loadfailout);
    V3_CHKPT_LOAD(ctx, "DR7", info->dbg_regs.dr7, loadfailout);

    // Segment registers
    V3_CHKPT_LOAD(ctx, "CS", info->segments.cs, loadfailout);
    V3_CHKPT_LOAD(ctx, "DS", info->segments.ds, loadfailout);
    V3_CHKPT_LOAD(ctx, "ES", info->segments.es, loadfailout);
    V3_CHKPT_LOAD(ctx, "FS", info->segments.fs, loadfailout);
    V3_CHKPT_LOAD(ctx, "GS", info->segments.gs, loadfailout);
    V3_CHKPT_LOAD(ctx, "SS", info->segments.ss, loadfailout);
    V3_CHKPT_LOAD(ctx, "LDTR", info->segments.ldtr, loadfailout);
    V3_CHKPT_LOAD(ctx, "GDTR", info->segments.gdtr, loadfailout);
    V3_CHKPT_LOAD(ctx, "IDTR", info->segments.idtr, loadfailout);
    V3_CHKPT_LOAD(ctx, "TR", info->segments.tr, loadfailout);
    
    // several MSRs...
    V3_CHKPT_LOAD(ctx, "STAR", info->msrs.star, loadfailout);
    V3_CHKPT_LOAD(ctx, "LSTAR", info->msrs.lstar, loadfailout);
    V3_CHKPT_LOAD(ctx, "SFMASK", info->msrs.sfmask, loadfailout);
    V3_CHKPT_LOAD(ctx, "KERN_GS_BASE", info->msrs.kern_gs_base, loadfailout);
        
    // Some components of guest state captured in the shadow pager
    V3_CHKPT_LOAD(ctx, "GUEST_CR3", info->shdw_pg_state.guest_cr3, loadfailout);
    V3_CHKPT_LOAD(ctx, "GUEST_CR0", info->shdw_pg_state.guest_cr0, loadfailout);
    V3_CHKPT_LOAD(ctx, "GUEST_EFER", info->shdw_pg_state.guest_efer, loadfailout);

    // floating point
    if (v3_load_fp_state(ctx,info)) {
      goto loadfailout;
    }

    v3_chkpt_close_ctx(ctx); ctx=0;

    PrintDebug(info->vm_info, info, "Finished reading guest_info information\n");

    info->cpu_mode = v3_get_vm_cpu_mode(info);
    info->mem_mode = v3_get_vm_mem_mode(info);

    if (info->shdw_pg_mode == SHADOW_PAGING) {
	if (v3_get_vm_mem_mode(info) == VIRTUAL_MEM) {
	    if (v3_activate_shadow_pt(info) == -1) {
		PrintError(info->vm_info, info, "Failed to activate shadow page tables\n");
		goto loadfailout;
	    }
	} else {
	    if (v3_activate_passthrough_pt(info) == -1) {
		PrintError(info->vm_info, info, "Failed to activate passthrough page tables\n");
		goto loadfailout;
	    }
	}
    }


    if (opts & V3_CHKPT_OPT_SKIP_ARCHDEP) { 
      goto donearch;
    }

    switch (v3_mach_type) {
	case V3_SVM_CPU:
	case V3_SVM_REV3_CPU: {
	    char key_name[16];

	    snprintf(key_name, 16, "vmcb_data%d", info->vcpu_id);
	    ctx = v3_chkpt_open_ctx(chkpt, key_name);

	    if (!ctx) { 
		PrintError(info->vm_info, info, "Could not open context to load SVM core\n");
		goto loadfailout;
	    }
	    
	    if (v3_svm_load_core(info, ctx) < 0 ) {
		PrintError(info->vm_info, info, "Failed to patch core %d\n", info->vcpu_id);
		goto loadfailout;
	    }

	    v3_chkpt_close_ctx(ctx); ctx=0;

	    break;
	}
	case V3_VMX_CPU:
	case V3_VMX_EPT_CPU:
	case V3_VMX_EPT_UG_CPU: {
	    char key_name[16];

	    snprintf(key_name, 16, "vmcs_data%d", info->vcpu_id);

	    ctx = v3_chkpt_open_ctx(chkpt, key_name);

	    if (!ctx) { 
		PrintError(info->vm_info, info, "Could not open context to load VMX core\n");
		goto loadfailout;
	    }
	    
	    if (v3_vmx_load_core(info, ctx) < 0) {
		PrintError(info->vm_info, info, "VMX checkpoint failed\n");
		goto loadfailout;
	    }

	    v3_chkpt_close_ctx(ctx); ctx=0;

	    break;
	}
	default:
	    PrintError(info->vm_info, info, "Invalid CPU Type (%d)\n", v3_mach_type);
	    goto loadfailout;
    }

 donearch:

    PrintDebug(info->vm_info, info, "Load of core succeeded\n");

    v3_print_guest_state(info);

    return 0;

 loadfailout:
    PrintError(info->vm_info, info, "Failed to load core\n");
    if (ctx) { v3_chkpt_close_ctx(ctx);}
    return -1;

}

// GEM5 - Hypercall for initiating transfer to gem5 (checkpoint)

static int save_core(struct guest_info * info, struct v3_chkpt * chkpt, v3_chkpt_options_t opts) {
    extern v3_cpu_arch_t v3_mach_type;
    void * ctx = NULL;
    char key_name[16];
    v3_reg_t tempreg;

    PrintDebug(info->vm_info, info, "Saving core\n");

    v3_print_guest_state(info);

    memset(key_name, 0, 16);

    snprintf(key_name, 16, "guest_info%d", info->vcpu_id);

    ctx = v3_chkpt_open_ctx(chkpt, key_name);
    
    if (!ctx) { 
	PrintError(info->vm_info, info, "Unable to open context to save core\n");
	goto savefailout;
    }

    V3_CHKPT_SAVE(ctx,"run_state",info->core_run_state,savefailout);

    V3_CHKPT_SAVE(ctx, "RIP", info->rip, savefailout);
    
    // GPRs
    V3_CHKPT_SAVE(ctx,"RDI",info->vm_regs.rdi, savefailout); 
    V3_CHKPT_SAVE(ctx,"RSI",info->vm_regs.rsi, savefailout); 
    V3_CHKPT_SAVE(ctx,"RBP",info->vm_regs.rbp, savefailout); 
    V3_CHKPT_SAVE(ctx,"RSP",info->vm_regs.rsp, savefailout); 
    V3_CHKPT_SAVE(ctx,"RBX",info->vm_regs.rbx, savefailout); 
    V3_CHKPT_SAVE(ctx,"RDX",info->vm_regs.rdx, savefailout); 
    V3_CHKPT_SAVE(ctx,"RCX",info->vm_regs.rcx, savefailout); 
    V3_CHKPT_SAVE(ctx,"RAX",info->vm_regs.rax, savefailout);
    V3_CHKPT_SAVE(ctx,"R8",info->vm_regs.r8, savefailout);
    V3_CHKPT_SAVE(ctx,"R9",info->vm_regs.r9, savefailout);
    V3_CHKPT_SAVE(ctx,"R10",info->vm_regs.r10, savefailout);
    V3_CHKPT_SAVE(ctx,"R11",info->vm_regs.r11, savefailout);
    V3_CHKPT_SAVE(ctx,"R12",info->vm_regs.r12, savefailout);
    V3_CHKPT_SAVE(ctx,"R13",info->vm_regs.r13, savefailout);
    V3_CHKPT_SAVE(ctx,"R14",info->vm_regs.r14, savefailout);
    V3_CHKPT_SAVE(ctx,"R15",info->vm_regs.r15, savefailout);

    // Control registers
    V3_CHKPT_SAVE(ctx, "CR0", info->ctrl_regs.cr0, savefailout);
    // there is no CR1
    V3_CHKPT_SAVE(ctx, "CR2", info->ctrl_regs.cr2, savefailout);
    V3_CHKPT_SAVE(ctx, "CR3", info->ctrl_regs.cr3, savefailout);
    V3_CHKPT_SAVE(ctx, "CR4", info->ctrl_regs.cr4, savefailout);
    // There are no CR5,6,7
    // CR8 is derived from apic_tpr
    tempreg = (info->ctrl_regs.apic_tpr >> 4) & 0xf;
    V3_CHKPT_SAVE(ctx, "CR8", tempreg, savefailout);
    V3_CHKPT_SAVE(ctx, "APIC_TPR", info->ctrl_regs.apic_tpr, savefailout);
    V3_CHKPT_SAVE(ctx, "RFLAGS", info->ctrl_regs.rflags, savefailout);
    V3_CHKPT_SAVE(ctx, "EFER", info->ctrl_regs.efer, savefailout);

    // Debug registers
    V3_CHKPT_SAVE(ctx, "DR0", info->dbg_regs.dr0, savefailout);
    V3_CHKPT_SAVE(ctx, "DR1", info->dbg_regs.dr1, savefailout);
    V3_CHKPT_SAVE(ctx, "DR2", info->dbg_regs.dr2, savefailout);
    V3_CHKPT_SAVE(ctx, "DR3", info->dbg_regs.dr3, savefailout);
    // there is no DR4 or DR5
    V3_CHKPT_SAVE(ctx, "DR6", info->dbg_regs.dr6, savefailout);
    V3_CHKPT_SAVE(ctx, "DR7", info->dbg_regs.dr7, savefailout);

    // Segment registers
    V3_CHKPT_SAVE(ctx, "CS", info->segments.cs, savefailout);
    V3_CHKPT_SAVE(ctx, "DS", info->segments.ds, savefailout);
    V3_CHKPT_SAVE(ctx, "ES", info->segments.es, savefailout);
    V3_CHKPT_SAVE(ctx, "FS", info->segments.fs, savefailout);
    V3_CHKPT_SAVE(ctx, "GS", info->segments.gs, savefailout);
    V3_CHKPT_SAVE(ctx, "SS", info->segments.ss, savefailout);
    V3_CHKPT_SAVE(ctx, "LDTR", info->segments.ldtr, savefailout);
    V3_CHKPT_SAVE(ctx, "GDTR", info->segments.gdtr, savefailout);
    V3_CHKPT_SAVE(ctx, "IDTR", info->segments.idtr, savefailout);
    V3_CHKPT_SAVE(ctx, "TR", info->segments.tr, savefailout);
    
    // several MSRs...
    V3_CHKPT_SAVE(ctx, "STAR", info->msrs.star, savefailout);
    V3_CHKPT_SAVE(ctx, "LSTAR", info->msrs.lstar, savefailout);
    V3_CHKPT_SAVE(ctx, "SFMASK", info->msrs.sfmask, savefailout);
    V3_CHKPT_SAVE(ctx, "KERN_GS_BASE", info->msrs.kern_gs_base, savefailout);
        
    // Some components of guest state captured in the shadow pager
    V3_CHKPT_SAVE(ctx, "GUEST_CR3", info->shdw_pg_state.guest_cr3, savefailout);
    V3_CHKPT_SAVE(ctx, "GUEST_CR0", info->shdw_pg_state.guest_cr0, savefailout);
    V3_CHKPT_SAVE(ctx, "GUEST_EFER", info->shdw_pg_state.guest_efer, savefailout);

    // floating point
    if (v3_save_fp_state(ctx,info)) {
      goto savefailout;
    }

    v3_chkpt_close_ctx(ctx); ctx=0;

    if (opts & V3_CHKPT_OPT_SKIP_ARCHDEP) {
      goto donearch;
    }

    //Architechture specific code
    switch (v3_mach_type) {
	case V3_SVM_CPU:
	case V3_SVM_REV3_CPU: {
	    char key_name[16];
	    
	    snprintf(key_name, 16, "vmcb_data%d", info->vcpu_id);
	    
	    ctx = v3_chkpt_open_ctx(chkpt, key_name);

	    if (!ctx) { 
		PrintError(info->vm_info, info, "Could not open context to store SVM core\n");
		goto savefailout;
	    }
	    
	    if (v3_svm_save_core(info, ctx) < 0) {
		PrintError(info->vm_info, info, "VMCB Unable to be written\n");
		goto savefailout;
	    }
	    
	    v3_chkpt_close_ctx(ctx); ctx=0;;
	    break;
	}
	case V3_VMX_CPU:
	case V3_VMX_EPT_CPU:
	case V3_VMX_EPT_UG_CPU: {
	    char key_name[16];

	    snprintf(key_name, 16, "vmcs_data%d", info->vcpu_id);
	    
	    ctx = v3_chkpt_open_ctx(chkpt, key_name);
	    
	    if (!ctx) { 
		PrintError(info->vm_info, info, "Could not open context to store VMX core\n");
		goto savefailout;
	    }

	    if (v3_vmx_save_core(info, ctx) == -1) {
		PrintError(info->vm_info, info, "VMX checkpoint failed\n");
		goto savefailout;
	    }

	    v3_chkpt_close_ctx(ctx); ctx=0;

	    break;
	}
	default:
	    PrintError(info->vm_info, info, "Invalid CPU Type (%d)\n", v3_mach_type);
	    goto savefailout;
	    
    }

 donearch:
    
    return 0;

 savefailout:
    PrintError(info->vm_info, info, "Failed to save core\n");
    if (ctx) { v3_chkpt_close_ctx(ctx); }
    return -1;

}

//
// GEM5 - Madhav has debug code here for printing instrucions
//

int v3_chkpt_save_vm(struct v3_vm_info * vm, char * store, char * url, v3_chkpt_options_t opts) {
    struct v3_chkpt * chkpt = NULL;
    int ret = 0;;
    int i = 0;


    chkpt = chkpt_open(vm, store, url, SAVE);

    if (chkpt == NULL) {
	PrintError(vm, VCORE_NONE, "Error creating checkpoint store for url %s\n",url);
	return -1;
    }

    /* If this guest is running we need to block it while the checkpoint occurs */
    if (vm->run_state == VM_RUNNING) {
	while (v3_raise_barrier(vm, NULL) == -1);
    }

    if (!(opts & V3_CHKPT_OPT_SKIP_MEM)) {
      if ((ret = save_memory(vm, chkpt)) == -1) {
	PrintError(vm, VCORE_NONE, "Unable to save memory\n");
	goto out;
      }
    }
    
    
    if (!(opts & V3_CHKPT_OPT_SKIP_DEVS)) {
      if ((ret = v3_save_vm_devices(vm, chkpt)) == -1) {
	PrintError(vm, VCORE_NONE, "Unable to save devices\n");
	goto out;
      }
    }

    if ((ret = save_header(vm, chkpt)) == -1) {
	PrintError(vm, VCORE_NONE, "Unable to save header\n");
	goto out;
    }

    if (!(opts & V3_CHKPT_OPT_SKIP_CORES)) { 
      for (i = 0; i < vm->num_cores; i++){
	if ((ret = save_core(&(vm->cores[i]), chkpt, opts)) == -1) {
	  PrintError(vm, VCORE_NONE, "chkpt of core %d failed\n", i);
	  goto out;
	}
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

int v3_chkpt_load_vm(struct v3_vm_info * vm, char * store, char * url, v3_chkpt_options_t opts) {
    struct v3_chkpt * chkpt = NULL;
    int i = 0;
    int ret = 0;
    
    chkpt = chkpt_open(vm, store, url, LOAD);

    if (chkpt == NULL) {
	PrintError(vm, VCORE_NONE, "Error creating checkpoint store\n");
	return -1;
    }

    /* If this guest is running we need to block it while the checkpoint occurs */
    if (vm->run_state == VM_RUNNING) {
	while (v3_raise_barrier(vm, NULL) == -1);
    }

    if (!(opts & V3_CHKPT_OPT_SKIP_MEM)) {
      if ((ret = load_memory(vm, chkpt)) == -1) {
	PrintError(vm, VCORE_NONE, "Unable to load memory\n");
	goto out;
      }
    }

    if (!(opts & V3_CHKPT_OPT_SKIP_DEVS)) {
      if ((ret = v3_load_vm_devices(vm, chkpt)) == -1) {
	PrintError(vm, VCORE_NONE, "Unable to load devies\n");
	goto out;
      }
    }


    if ((ret = load_header(vm, chkpt)) == -1) {
	PrintError(vm, VCORE_NONE, "Unable to load header\n");
	goto out;
    }

    //per core cloning
    if (!(opts & V3_CHKPT_OPT_SKIP_CORES)) {
      for (i = 0; i < vm->num_cores; i++) {
	if ((ret = load_core(&(vm->cores[i]), chkpt, opts)) == -1) {
	  PrintError(vm, VCORE_NONE, "Error loading core state (core=%d)\n", i);
	  goto out;
	}
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



int v3_chkpt_send_vm(struct v3_vm_info * vm, char * store, char * url, v3_chkpt_options_t opts) {
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

    // Cores must all be in the same mode
    // or we must be skipping mmeory
    if (!(opts & V3_CHKPT_OPT_SKIP_MEM)) { 
      v3_paging_mode_t mode = vm->cores[0].shdw_pg_mode;
      for (i=1;i<vm->num_cores;i++) { 
	if (vm->cores[i].shdw_pg_mode != mode) { 
	  PrintError(vm, VCORE_NONE, "Cores having different paging modes (nested and shadow) are not supported\n");
	  return -1;
	}
      }
    }
    
    
    chkpt = chkpt_open(vm, store, url, SAVE);
    
    if (chkpt == NULL) {
	PrintError(vm, VCORE_NONE, "Error creating checkpoint store\n");
	chkpt_close(chkpt);
	return -1;
    }
    
    if (opts & V3_CHKPT_OPT_SKIP_MEM) {
      goto memdone;
    }

    // In a send, the memory is copied incrementally first,
    // followed by the remainder of the state
    
    if (v3_bitmap_init(&modified_pages_to_send,
		       vm->mem_size>>12 // number of pages in main region
		       ) == -1) {
        PrintError(vm, VCORE_NONE, "Could not intialize bitmap.\n");
        return -1;
    }

    // 0. Initialize bitmap to all 1s
    for (i=0; i < modified_pages_to_send.num_bits; i++) {
        v3_bitmap_set(&modified_pages_to_send,i);
    }

    iter = 0;
    while (!last_modpage_iteration) {
        PrintDebug(vm, VCORE_NONE, "Modified memory page iteration %d\n",i++);
        
        start_time = v3_get_host_time(&(vm->cores[0].time_state));
        
	// We will pause the VM for a short while
	// so that we can collect the set of changed pages
        if (v3_pause_vm(vm) == -1) {
            PrintError(vm, VCORE_NONE, "Could not pause VM\n");
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
            PrintDebug(vm, VCORE_NONE, "Last modified memory page iteration.\n");
            last_modpage_iteration = true;
	} else {
	    // we are not done, so we will restart page tracking
	    // to prepare for a second round of pages
	    // we will resume the VM as this happens
	    if (!(mm_state=start_page_tracking(vm))) { 
		PrintError(vm, VCORE_NONE, "Error enabling page tracking.\n");
		ret = -1;
		goto out;
	    }
            if (v3_continue_vm(vm) == -1) {
                PrintError(vm, VCORE_NONE, "Error resuming the VM\n");
		stop_page_tracking(mm_state);
                ret = -1;
                goto out;
            }
	    
            stop_time = v3_get_host_time(&(vm->cores[0].time_state));
            PrintDebug(vm, VCORE_NONE, "num_mod_pages=%d\ndowntime=%llu\n",num_mod_pages,stop_time-start_time);
        }
	

	// At this point, we are either paused and about to copy
	// the last chunk, or we are running, and will copy the last
	// round in parallel with current execution
	if (num_mod_pages>0) { 
	    if (save_inc_memory(vm, &modified_pages_to_send, chkpt) == -1) {
		PrintError(vm, VCORE_NONE, "Error sending incremental memory.\n");
		ret = -1;
		goto out;
	    }
	} // we don't want to copy an empty bitmap here
	
	iter++;
    }        
    
    if (v3_bitmap_reset(&modified_pages_to_send) == -1) {
        PrintError(vm, VCORE_NONE, "Error reseting bitmap.\n");
        ret = -1;
        goto out;
    }    
    
    // send bitmap of 0s to signal end of modpages
    if (save_inc_memory(vm, &modified_pages_to_send, chkpt) == -1) {
        PrintError(vm, VCORE_NONE, "Error sending incremental memory.\n");
        ret = -1;
        goto out;
    }

 memdone:    
    // save the non-memory state
    if (!(opts & V3_CHKPT_OPT_SKIP_DEVS)) {
      if ((ret = v3_save_vm_devices(vm, chkpt)) == -1) {
	PrintError(vm, VCORE_NONE, "Unable to save devices\n");
	goto out;
      }
    }

    if ((ret = save_header(vm, chkpt)) == -1) {
	PrintError(vm, VCORE_NONE, "Unable to save header\n");
	goto out;
    }
    
    if (!(opts & V3_CHKPT_OPT_SKIP_CORES)) {
      for (i = 0; i < vm->num_cores; i++){
	if ((ret = save_core(&(vm->cores[i]), chkpt, opts)) == -1) {
	  PrintError(vm, VCORE_NONE, "chkpt of core %d failed\n", i);
	  goto out;
	}
      }
    }

    if (!(opts & V3_CHKPT_OPT_SKIP_MEM)) {
      stop_time = v3_get_host_time(&(vm->cores[0].time_state));
      PrintDebug(vm, VCORE_NONE, "num_mod_pages=%d\ndowntime=%llu\n",num_mod_pages,stop_time-start_time);
      PrintDebug(vm, VCORE_NONE, "Done sending VM!\n"); 
    out:
      v3_bitmap_deinit(&modified_pages_to_send);
    }

    chkpt_close(chkpt);
    
    return ret;

}

int v3_chkpt_receive_vm(struct v3_vm_info * vm, char * store, char * url, v3_chkpt_options_t opts) {
    struct v3_chkpt * chkpt = NULL;
    int i = 0;
    int ret = 0;
    struct v3_bitmap mod_pgs;
 
    // Currently will work only for shadow paging
    for (i=0;i<vm->num_cores;i++) { 
      if (vm->cores[i].shdw_pg_mode!=SHADOW_PAGING && !(opts & V3_CHKPT_OPT_SKIP_MEM)) { 
	PrintError(vm, VCORE_NONE, "Cannot currently handle nested paging\n");
	return -1;
      }
    }
    
    chkpt = chkpt_open(vm, store, url, LOAD);
    
    if (chkpt == NULL) {
	PrintError(vm, VCORE_NONE, "Error creating checkpoint store\n");
	chkpt_close(chkpt);
	return -1;
    }
    

    if (opts & V3_CHKPT_OPT_SKIP_MEM) { 
      goto memdone;
    }

    if (v3_bitmap_init(&mod_pgs,vm->mem_size>>12) == -1) {
	chkpt_close(chkpt);
        PrintError(vm, VCORE_NONE, "Could not intialize bitmap.\n");
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
        PrintDebug(vm, VCORE_NONE, "Memory page iteration %d\n",i++);
        int retval = load_inc_memory(vm, &mod_pgs, chkpt);
        if (retval == 1) {
            // end of receiving memory pages
            break;        
        } else if (retval == -1) {
            PrintError(vm, VCORE_NONE, "Error receiving incremental memory.\n");
            ret = -1;
            goto out;
	}
    }        

 memdone:
    
    if (!(opts & V3_CHKPT_OPT_SKIP_DEVS)) { 
      if ((ret = v3_load_vm_devices(vm, chkpt)) == -1) {
	PrintError(vm, VCORE_NONE, "Unable to load devices\n");
	ret = -1;
	goto out;
      }
    }
    
    if ((ret = load_header(vm, chkpt)) == -1) {
	PrintError(vm, VCORE_NONE, "Unable to load header\n");
	ret = -1;
	goto out;
    }
    
    //per core cloning
    if (!(opts & V3_CHKPT_OPT_SKIP_CORES)) {
      for (i = 0; i < vm->num_cores; i++) {
	if ((ret = load_core(&(vm->cores[i]), chkpt, opts)) == -1) {
	  PrintError(vm, VCORE_NONE, "Error loading core state (core=%d)\n", i);
	  goto out;
	}
      }
    }

 out:
    if (ret==-1) { 
	PrintError(vm, VCORE_NONE, "Unable to receive VM\n");
    } else {
	PrintDebug(vm, VCORE_NONE, "Done receving the VM\n");
    }
	
	
    /* Resume the guest if it was running and we didn't just trash the state*/
    if (vm->run_state == VM_RUNNING) { 
	if (ret == -1) {
	    PrintError(vm, VCORE_NONE, "VM was previously running.  It is now borked.  Pausing it. \n");
	    vm->run_state = VM_STOPPED;
	}
	    
	/* We check the run state of the VM after every barrier 
	   So this will immediately halt the VM 
	*/
	v3_lower_barrier(vm);
    } 
    

    if (!(opts & V3_CHKPT_OPT_SKIP_MEM)) { 
      v3_bitmap_deinit(&mod_pgs);
    }

    chkpt_close(chkpt);

    return ret;
}

#endif
