#include <geekos/svm.h>


extern struct vmm_os_hooks * os_hooks;

extern uint_t cpuid_ecx(uint_t op);
extern uint_t cpuid_edx(uint_t op);
extern void Get_MSR(uint_t MSR, uint_t * high_byte, uint_t * low_byte); 
extern void Set_MSR(uint_t MSR, uint_t high_byte, uint_t low_byte);

/* Checks machine SVM capability */
/* Implemented from: AMD Arch Manual 3, sect 15.4 */ 
int is_svm_capable() {
  uint_t ret =  cpuid_ecx(CPUID_FEATURE_IDS);
  uint_t vm_cr_low = 0, vm_cr_high = 0;


  if ((ret & CPUID_FEATURE_IDS_ecx_svm_avail) == 0) {
    PrintDebug("SVM Not Available\n");
    return 0;
  } 

  Get_MSR(SVM_VM_CR_MSR, &vm_cr_high, &vm_cr_low);

  if ((vm_cr_low & SVM_VM_CR_MSR_svmdis) == 0) {
    return 1;
  }

  ret = cpuid_edx(CPUID_SVM_REV_AND_FEATURE_IDS);
  
  if ((ret & CPUID_SVM_REV_AND_FEATURE_IDS_edx_svml) == 0) {
    PrintDebug("SVM BIOS Disabled, not unlockable\n");
  } else {
    PrintDebug("SVM is locked with a key\n");
  }

  return 0;
}



void Init_SVM() {
  reg_ex_t msr;
  void * host_state;


  // setup 
  Get_MSR(EFER_MSR, &(msr.e_reg.high), &(msr.e_reg.low));
  msr.e_reg.low |= EFER_MSR_svm_enable;
  Set_MSR(EFER_MSR, 0, msr.e_reg.low);
  
  PrintDebug("SVM Enabled\n");


  // Setup the host state save area
  host_state = os_hooks->Allocate_Pages(1);
  
  msr.e_reg.high = 0;
  msr.e_reg.low = (uint_t)host_state;

  Set_MSR(SVM_VM_HSAVE_PA_MSR, msr.e_reg.high, msr.e_reg.low);


  return;
}




void Allocate_VMCB() {
  void * vmcb_page = os_hooks->Allocate_Pages(1);


  memset(vmcb_page, 0, 4096);
}
