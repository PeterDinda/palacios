#ifndef __VM_GUEST_H
#define __VM_GUEST_H

#include <geekos/svm.h>
#include <geekos/vmx.h>





typedef struct guest_state {
  reg_ex_t rip;
  reg_ex_t rsp;

  
  void * arch_data;
} guest_state_t;








#endif
