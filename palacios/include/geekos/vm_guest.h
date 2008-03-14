#ifndef __VM_GUEST_H
#define __VM_GUEST_H

#include <geekos/vmm_mem.h>
#include <geekos/ktypes.h>
#include <geekos/vmm_io.h>

struct guest_gprs {
  ullong_t rbx;
  ullong_t rcx;
  ullong_t rdx;
  ullong_t rsi;
  ullong_t rdi;
  ullong_t rbp;

};




typedef struct guest_info {
  ullong_t rip;
  ullong_t rsp;

  vmm_mem_list_t mem_list;
  vmm_mem_layout_t mem_layout;

  vmm_io_map_t io_map;
  // device_map


  struct guest_gprs vm_regs;

  void * page_tables;
  void * vmm_data;
} guest_info_t;






#endif
