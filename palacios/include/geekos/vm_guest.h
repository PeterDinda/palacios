#ifndef __VM_GUEST_H
#define __VM_GUEST_H

#include <geekos/vmm_mem.h>
#include <geekos/ktypes.h>
#include <geekos/vmm_io.h>
//#include <geekos/vmm_paging.h>



struct guest_info;


#include <geekos/vmm_shadow_paging.h>

struct guest_gprs {
  ullong_t rbx;
  ullong_t rcx;
  ullong_t rdx;
  ullong_t rsi;
  ullong_t rdi;
  ullong_t rbp;

};


typedef enum {SHADOW_PAGING, NESTED_PAGING} vm_page_mode_t;
typedef enum {REAL, PROTECTED, PROTECTED_PG, PROTECTED_PAE, PROTECTED_PAE_PG, LONG, LONG_PG} vm_cpu_mode_t;

struct guest_info {
  ullong_t rip;
  ullong_t rsp;

  shadow_map_t mem_map;

  
  vm_page_mode_t page_mode;
  struct shadow_page_state  shdw_pg_state;
  // nested_paging_t nested_page_state;


  vmm_io_map_t io_map;
  // device_map

  vm_cpu_mode_t cpu_mode;


  struct guest_gprs vm_regs;

  void * vmm_data;
};




#endif
