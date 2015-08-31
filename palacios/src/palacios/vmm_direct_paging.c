/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Steven Jaconette <stevenjaconette2007@u.northwestern.edu> 
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Steven Jaconette <stevenjaconette2007@u.northwestern.edu>
 *         Peter Dinda <pdinda@northwestern.edu> (refactor + events)
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm_direct_paging.h>
#include <palacios/vmm_paging.h>
#include <palacios/vmm.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_ctrl_regs.h>


#if !defined(V3_CONFIG_DEBUG_NESTED_PAGING) && !defined(V3_CONFIG_DEBUG_SHADOW_PAGING)
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif



/*

  "Direct Paging" combines these three functionalities:

   1. Passthrough paging for SVM and VMX

      Passthrough paging is used for shadow paging when
      the guest does not have paging turn on, for example 
      when it is running in real mode or protected mode 
      early in a typical boot process.    Passthrough page
      tables are shadow page tables that are built assuming
      the guest virtual to guest physical mapping is the identity.
      Thus, what they implement are the GPA->HPA mapping. 

      Passthrough page tables are built using 32PAE paging.
      

   2. Nested paging on SVM
  
      The SVM nested page tables have the same format as
      regular page tables.   For this reason, we can reuse 
      much of the passthrough implementation.   A nested page
      table mapping is a GPA->HPA mapping, creating a very 
      simlar model as with passthrough paging, just that it's 
      always active, whether the guest has paging on or not.


   3. Nested paging on VMX

      The VMX nested page tables have a different format
      than regular page tables.  For this reason, we have
      implemented them in the vmx_npt.h file.  The code
      here then is a wrapper, allowing us to make nested
      paging functionality appear uniform across VMX and SVM
      elsewhere in the codebase.

*/



static inline int is_vmx_nested()
{
    extern v3_cpu_arch_t v3_mach_type;

    return (v3_mach_type==V3_VMX_EPT_CPU || v3_mach_type==V3_VMX_EPT_UG_CPU);
}

static inline int is_svm_nested()
{
    extern v3_cpu_arch_t v3_mach_type;

    return (v3_mach_type==V3_SVM_REV3_CPU);
}


struct passthrough_event_callback {
    int (*callback)(struct guest_info *core, struct v3_passthrough_pg_event *event, void *priv_data);
    void *priv_data;

    struct list_head node;
};


static int have_passthrough_callbacks(struct guest_info *core)
{
    // lock acquistion unnecessary
    // caller will acquire the lock before *iterating* through the list
    // so any race will be resolved then
    return !list_empty(&(core->vm_info->passthrough_impl.event_callback_list));
}

static void dispatch_passthrough_event(struct guest_info *core, struct v3_passthrough_pg_event *event)
{
    struct passthrough_event_callback *cb,*temp;
 
    v3_read_lock(&(core->vm_info->passthrough_impl.event_callback_lock));
   
    list_for_each_entry_safe(cb,
			     temp,
			     &(core->vm_info->passthrough_impl.event_callback_list),
			     node) {
	cb->callback(core,event,cb->priv_data);
    }

    v3_read_unlock(&(core->vm_info->passthrough_impl.event_callback_lock));

}

struct nested_event_callback {
    int (*callback)(struct guest_info *core, struct v3_nested_pg_event *event, void *priv_data);
    void *priv_data;

    struct list_head node;
};


static int have_nested_callbacks(struct guest_info *core)
{
    // lock acquistion unnecessary
    // caller will acquire the lock before *iterating* through the list
    // so any race will be resolved then
    return !list_empty(&(core->vm_info->nested_impl.event_callback_list));
}

static void dispatch_nested_event(struct guest_info *core, struct v3_nested_pg_event *event)
{
    struct nested_event_callback *cb,*temp;
    
    v3_read_lock(&(core->vm_info->nested_impl.event_callback_lock));

    list_for_each_entry_safe(cb,
			     temp,
			     &(core->vm_info->nested_impl.event_callback_list),
			     node) {
	cb->callback(core,event,cb->priv_data);
    }

    v3_read_unlock(&(core->vm_info->nested_impl.event_callback_lock));
}




