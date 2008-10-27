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




#include <palacios/vm_guest.h>
#include <palacios/vmm_ctrl_regs.h>
#include <palacios/vmm.h>
#include <palacios/vmcb.h>


v3_vm_cpu_mode_t v3_get_cpu_mode(struct guest_info * info) {
  struct cr0_32 * cr0;
  struct cr4_32 * cr4 = (struct cr4_32 *)&(info->ctrl_regs.cr4);
  struct efer_64 * efer = (struct efer_64 *)&(info->ctrl_regs.efer);
  struct v3_segment * cs = &(info->segments.cs);

  if (info->shdw_pg_mode == SHADOW_PAGING) {
    cr0 = (struct cr0_32 *)&(info->shdw_pg_state.guest_cr0);
  } else if (info->shdw_pg_mode == NESTED_PAGING) {
    cr0 = (struct cr0_32 *)&(info->ctrl_regs.cr0);
  } else {
    PrintError("Invalid Paging Mode...\n");
    V3_ASSERT(0);
    return -1;
  }

  if (cr0->pe == 0) {
    return REAL;
  } else if ((cr4->pae == 0) && (efer->lme == 0)) {
    return PROTECTED;
  } else if (efer->lme == 0) {
    return PROTECTED_PAE;
  } else if ((efer->lme == 1) && (cs->long_mode == 1)) {
    return LONG;
  } else {
    // What about LONG_16_COMPAT???
    return LONG_32_COMPAT;
  }
}

v3_vm_mem_mode_t v3_get_mem_mode(struct guest_info * info) {
  struct cr0_32 * cr0;

  if (info->shdw_pg_mode == SHADOW_PAGING) {
    cr0 = (struct cr0_32 *)&(info->shdw_pg_state.guest_cr0);
  } else if (info->shdw_pg_mode == NESTED_PAGING) {
    cr0 = (struct cr0_32 *)&(info->ctrl_regs.cr0);
  } else {
    PrintError("Invalid Paging Mode...\n");
    V3_ASSERT(0);
    return -1;
  }



  if (cr0->pg == 0) {
    return PHYSICAL_MEM;
  } else {
    return VIRTUAL_MEM;
  }
}


void v3_print_segments(struct guest_info * info) {
  struct v3_segments * segs = &(info->segments);
  int i = 0;
  struct v3_segment * seg_ptr;

  seg_ptr=(struct v3_segment *)segs;
  
  char *seg_names[] = {"CS", "DS" , "ES", "FS", "GS", "SS" , "LDTR", "GDTR", "IDTR", "TR", NULL};
  PrintDebug("Segments\n");

  for (i = 0; seg_names[i] != NULL; i++) {

    PrintDebug("\t%s: Sel=%x, base=%p, limit=%x\n", seg_names[i], seg_ptr[i].selector, 
	       (void *)(addr_t)seg_ptr[i].base, seg_ptr[i].limit);

  }

}


void v3_print_ctrl_regs(struct guest_info * info) {
  struct v3_ctrl_regs * regs = &(info->ctrl_regs);
  int i = 0;
  v3_reg_t * reg_ptr;
  char * reg_names[] = {"CR0", "CR2", "CR3", "CR4", "CR8", "FLAGS", NULL};
  vmcb_saved_state_t * guest_state = GET_VMCB_SAVE_STATE_AREA(info->vmm_data);

  reg_ptr= (v3_reg_t *)regs;

  PrintDebug("32 bit Ctrl Regs:\n");

  for (i = 0; reg_names[i] != NULL; i++) {
    PrintDebug("\t%s=0x%p\n", reg_names[i], (void *)(addr_t)reg_ptr[i]);  
  }

  PrintDebug("\tEFER=0x%p\n", (void*)(addr_t)(guest_state->efer));

}


void v3_print_GPRs(struct guest_info * info) {
  struct v3_gprs * regs = &(info->vm_regs);
  int i = 0;
  v3_reg_t * reg_ptr;
  char * reg_names[] = { "RDI", "RSI", "RBP", "RSP", "RBX", "RDX", "RCX", "RAX", NULL};

  reg_ptr= (v3_reg_t *)regs;

  PrintDebug("32 bit GPRs:\n");

  for (i = 0; reg_names[i] != NULL; i++) {
    PrintDebug("\t%s=0x%p\n", reg_names[i], (void *)(addr_t)reg_ptr[i]);  
  }
}
