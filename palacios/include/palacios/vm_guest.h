#ifndef __VM_GUEST_H
#define __VM_GUEST_H




#include <palacios/vmm_mem.h>
#include <palacios/vmm_types.h>
#include <palacios/vmm_io.h>
#include <palacios/vmm_shadow_paging.h>
#include <palacios/vmm_intr.h>
#include <palacios/vmm_dev_mgr.h>
#include <palacios/vmm_irq.h>


typedef ullong_t gpr_t;

/*
  struct guest_gprs {
  addr_t rax;
  addr_t rbx;
  addr_t rcx;
  addr_t rdx;
  addr_t rsi;
  addr_t rdi;
  addr_t rbp;
  };
*/

struct guest_gprs {
  gpr_t rdi;
  gpr_t rsi;
  gpr_t rbp;
  gpr_t rsp;
  gpr_t rbx;
  gpr_t rdx;
  gpr_t rcx;
  gpr_t rax;
};


struct shadow_page_state;
struct shadow_map;


struct vm_ctrl_ops {
  int (*raise_irq)(struct guest_info * info, int irq, int error_code);
};





typedef enum {SHADOW_PAGING, NESTED_PAGING} vm_page_mode_t;
typedef enum {REAL, PROTECTED, PROTECTED_PG, PROTECTED_PAE, PROTECTED_PAE_PG, LONG, LONG_PG} vm_cpu_mode_t;

struct guest_info {
  ullong_t rip;


  struct shadow_map mem_map;

  
  vm_page_mode_t page_mode;
  struct shadow_page_state shdw_pg_state;
  // nested_paging_t nested_page_state;


  // This structure is how we get interrupts for the guest
  struct vm_intr intr_state;


  // struct vmm_irq_map irq_map;
  vmm_io_map_t io_map;
  // device_map

  struct vmm_dev_mgr  dev_mgr;

  vm_cpu_mode_t cpu_mode;


  struct guest_gprs vm_regs;

  struct vm_ctrl_ops vm_ops;

  void * vmm_data;
};





#endif
