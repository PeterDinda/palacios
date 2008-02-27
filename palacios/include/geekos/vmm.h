#ifndef __VMM_H
#define __VMM_H


#define VMM_INVALID_CPU 0
#define VMM_VMX_CPU 1
#define VMM_SVM_CPU 2


/* This will contain function pointers that provide OS services */
struct vmm_os_hooks {
  

};




/* This will contain Function pointers that control the VMs */
struct vmm_ctrl_ops {
  

};



void Init_VMM();



#endif
