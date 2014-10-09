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


#ifndef __VMM_DEBUG_H__
#define __VMM_DEBUG_H__


#ifdef __V3VEE__

#include <palacios/vmm.h>
#include <palacios/vmm_regs.h>

#define NUM_IDT_ENTRIES 256
#define NUM_GDT_ENTRIES 16

struct segment_selector {
    uint8_t  rpl        :  2;
    uint8_t  ti         :  1;
    uint16_t index      : 13;
}__attribute__((packed));

struct int_trap_gate_long {
    uint16_t offset_lo  : 16;
    uint16_t selector   : 16;
    uint8_t  ist        :  3;
    uint8_t  ign        :  5;
    uint8_t  type       :  4;
    uint8_t  s          :  1;
    uint8_t  dpl        :  2;
    uint8_t  p          :  1;
    uint16_t offset_mid : 16;
    uint32_t offset_hi  : 32;
    uint32_t ign2       : 32;
}__attribute__((packed));

struct call_gate_long {
    uint16_t offset_lo  : 16;
    uint16_t selector   : 16;
    uint8_t  ign        :  8;
    uint8_t  type       :  4;
    uint8_t  s          :  1;
    uint8_t  dpl        :  2;
    uint8_t  p          :  1;
    uint16_t offset_mid : 16;
    uint32_t offset_hi  : 32;
    uint8_t  ign2       :  8;
    uint8_t  count      :  5;
    uint32_t ign3       : 19;
}__attribute__((packed));

struct system_desc_long {
    uint16_t limit_lo   : 16;
    uint16_t base_lo    : 16;
    uint8_t  base_mid1  :  8;
    uint8_t  type       :  4;
    uint8_t  s          :  1;
    uint8_t  dpl        :  2;
    uint8_t  p          :  1;
    uint8_t  limit_hi   :  4;
    uint8_t  avl        :  1;
    uint8_t  ign        :  2;
    uint8_t  g          :  1;
    uint8_t  base_mid2  :  8;
    uint32_t base_hi    : 32;
    uint8_t  ign2       :  8;
    uint8_t  lgcy_type  :  5;
    uint32_t ign3       : 19;
}__attribute__((packed));

struct data_desc_long {
    uint16_t limit_lo   : 16;
    uint16_t base_lo    : 16;
    uint8_t  base_mid   :  8;
    uint8_t  a          :  1;
    uint8_t  w          :  1;
    uint8_t  e          :  1;
    uint8_t  zero       :  1;
    uint8_t  one        :  1;
    uint8_t  dpl        :  2;
    uint8_t  p          :  1;
    uint8_t  limit_hi   :  4;
    uint8_t  avl        :  1;
    uint8_t  ign        :  1;
    uint8_t  db         :  1;
    uint8_t  g          :  1;
    uint8_t  base_hi    :  8;
}__attribute__((packed));

struct code_desc_long {
    uint16_t limit_lo   : 16;
    uint16_t base_lo    : 16;
    uint8_t  base_mid   :  8;
    uint8_t  a          :  1;
    uint8_t  r          :  1;
    uint8_t  c          :  1;
    uint8_t  one1       :  1;
    uint8_t  one2       :  1;
    uint8_t  dpl        :  2;
    uint8_t  p          :  1;
    uint8_t  limit_hi   :  4;
    uint8_t  avl        :  1;
    uint8_t  l          :  1;
    uint8_t  d          :  1;
    uint8_t  g          :  1;
    uint8_t  base_hi    :  8;
}__attribute__((packed));

struct int_trap_gate_lgcy {
    uint16_t offset_lo  : 16;
    uint16_t selector   : 16;
    uint8_t  ign        :  8;
    uint8_t  type       :  4;
    uint8_t  s          :  1;
    uint8_t  dpl        :  2;
    uint8_t  p          :  1;
    uint16_t offset_hi  : 16;
}__attribute__((packed));

struct call_gate_lgcy {
    uint16_t offset_lo  : 16;
    uint16_t selector   : 16;
    uint8_t  count      :  4;
    uint8_t  ign        :  4;
    uint8_t  type       :  4;
    uint8_t  s          :  1;
    uint8_t  dpl        :  2;
    uint8_t  p          :  1;
    uint16_t offset_hi  : 16;
}__attribute__((packed));

struct trap_gate_lgcy {
    uint16_t ign        : 16;
    uint16_t selector   : 16;
    uint8_t  ign2       :  8;
    uint8_t  type       :  4;
    uint8_t  s          :  1;
    uint8_t  dpl        :  2;
    uint8_t  p          :  1;
    uint16_t ign3       : 16;
}__attribute__((packed));

struct system_desc_lgcy {
    uint16_t limit_lo   : 16;
    uint16_t base_lo    : 16;
    uint8_t  base_mid   :  8;
    uint8_t  type       :  4;
    uint8_t  s          :  1;
    uint8_t  dpl        :  2;
    uint8_t  p          :  1;
    uint8_t  limit_hi   :  4;
    uint8_t  avl        :  1;
    uint8_t  ign        :  2;
    uint8_t  g          :  1;
    uint8_t  base_hi    :  8;
}__attribute__((packed));

struct data_desc_lgcy {
    uint16_t limit_lo   : 16;
    uint16_t base_lo    : 16;
    uint8_t  base_mid   :  8;
    uint8_t  type       :  4;
    uint8_t  s          :  1;
    uint8_t  dpl        :  2;
    uint8_t  p          :  1;
    uint8_t  limit_hi   :  4;
    uint8_t  avl        :  1;
    uint8_t  ign        :  1;
    uint8_t  db         :  1;
    uint8_t  g          :  1;
    uint8_t  base_hi    :  8;
}__attribute__((packed));

struct code_desc_lgcy {
    uint16_t limit_lo   : 16;
    uint16_t base_lo    : 16;
    uint8_t  base_mid   :  8;
    uint8_t  type       :  4;
    uint8_t  s          :  1;
    uint8_t  dpl        :  2;
    uint8_t  p          :  1;
    uint8_t  limit_hi   :  4;
    uint8_t  avl        :  1;
    uint8_t  ign        :  1;
    uint8_t  d          :  1;
    uint8_t  g          :  1;
    uint8_t  base_hi    :  8;
}__attribute__((packed));
        

struct selector_error_code {
    uint8_t  ext        :  1;
    uint8_t  idt        :  1;
    uint8_t  ti         :  1;
    uint16_t index      : 13;
    uint16_t ign        : 16;
}__attribute__((packed));


int v3_init_vm_debugging(struct v3_vm_info * vm);

void v3_print_guest_state(struct guest_info * core);
void v3_print_arch_state(struct guest_info * core);

void v3_print_segments(struct v3_segments * segs);
void v3_print_ctrl_regs(struct guest_info * core);
void v3_print_GPRs(struct guest_info * core);

void v3_print_backtrace(struct guest_info * core);
void v3_print_stack(struct guest_info * core);
void v3_print_guest_state_all(struct v3_vm_info * vm);

void v3_print_idt(struct guest_info * core, addr_t idtr_base);
void v3_print_gdt(struct guest_info * core, addr_t gdtr_base);
void v3_print_gp_error(struct guest_info * core, addr_t exit_info1);

#endif // !__V3VEE__

#endif
