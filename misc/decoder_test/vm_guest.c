#include "vm_guest.h"




void PrintV3Segments(struct guest_info * info) {
  struct v3_segments * segs = &(info->segments);
  int i = 0;
  struct v3_segment * seg_ptr;

  seg_ptr=(struct v3_segment *)segs;
  
  char *seg_names[] = {"CS", "DS" , "ES", "FS", "GS", "SS" , "LDTR", "GDTR", "IDTR", "TR", NULL};
  PrintDebug("Segments\n");

  for (i = 0; seg_names[i] != NULL; i++) {

    PrintDebug("\t%s: Sel=%x, base=%x, limit=%x (at 0x%x)\n", seg_names[i], seg_ptr[i].selector, (uint_t)seg_ptr[i].base, seg_ptr[i].limit, &seg_ptr[i]);

  }

}


void PrintV3CtrlRegs(struct guest_info * info) {
  struct v3_ctrl_regs * regs = &(info->ctrl_regs);
  int i = 0;
  v3_reg_t * reg_ptr;
  char * reg_names[] = {"CR0", "CR2", "CR3", "CR4", "CR8", "FLAGS", NULL};

  reg_ptr= (v3_reg_t *)regs;

  PrintDebug("32 bit Ctrl Regs:\n");

  for (i = 0; reg_names[i] != NULL; i++) {
    PrintDebug("\t%s=0x%x (at=0x%x)\n", reg_names[i], (uint_t)reg_ptr[i], &reg_ptr[i]);  
  }
}


void PrintV3GPRs(struct guest_info * info) {
  struct v3_gprs * regs = &(info->vm_regs);
  int i = 0;
  v3_reg_t * reg_ptr;
  char * reg_names[] = { "RDI", "RSI", "RBP", "RSP", "RBX", "RDX", "RCX", "RAX", NULL};

  reg_ptr= (v3_reg_t *)regs;

  PrintDebug("32 bit GPRs:\n");

  for (i = 0; reg_names[i] != NULL; i++) {
    PrintDebug("\t%s=0x%x (at 0x%x)\n", reg_names[i], (uint_t)reg_ptr[i], &reg_ptr[i]);  
  }
}