static addr_t create_generic_pt_page(struct guest_info *core) {
    void * page = 0;
    void *temp;

    temp = V3_AllocPagesExtended(1, PAGE_SIZE_4KB, 
				 core->resource_control.pg_node_id,
				 core->resource_control.pg_filter_func,
				 core->resource_control.pg_filter_state);

    if (!temp) {  
	PrintError(VM_NONE, VCORE_NONE,"Cannot allocate page\n");
	return 0;
    }

    page = V3_VAddr(temp);
    memset(page, 0, PAGE_SIZE);

    return (addr_t)page;
}

// Inline handler functions for each cpu mode
#include "vmm_direct_paging_32.h"
#include "vmm_direct_paging_32pae.h"
#include "vmm_direct_paging_64.h"



int v3_init_passthrough_pts(struct guest_info * info) {
    if (info->shdw_pg_mode == NESTED_PAGING && is_vmx_nested()) { 
        // skip - ept_init will do this allocation
        return 0;
    }
    info->direct_map_pt = (addr_t)V3_PAddr((void *)create_generic_pt_page(info));
    return 0;
}


int v3_free_passthrough_pts(struct guest_info * core) {
    v3_cpu_mode_t mode = v3_get_vm_cpu_mode(core);

    if (core->shdw_pg_mode == NESTED_PAGING && is_vmx_nested()) { 
        // there are no passthrough page tables, but
        // the EPT implementation is using direct_map_pt to store
        // the EPT root table pointer...  and the EPT tables
        // are not compatible with regular x86 tables, so we
        // must not attempt to free them here...
        return 0;
    }
  
    // we are either in shadow or in SVM nested
    // in either case, we can nuke the PTs

    // Delete the old direct map page tables
    switch(mode) {
	case REAL:
	case PROTECTED:
	  // Intentional fallthrough here
	  // There are *only* PAE tables
	case PROTECTED_PAE:
	case LONG:
	case LONG_32_COMPAT:
	    // Long mode will only use 32PAE page tables...
	    if (core->direct_map_pt) { 
		delete_page_tables_32pae((pdpe32pae_t *)V3_VAddr((void *)(core->direct_map_pt))); 
	    }
	    break;
	default:
	    PrintError(core->vm_info, core, "Unknown CPU Mode\n");
	    return -1;
	    break;
    }

    return 0;
}


int v3_reset_passthrough_pts(struct guest_info * core) {

    v3_free_passthrough_pts(core);

    // create new direct map page table
    v3_init_passthrough_pts(core);
    
    return 0;
}



int v3_activate_passthrough_pt(struct guest_info * info) {
    // For now... But we need to change this....
    // As soon as shadow paging becomes active the passthrough tables are hosed
    // So this will cause chaos if it is called at that time

    if (have_passthrough_callbacks(info)) { 
	struct v3_passthrough_pg_event event={PASSTHROUGH_ACTIVATE,PASSTHROUGH_PREIMPL,0,{0,0,0,0,0,0},0,0};
	dispatch_passthrough_event(info,&event);
    }
	
    struct cr3_32_PAE * shadow_cr3 = (struct cr3_32_PAE *) &(info->ctrl_regs.cr3);
    struct cr4_32 * shadow_cr4 = (struct cr4_32 *) &(info->ctrl_regs.cr4);
    addr_t shadow_pt_addr = *(addr_t*)&(info->direct_map_pt);
    // Passthrough PTs will only be PAE page tables.
    shadow_cr3->pdpt_base_addr = shadow_pt_addr >> 5;
    shadow_cr4->pae = 1;
    PrintDebug(info->vm_info, info, "Activated Passthrough Page tables\n");

    if (have_passthrough_callbacks(info)) { 
	struct v3_passthrough_pg_event event={PASSTHROUGH_ACTIVATE,PASSTHROUGH_POSTIMPL,0,{0,0,0,0,0,0},0,0};
	dispatch_passthrough_event(info,&event);
    }

    return 0;
}



