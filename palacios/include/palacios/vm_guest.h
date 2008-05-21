#ifndef __VM_GUEST_H
#define __VM_GUEST_H




#include <palacios/vmm_mem.h>
#include <palacios/vmm_types.h>
#include <palacios/vmm_io.h>
#include <palacios/vmm_shadow_paging.h>
#include <palacios/vmm_intr.h>
#include <palacios/vmm_dev_mgr.h>
#include <palacios/vmm_time.h>


typedef ullong_t v3_reg_t;



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


struct vm_ctrl_ops {
  int (*raise_irq)(struct guest_info * info, int irq);
};





typedef enum {SHADOW_PAGING, NESTED_PAGING} vm_page_mode_t;
typedef enum {REAL, PROTECTED, PROTECTED_PG, PROTECTED_PAE, PROTECTED_PAE_PG, LONG, LONG_PG} vm_cpu_mode_t;

struct guest_info {
  ullong_t rip;


  uint_t cpl;


  struct shadow_map mem_map;

  struct vm_time time_state;
  
  vm_page_mode_t page_mode;
  struct shadow_page_state shdw_pg_state;
  // nested_paging_t nested_page_state;


  // This structure is how we get interrupts for the guest
  struct vm_intr intr_state;

  vmm_io_map_t io_map;
  // device_map

  struct vmm_dev_mgr  dev_mgr;

  vm_cpu_mode_t cpu_mode;


  struct v3_gprs vm_regs;
  struct v3_ctrl_regs ctrl_regs;
  struct v3_segments segments;

  struct vm_ctrl_ops vm_ops;



  void * vmm_data;
};





#endif
