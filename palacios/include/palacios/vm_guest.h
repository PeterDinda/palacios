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

#ifndef __VM_GUEST_H__
#define __VM_GUEST_H__

#ifdef __V3VEE__

#include <palacios/vmm_types.h>
#include <palacios/vmm_mem.h>
#include <palacios/vmm_io.h>
#include <palacios/vmm_shadow_paging.h>
#include <palacios/vmm_intr.h>
#include <palacios/vmm_dev_mgr.h>
#include <palacios/vmm_time.h>
#include <palacios/vmm_emulator.h>
#include <palacios/vmm_host_events.h>
#include <palacios/vmm_msr.h>




struct v3_gprs {
  v3_reg_t rdi;
  v3_reg_t rsi;
  v3_reg_t rbp;
  v3_reg_t rsp;
  v3_reg_t rbx;
  v3_reg_t rdx;
  v3_reg_t rcx;
  v3_reg_t rax;
};


struct v3_ctrl_regs {
  v3_reg_t cr0;
  v3_reg_t cr2;
  v3_reg_t cr3;
  v3_reg_t cr4;
  v3_reg_t cr8;
  v3_reg_t rflags;
  v3_reg_t efer;
};



struct v3_dbg_regs {
  v3_reg_t dr0;
  v3_reg_t dr1;
  v3_reg_t dr2;
  v3_reg_t dr3;
  v3_reg_t dr6;
  v3_reg_t dr7;
};

struct v3_segment {
  ushort_t selector;
  uint_t limit;
  ullong_t base;
  uint_t type           : 4;
  uint_t system         : 1;
  uint_t dpl            : 2;
  uint_t present        : 1;
  uint_t avail          : 1;
  uint_t long_mode      : 1;
  uint_t db             : 1;
  uint_t granularity    : 1;
};


struct v3_segments {
  struct v3_segment cs;
  struct v3_segment ds;
  struct v3_segment es;
  struct v3_segment fs;
  struct v3_segment gs;
  struct v3_segment ss;
  struct v3_segment ldtr;
  struct v3_segment gdtr;
  struct v3_segment idtr;
  struct v3_segment tr;
};

struct shadow_page_state;
struct shadow_map;
struct vmm_io_map;
struct emulation_state;
struct v3_intr_state;



typedef enum {SHADOW_PAGING, NESTED_PAGING} v3_paging_mode_t;
typedef enum {VM_RUNNING, VM_STOPPED, VM_SUSPENDED, VM_ERROR, VM_EMULATING} v3_vm_operating_mode_t;


typedef enum {REAL, /*UNREAL,*/ PROTECTED, PROTECTED_PAE, LONG, LONG_32_COMPAT, LONG_16_COMPAT} v3_vm_cpu_mode_t;
typedef enum {PHYSICAL_MEM, VIRTUAL_MEM} v3_vm_mem_mode_t;



struct guest_info {
  ullong_t rip;

  uint_t cpl;

  struct shadow_map mem_map;

  struct vm_time time_state;
  
  v3_paging_mode_t shdw_pg_mode;
  struct shadow_page_state shdw_pg_state;
  addr_t direct_map_pt;
  // nested_paging_t nested_page_state;


  // This structure is how we get interrupts for the guest
  struct v3_intr_state intr_state;

  struct vmm_io_map io_map;

  struct v3_msr_map msr_map;
  // device_map

  struct vmm_dev_mgr  dev_mgr;

  struct v3_host_events host_event_hooks;

  v3_vm_cpu_mode_t cpu_mode;
  v3_vm_mem_mode_t mem_mode;


  struct v3_gprs vm_regs;
  struct v3_ctrl_regs ctrl_regs;
  struct v3_dbg_regs dbg_regs;
  struct v3_segments segments;

  struct emulation_state emulator;

  v3_vm_operating_mode_t run_state;
  void * vmm_data;

  /* TEMP */
  //ullong_t exit_tsc;

};


v3_vm_cpu_mode_t v3_get_cpu_mode(struct guest_info * info);
v3_vm_mem_mode_t v3_get_mem_mode(struct guest_info * info);


void v3_print_segments(struct guest_info * info);
void v3_print_ctrl_regs(struct guest_info * info);
void v3_print_GPRs(struct guest_info * info);

#endif // ! __V3VEE__



#endif