int v3_handle_passthrough_pagefault(struct guest_info * info, addr_t fault_addr, pf_error_t error_code,
				    addr_t *actual_start, addr_t *actual_end) {
    v3_cpu_mode_t mode = v3_get_vm_cpu_mode(info);
    addr_t start, end;
    int rc;

    if (have_passthrough_callbacks(info)) {				       
	struct v3_passthrough_pg_event event={PASSTHROUGH_PAGEFAULT,PASSTHROUGH_PREIMPL,fault_addr,error_code,fault_addr,fault_addr};
	dispatch_passthrough_event(info,&event);	
    }

    if (!actual_start) { actual_start=&start; }
    if (!actual_end) { actual_end=&end; }


    rc=-1;

    switch(mode) {
	case REAL:
	case PROTECTED:
	  // Note intentional fallthrough here
	  // There are only PAE page tables now
	case PROTECTED_PAE:
	case LONG:
	case LONG_32_COMPAT:
	    // Long mode will only use 32PAE page tables...
	    rc=handle_passthrough_pagefault_32pae(info, fault_addr, error_code, actual_start, actual_end);
	    break;
	default:
	    PrintError(info->vm_info, info, "Unknown CPU Mode\n");
	    break;
    }

    if (have_passthrough_callbacks(info)) {				       
	struct v3_passthrough_pg_event event={PASSTHROUGH_PAGEFAULT,PASSTHROUGH_POSTIMPL,fault_addr,error_code,*actual_start,*actual_end};
	dispatch_passthrough_event(info,&event);	
    }

    return rc;
}



int v3_invalidate_passthrough_addr(struct guest_info * info, addr_t inv_addr, 
				   addr_t *actual_start, addr_t *actual_end) {

    v3_cpu_mode_t mode = v3_get_vm_cpu_mode(info);
    addr_t start, end;
    int rc;

    if (have_passthrough_callbacks(info)) {				       
	struct v3_passthrough_pg_event event={PASSTHROUGH_INVALIDATE_RANGE,PASSTHROUGH_PREIMPL,0,{0,0,0,0,0,0},PAGE_ADDR(inv_addr),PAGE_ADDR(inv_addr)+PAGE_SIZE-1};
	dispatch_passthrough_event(info,&event);	
    }

    if (!actual_start) { actual_start=&start;}
    if (!actual_end) { actual_end=&end;}



    rc=-1;

    switch(mode) {
	case REAL:
	case PROTECTED:
	  // Intentional fallthrough - there
	  // are only PAE page tables now
	case PROTECTED_PAE:
	case LONG:
	case LONG_32_COMPAT:
	    // Long mode will only use 32PAE page tables...
	    rc=invalidate_addr_32pae(info, inv_addr, actual_start, actual_end);
	    break;
	default:
	    PrintError(info->vm_info, info, "Unknown CPU Mode\n");
	    break;
    }

    if (have_passthrough_callbacks(info)) {				       
	struct v3_passthrough_pg_event event={PASSTHROUGH_INVALIDATE_RANGE,PASSTHROUGH_POSTIMPL,0,{0,0,0,0,0,0},*actual_start,*actual_end};
	dispatch_passthrough_event(info,&event);	
    }


    return rc;
}


