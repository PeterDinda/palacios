#ifndef __VM_GUEST_H
#define __VM_GUEST_H

#include <geekos/vmm_mem.h>
#include <geekos/ktypes.h>
#include <geekos/vmm_io.h>
#include <geekos/vmm_shadow_paging.h>


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


typedef enum {SHADOW_PAGING, NESTED_PAGING} vm_page_mode_t;
typedef enum {REAL, PROTECTED, PROTECTED_PG, PROTECTED_PAE, PROTECTED_PAE_PG, LONG, LONG_PG} vm_cpu_mode_t;

struct guest_info {
  ullong_t rip;


  shadow_map_t mem_map;

  
  vm_page_mode_t page_mode;
  struct shadow_page_state shdw_pg_state;
  // nested_paging_t nested_page_state;


  vmm_io_map_t io_map;
  // device_map

  vm_cpu_mode_t cpu_mode;


  struct guest_gprs vm_regs;

  void * vmm_data;
};




#endif
