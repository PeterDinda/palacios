#ifndef __VM_GUEST_H
#define __VM_GUEST_H

#ifdef __V3VEE__

#include "test.h"

typedef ullong_t v3_reg_t;
typedef ulong_t addr_t;



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








typedef enum {SHADOW_PAGING, NESTED_PAGING} vmm_paging_mode_t;
typedef enum {REAL, /*UNREAL,*/ PROTECTED, PROTECTED_PAE, LONG, LONG_32_COMPAT, LONG_16_COMPAT} vm_cpu_mode_t;
typedef enum {PHYSICAL_MEM, VIRTUAL_MEM} vm_mem_mode_t;

struct guest_info {
  addr_t rip;

  vm_cpu_mode_t cpu_mode;
  struct v3_gprs vm_regs;
  struct v3_ctrl_regs ctrl_regs;
  struct v3_segments segments;



};



void PrintV3Segments(struct guest_info * info);
void PrintV3CtrlRegs(struct guest_info * info);
void PrintV3GPRs(struct guest_info * info);

#endif



#endif
