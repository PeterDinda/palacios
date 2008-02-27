#include <geekos/vmm.h>
#include <geekos/svm.h>
#include <geekos/vmx.h>

uint_t vmm_cpu_type;



void Init_VMM() {
  vmm_cpu_type = VMM_INVALID_CPU;

  if (is_svm_capable()) {
    vmm_cpu_type = VMM_SVM_CPU;
    Print("Machine is SVM Capable\n");
    Init_SVM();
  } else if (is_vmx_capable()) {
    vmm_cpu_type = VMM_VMX_CPU;
    Print("Machine is VMX Capable\n");
    Init_VMX();
  } else {
    PrintBoth("CPU has no virtualization Extensions\n");
  }
}
