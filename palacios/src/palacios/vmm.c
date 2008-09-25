/* (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> */
/* (c) 2008, The V3VEE Project <http://www.v3vee.org> */

#include <palacios/vmm.h>
#include <palacios/svm.h>
#include <palacios/vmx.h>
#include <palacios/vmm_intr.h>
#include <palacios/vmm_config.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_decoder.h>

v3_cpu_arch_t v3_cpu_type;
struct vmm_os_hooks * os_hooks = NULL;



struct guest_info * allocate_guest() {
  void * info = V3_Malloc(sizeof(struct guest_info));
  memset(info, 0, sizeof(struct guest_info));
  return info;
}



void Init_V3(struct vmm_os_hooks * hooks, struct vmm_ctrl_ops * vmm_ops) {
  os_hooks = hooks;

  v3_cpu_type = V3_INVALID_CPU;

  init_decoder();

  if (is_svm_capable()) {

    PrintDebug("Machine is SVM Capable\n");
    vmm_ops->allocate_guest = &allocate_guest;
    vmm_ops->config_guest = &config_guest;
    Init_SVM(vmm_ops);

    /*
  } else if (is_vmx_capable()) {
    vmm_cpu_type = VMM_VMX_CPU;
    PrintDebug("Machine is VMX Capable\n");
    //Init_VMX();*/
  } else {
    PrintDebug("CPU has no virtualization Extensions\n");
  }
}


// Get CPU Type..

