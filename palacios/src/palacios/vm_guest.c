#include <palacios/vm_guest.h>
#include <palacios/vmm.h>

void PrintV3Segments(struct v3_segments * segs) {
  int i = 0;
  struct v3_segment * seg_ptr = (struct v3_segment *)segs;
  
  char *seg_names[] = {"CS", "DS" , "ES", "FS", "GS", "SS" , "LDTR", "GDTR", "IDTR", "TR", NULL};
  PrintDebug("Segments\n");

  for (i = 0; seg_names[i] != NULL; i++) {

    PrintDebug("\t%s: Sel=%x, base=%x, limit=%x\n", seg_names[i], seg_ptr[i].selector, seg_ptr[i].base, seg_ptr[i].limit);

  }

}


void PrintV3CtrlRegs(struct v3_ctrl_regs * regs) {
  int i = 0;
  v3_reg_t * reg_ptr = (v3_reg_t *)regs;
  char * reg_names[] = {"CR0", "CR2", "CR3", "CR4", "CR8", "FLAGS", NULL};

  PrintDebug("32 bit Ctrl Regs:\n");

  for (i = 0; reg_names[i] != NULL; i++) {
    PrintDebug("\t%s=0x%x\n", reg_names[i], reg_ptr[i]);  
  }
}
