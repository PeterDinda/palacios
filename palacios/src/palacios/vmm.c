/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>
#include <palacios/svm.h>
#include <palacios/vmx.h>
#include <palacios/vmm_intr.h>
#include <palacios/vmm_config.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_instrument.h>


v3_cpu_arch_t v3_cpu_type;
struct v3_os_hooks * os_hooks = NULL;



static struct guest_info * allocate_guest() {
  void * info = V3_Malloc(sizeof(struct guest_info));
  memset(info, 0, sizeof(struct guest_info));
  return info;
}



void Init_V3(struct v3_os_hooks * hooks, struct v3_ctrl_ops * vmm_ops) {
  os_hooks = hooks;

  v3_cpu_type = V3_INVALID_CPU;

#ifdef INSTRUMENT_VMM
  v3_init_instrumentation();
#endif

  if (v3_is_svm_capable()) {

    PrintDebug("Machine is SVM Capable\n");
    vmm_ops->allocate_guest = &allocate_guest;
    vmm_ops->config_guest = &v3_config_guest;
    v3_init_SVM(vmm_ops);

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

