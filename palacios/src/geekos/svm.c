#include <geekos/svm.h>


extern uint_t cpuid_ecx(uint_t op);

void Init_SVM() {

  uint_t ret =  cpuid_ecx(CPUID_ECX_FEATURE_IDS);

  if (ret & CPUID_SVM_AVAIL) {
    PrintBoth("SVM Available\n");
  }


}