int v3_invalidate_passthrough_addr_range(struct guest_info * info, 
					 addr_t inv_addr_start, addr_t inv_addr_end,
					 addr_t *actual_start, addr_t *actual_end) {
    v3_cpu_mode_t mode = v3_get_vm_cpu_mode(info);
    addr_t start, end;
    int rc;

    if (!actual_start) { actual_start=&start;}
    if (!actual_end) { actual_end=&end;}

    if (have_passthrough_callbacks(info)) {				       
	struct v3_passthrough_pg_event event={PASSTHROUGH_INVALIDATE_RANGE,PASSTHROUGH_PREIMPL,0,{0,0,0,0,0,0},PAGE_ADDR(inv_addr_start),PAGE_ADDR(inv_addr_end-1)+PAGE_SIZE-1};
	dispatch_passthrough_event(info,&event);	
    }
    
    rc=-1;

    switch(mode) {
	case REAL:
	case PROTECTED:
	  // Intentional fallthrough
	  // There are only PAE PTs now
	case PROTECTED_PAE:
	case LONG:
	case LONG_32_COMPAT:
	    // Long mode will only use 32PAE page tables...
	  rc=invalidate_addr_32pae_range(info, inv_addr_start, inv_addr_end, actual_start, actual_end);
	  break;
	default:
	    PrintError(info->vm_info, info, "Unknown CPU Mode\n");
	    break;
    }

    if (have_passthrough_callbacks(info)) {				       
	struct v3_passthrough_pg_event event={PASSTHROUGH_INVALIDATE_RANGE,PASSTHROUGH_POSTIMPL,0,{0,0,0,0,0,0},*actual_start,*actual_end};
	dispatch_passthrough_event(info,&event);	
    }

    return rc;
}


int v3_init_passthrough_paging(struct v3_vm_info *vm)
{
  INIT_LIST_HEAD(&(vm->passthrough_impl.event_callback_list));
  v3_rw_lock_init(&(vm->passthrough_impl.event_callback_lock));
  return 0;
}

int v3_deinit_passthrough_paging(struct v3_vm_info *vm)
{
  struct passthrough_event_callback *cb,*temp;
  addr_t flags;
  
  flags=v3_write_lock_irqsave(&(vm->passthrough_impl.event_callback_lock));
  
  list_for_each_entry_safe(cb,
			   temp,
			   &(vm->passthrough_impl.event_callback_list),
			   node) {
    list_del(&(cb->node));
    V3_Free(cb);
  }

  v3_write_unlock_irqrestore(&(vm->passthrough_impl.event_callback_lock),flags);

  v3_rw_lock_deinit(&(vm->passthrough_impl.event_callback_lock));
  
  return 0;
}

int v3_init_passthrough_paging_core(struct guest_info *core)
{
  // currently nothing to init
  return 0;
}

int v3_deinit_passthrough_paging_core(struct guest_info *core)
{
  // currently nothing to deinit
  return 0;
}


int v3_register_passthrough_paging_event_callback(struct v3_vm_info *vm,
						  int (*callback)(struct guest_info *core, 
								  struct v3_passthrough_pg_event *,
								  void      *priv_data),
						  void *priv_data)
{
    struct passthrough_event_callback *ec = V3_Malloc(sizeof(struct passthrough_event_callback));
    addr_t flags;
    
    if (!ec) { 
	PrintError(vm, VCORE_NONE, "Unable to allocate for a nested paging event callback\n");
	return -1;
    }
    
    ec->callback = callback;
    ec->priv_data = priv_data;
    
    flags=v3_write_lock_irqsave(&(vm->passthrough_impl.event_callback_lock));
    list_add(&(ec->node),&(vm->passthrough_impl.event_callback_list));
    v3_write_unlock_irqrestore(&(vm->passthrough_impl.event_callback_lock),flags);

    return 0;

}



int v3_unregister_passthrough_paging_event_callback(struct v3_vm_info *vm,
						    int (*callback)(struct guest_info *core, 
								    struct v3_passthrough_pg_event *,
								    void      *priv_data),
						    void *priv_data)
{
    struct passthrough_event_callback *cb,*temp;
    addr_t flags;

    flags=v3_write_lock_irqsave(&(vm->passthrough_impl.event_callback_lock));

    list_for_each_entry_safe(cb,
			     temp,
			     &(vm->passthrough_impl.event_callback_list),
			     node) {
	if ((callback == cb->callback) && (priv_data == cb->priv_data)) { 
	    list_del(&(cb->node));
	    V3_Free(cb);
	    v3_write_unlock_irqrestore(&(vm->passthrough_impl.event_callback_lock),flags);
	    return 0;
	}
    }
    
    v3_write_unlock_irqrestore(&(vm->passthrough_impl.event_callback_lock),flags);

    PrintError(vm, VCORE_NONE, "No callback found!\n");
    
    return -1;
}


