#include <geekos/vmm.h>
#include <geekos/svm.h>
#include <geekos/vmx.h>


uint_t vmm_cpu_type;




struct vmm_os_hooks * os_hooks = NULL;



void Init_VMM(struct vmm_os_hooks * hooks, struct vmm_ctrl_ops * vmm_ops) {
  vmm_cpu_type = VMM_INVALID_CPU;

  os_hooks = hooks;



  if (is_svm_capable()) {
    vmm_cpu_type = VMM_SVM_CPU;
    PrintDebug("Machine is SVM Capable\n");

    Init_SVM(vmm_ops);

  } else if (is_vmx_capable()) {
    vmm_cpu_type = VMM_VMX_CPU;
    PrintDebug("Machine is VMX Capable\n");
    Init_VMX();
  } else {
    PrintDebug("CPU has no virtualization Extensions\n");
  }
}
