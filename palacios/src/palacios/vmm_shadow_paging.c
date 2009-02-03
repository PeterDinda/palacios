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


#include <palacios/vmm_shadow_paging.h>


#include <palacios/vmm.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_decoder.h>
#include <palacios/vmm_ctrl_regs.h>

#include <palacios/vmm_hashtable.h>

#ifndef DEBUG_SHADOW_PAGING
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


/*** 
 ***  There be dragons
 ***/


struct guest_table {
  addr_t cr3;
  struct list_head link;
};


struct backptr {
  addr_t ptr;
  struct list_head link;
};


struct shadow_page_data {
  addr_t ptr;
  addr_t guest_addr; 

  struct list_head backptrs;
  struct list_head guest_tables;
};




//DEFINE_HASHTABLE_INSERT(add_cr3_to_cache, addr_t, struct hashtable *);
//DEFINE_HASHTABLE_SEARCH(find_cr3_in_cache, addr_t, struct hashtable *);
//DEFINE_HASHTABLE_REMOVE(del_cr3_from_cache, addr_t, struct hashtable *, 0);


DEFINE_HASHTABLE_INSERT(add_pte_map, addr_t, addr_t);
DEFINE_HASHTABLE_SEARCH(find_pte_map, addr_t, addr_t);
//DEFINE_HASHTABLE_REMOVE(del_pte_map, addr_t, addr_t, 0);



static uint_t pte_hash_fn(addr_t key) {
  return hash_long(key, 32);
}

static int pte_equals(addr_t key1, addr_t key2) {
  return (key1 == key2);
}

static addr_t create_new_shadow_pt();
static void inject_guest_pf(struct guest_info * info, addr_t fault_addr, pf_error_t error_code);
static int is_guest_pf(pt_access_status_t guest_access, pt_access_status_t shadow_access);


#include "vmm_shadow_paging_32.h"
#include "vmm_shadow_paging_32pae.h"
#include "vmm_shadow_paging_64.h"



int v3_init_shadow_page_state(struct guest_info * info) {
  struct shadow_page_state * state = &(info->shdw_pg_state);
  
  state->guest_cr3 = 0;
  state->guest_cr0 = 0;

  state->cached_ptes = NULL;
  state->cached_cr3 = 0;
  
  return 0;
}



// Reads the guest CR3 register
// creates new shadow page tables
// updates the shadow CR3 register to point to the new pts
int v3_activate_shadow_pt(struct guest_info * info) {
  switch (info->cpu_mode) {

  case PROTECTED:
    return activate_shadow_pt_32(info);
  case PROTECTED_PAE:
    return activate_shadow_pt_32pae(info);
  case LONG:
  case LONG_32_COMPAT:
  case LONG_16_COMPAT:
    return activate_shadow_pt_64(info);
  default:
    PrintError("Invalid CPU mode: %d\n", info->cpu_mode);
    return -1;
  }

  return 0;
}


int v3_activate_passthrough_pt(struct guest_info * info) {
  // For now... But we need to change this....
  // As soon as shadow paging becomes active the passthrough tables are hosed
  // So this will cause chaos if it is called at that time

  info->ctrl_regs.cr3 = *(addr_t*)&(info->direct_map_pt);
  //PrintError("Activate Passthrough Page tables not implemented\n");
  return 0;
}



int v3_handle_shadow_pagefault(struct guest_info * info, addr_t fault_addr, pf_error_t error_code) {
  
  if (info->mem_mode == PHYSICAL_MEM) {
    // If paging is not turned on we need to handle the special cases
    return handle_special_page_fault(info, fault_addr, fault_addr, error_code);
  } else if (info->mem_mode == VIRTUAL_MEM) {

    switch (info->cpu_mode) {
    case PROTECTED:
      return handle_shadow_pagefault_32(info, fault_addr, error_code);
      break;
    case PROTECTED_PAE:
      return handle_shadow_pagefault_32pae(info, fault_addr, error_code);
    case LONG:
      return handle_shadow_pagefault_64(info, fault_addr, error_code);
      break;
    default:
      PrintError("Unhandled CPU Mode\n");
      return -1;
    }
  } else {
    PrintError("Invalid Memory mode\n");
    return -1;
  }
}



static addr_t create_new_shadow_pt() {
  void * host_pde = 0;

  host_pde = V3_VAddr(V3_AllocPages(1));
  memset(host_pde, 0, PAGE_SIZE);

  return (addr_t)host_pde;
}


static void inject_guest_pf(struct guest_info * info, addr_t fault_addr, pf_error_t error_code) {
  if (info->enable_profiler) {
    info->profiler.guest_pf_cnt++;
  }

  info->ctrl_regs.cr2 = fault_addr;
  v3_raise_exception_with_error(info, PF_EXCEPTION, *(uint_t *)&error_code);
}


static int is_guest_pf(pt_access_status_t guest_access, pt_access_status_t shadow_access) {
  /* basically the reasoning is that there can be multiple reasons for a page fault:
     If there is a permissions failure for a page present in the guest _BUT_ 
     the reason for the fault was that the page is not present in the shadow, 
     _THEN_ we have to map the shadow page in and reexecute, this will generate 
     a permissions fault which is _THEN_ valid to send to the guest
     _UNLESS_ both the guest and shadow have marked the page as not present

     whew...
  */
  if (guest_access != PT_ACCESS_OK) {
    // Guest Access Error
    
    if ((shadow_access != PT_ACCESS_NOT_PRESENT) &&
	(guest_access != PT_ACCESS_NOT_PRESENT)) {
      // aka (guest permission error)
      return 1;
    }

    if ((shadow_access == PT_ACCESS_NOT_PRESENT) &&
	(guest_access == PT_ACCESS_NOT_PRESENT)) {      
      // Page tables completely blank, handle guest first
      return 1;
    }

    // Otherwise we'll handle the guest fault later...?
  }

  return 0;
}










int v3_handle_shadow_invlpg(struct guest_info * info) {
  uchar_t instr[15];
  struct x86_instr dec_instr;
  int ret = 0;
  addr_t vaddr = 0;

  if (info->mem_mode != VIRTUAL_MEM) {
    // Paging must be turned on...
    // should handle with some sort of fault I think
    PrintError("ERROR: INVLPG called in non paged mode\n");
    return -1;
  }

  if (info->mem_mode == PHYSICAL_MEM) { 
    ret = read_guest_pa_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
  } else { 
    ret = read_guest_va_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
  }

  if (ret == -1) {
    PrintError("Could not read instruction into buffer\n");
    return -1;
  }

  if (v3_decode(info, (addr_t)instr, &dec_instr) == -1) {
    PrintError("Decoding Error\n");
    return -1;
  }
  
  if ((dec_instr.op_type != V3_OP_INVLPG) || 
      (dec_instr.num_operands != 1) ||
      (dec_instr.dst_operand.type != MEM_OPERAND)) {
    PrintError("Decoder Error: Not a valid INVLPG instruction...\n");
    return -1;
  }

  vaddr = dec_instr.dst_operand.operand;

  info->rip += dec_instr.instr_length;

  switch (info->cpu_mode) {
  case PROTECTED:
    return handle_shadow_invlpg_32(info, vaddr);
  case PROTECTED_PAE:
    return handle_shadow_invlpg_32pae(info, vaddr);
  case LONG:
  case LONG_32_COMPAT:
  case LONG_16_COMPAT:
    return handle_shadow_invlpg_64(info, vaddr);
  default:
    PrintError("Invalid CPU mode: %d\n", info->cpu_mode);
    return -1;
  }
}