// inline nested paging support for Intel and AMD
#include "svm_npt.h"
#include "vmx_npt.h"


inline void convert_to_pf_error(void *pfinfo, pf_error_t *out)
{
  if (is_vmx_nested()) {
#ifdef V3_CONFIG_VMX
    ept_exit_qual_to_pf_error((struct ept_exit_qual *)pfinfo, out);
#endif
  } else {
    *out = *(pf_error_t *)pfinfo;
  }
}

int v3_handle_nested_pagefault(struct guest_info * info, addr_t fault_addr, void *pfinfo, addr_t *actual_start, addr_t *actual_end)
{
  int rc;
  pf_error_t err;
  addr_t start, end;

  if (!actual_start) { actual_start=&start; }
  if (!actual_end) { actual_end=&end; }

  convert_to_pf_error(pfinfo,&err);

  if (have_nested_callbacks(info)) {				       
      struct v3_nested_pg_event event={NESTED_PAGEFAULT,NESTED_PREIMPL,fault_addr,err,fault_addr,fault_addr};
      dispatch_nested_event(info,&event);	
  }

  
  if (is_vmx_nested()) { 
    rc = handle_vmx_nested_pagefault(info,fault_addr,pfinfo,actual_start,actual_end);
  } else {
    rc = handle_svm_nested_pagefault(info,fault_addr,pfinfo,actual_start,actual_end);
  }
  
  if (have_nested_callbacks(info)) {
    struct v3_nested_pg_event event={NESTED_PAGEFAULT,NESTED_POSTIMPL,fault_addr,err,*actual_start,*actual_end};
    dispatch_nested_event(info,&event);
  }
  
  return rc;
}
  


int v3_invalidate_nested_addr(struct guest_info * info, addr_t inv_addr,
			      addr_t *actual_start, addr_t *actual_end) 
{
  int rc;
  
  addr_t start, end;

  if (!actual_start) { actual_start=&start; }
  if (!actual_end) { actual_end=&end; }
  

  if (have_nested_callbacks(info)) { 
    struct v3_nested_pg_event event={NESTED_INVALIDATE_RANGE,NESTED_PREIMPL,0,{0,0,0,0,0,0},PAGE_ADDR(inv_addr),PAGE_ADDR(inv_addr)+PAGE_SIZE-1};
    dispatch_nested_event(info,&event);
  }

  if (is_vmx_nested()) {
    rc = handle_vmx_invalidate_nested_addr(info, inv_addr, actual_start, actual_end);
  } else {
    rc = handle_svm_invalidate_nested_addr(info, inv_addr, actual_start, actual_end);
  }
  
  if (have_nested_callbacks(info)) { 
    struct v3_nested_pg_event event={NESTED_INVALIDATE_RANGE,NESTED_POSTIMPL,0,{0,0,0,0,0,0},*actual_start, *actual_end};
    dispatch_nested_event(info,&event);
  }
  return rc;
}


int v3_invalidate_nested_addr_range(struct guest_info * info, 
				    addr_t inv_addr_start, addr_t inv_addr_end,
				    addr_t *actual_start, addr_t *actual_end) 
{
  int rc;

  addr_t start, end;

  if (!actual_start) { actual_start=&start; }
  if (!actual_end) { actual_end=&end; }

  if (have_nested_callbacks(info)) { 
    struct v3_nested_pg_event event={NESTED_INVALIDATE_RANGE,NESTED_PREIMPL,0,{0,0,0,0,0,0},PAGE_ADDR(inv_addr_start),PAGE_ADDR(inv_addr_end-1)+PAGE_SIZE-1};
    dispatch_nested_event(info,&event);
  }
  
  if (is_vmx_nested()) {
    rc = handle_vmx_invalidate_nested_addr_range(info, inv_addr_start, inv_addr_end, actual_start, actual_end);
  } else {
    rc = handle_svm_invalidate_nested_addr_range(info, inv_addr_start, inv_addr_end, actual_start, actual_end);
  }
  

  if (have_nested_callbacks(info)) { 
    struct v3_nested_pg_event event={NESTED_INVALIDATE_RANGE,NESTED_PREIMPL,0,{0,0,0,0,0,0},*actual_start, *actual_end};
    dispatch_nested_event(info,&event);
  }
  
  return rc;
  
}


