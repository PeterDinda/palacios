#include <geekos/svm.h>


extern uint_t cpuid_ecx(uint_t op);
extern uint_t cpuid_edx(uint_t op);
extern void Get_MSR(uint_t MSR, ulong_t * high_byte, ulong_t * low_byte); 
extern void Set_MSR(uint_t MSR, ulong_t high_byte, ulong_t low_byte);

/* Checks machine SVM capability */
/* Implemented from: AMD Arch Manual 3, sect 15.4 */ 
int is_svm_capable() {
  uint_t ret =  cpuid_ecx(CPUID_FEATURE_IDS);
  ulong_t vm_cr_low = 0, vm_cr_high = 0;


  if ((ret & CPUID_FEATURE_IDS_ecx_svm_avail) == 0) {
    Print("SVM Not Available\n");
    return 0;
  } 

  Get_MSR(SVM_VM_CR_MSR, &vm_cr_high, &vm_cr_low);

  if ((vm_cr_low & SVM_VM_CR_MSR_svmdis) == 0) {
    return 1;
  }

  ret = cpuid_edx(CPUID_SVM_REV_AND_FEATURE_IDS);
  
  if ((ret & CPUID_SVM_REV_AND_FEATURE_IDS_edx_svml) == 0) {
    Print("SVM BIOS Disabled, not unlockable\n");
  } else {
    Print("SVM is locked with a key\n");
  }

  return 0;
}

void Init_SVM() {
  ulong_t msr_val_low = 0, msr_val_high = 0;

  Get_MSR(EFER_MSR, &msr_val_high, &msr_val_low);
  msr_val_low |= EFER_MSR_svm_enable;
  Set_MSR(EFER_MSR, 0, msr_val_low);
  
  Print("SVM Inited\n");

  return;
}