int v3_init_nested_paging(struct v3_vm_info *vm)
{
  INIT_LIST_HEAD(&(vm->nested_impl.event_callback_list));
  v3_rw_lock_init(&(vm->nested_impl.event_callback_lock));
  return 0;
}

int v3_init_nested_paging_core(struct guest_info *core, void *hwinfo)
{
  if (is_vmx_nested()) { 
    return init_ept(core, (struct vmx_hw_info *) hwinfo);
  } else {
    // no initialization for SVM
    // the direct map page tables are used since the 
    // nested pt format is identical to the main pt format
    return 0;
  }
}
    
int v3_deinit_nested_paging(struct v3_vm_info *vm)
{
  struct nested_event_callback *cb,*temp;
  addr_t flags;
  
  flags=v3_write_lock_irqsave(&(vm->nested_impl.event_callback_lock));
    
  list_for_each_entry_safe(cb,
			   temp,
			   &(vm->nested_impl.event_callback_list),
			   node) {
    list_del(&(cb->node));
    V3_Free(cb);
  }
  
  v3_write_unlock_irqrestore(&(vm->nested_impl.event_callback_lock),flags);
  
  v3_rw_lock_deinit(&(vm->nested_impl.event_callback_lock));

  return 0;
}

int v3_deinit_nested_paging_core(struct guest_info *core)
{
  if (core->shdw_pg_mode == NESTED_PAGING) {
    if (is_vmx_nested()) {
     return deinit_ept(core);
    } else {
      // SVM nested deinit is handled by the passthrough paging teardown
      return 0;
    }
  } else {
    // not relevant
    return 0;
  }
}


int v3_register_nested_paging_event_callback(struct v3_vm_info *vm,
                                            int (*callback)(struct guest_info *core, 
                                                            struct v3_nested_pg_event *,
                                                            void      *priv_data),
                                            void *priv_data)
{
    struct nested_event_callback *ec = V3_Malloc(sizeof(struct nested_event_callback));
    addr_t flags;

    if (!ec) { 
	PrintError(vm, VCORE_NONE, "Unable to allocate for a nested paging event callback\n");
	return -1;
    }
    
    ec->callback = callback;
    ec->priv_data = priv_data;

    flags=v3_write_lock_irqsave(&(vm->nested_impl.event_callback_lock));
    list_add(&(ec->node),&(vm->nested_impl.event_callback_list));
    v3_write_unlock_irqrestore(&(vm->nested_impl.event_callback_lock),flags);

    return 0;

}



int v3_unregister_nested_paging_event_callback(struct v3_vm_info *vm,
                                              int (*callback)(struct guest_info *core, 
                                                              struct v3_nested_pg_event *,
                                                              void      *priv_data),
                                              void *priv_data)
{
    struct nested_event_callback *cb,*temp;
    addr_t flags;

    flags=v3_write_lock_irqsave(&(vm->nested_impl.event_callback_lock));

    list_for_each_entry_safe(cb,
			     temp,
			     &(vm->nested_impl.event_callback_list),
			     node) {
	if ((callback == cb->callback) && (priv_data == cb->priv_data)) { 
	    list_del(&(cb->node));
	    V3_Free(cb);
	    v3_write_unlock_irqrestore(&(vm->nested_impl.event_callback_lock),flags);
	    return 0;
	}
    }
    
    v3_write_unlock_irqrestore(&(vm->nested_impl.event_callback_lock),flags);

    PrintError(vm, VCORE_NONE, "No callback found!\n");
    
    return -1;
}
