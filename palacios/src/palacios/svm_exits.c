/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2012, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2012, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#include <palacios/vmm.h>
#include <palacios/vmcb.h>
#include <palacios/vmm_exits.h>



static int enable_exit(struct guest_info * core, v3_exit_type_t exit_type) {
    vmcb_ctrl_t * ctrl_area = GET_VMCB_CTRL_AREA((vmcb_t *)(core->vmm_data));

    switch (exit_type) {

	case V3_EXIT_RDTSC:
	    ctrl_area->instrs.RDTSC = 1;
	    break;
	case V3_EXIT_RDTSCP:
	    ctrl_area->svm_instrs.RDTSCP = 1;
	    break;

	default:
	    PrintError("Unhandled Exit Type (%d)\n", exit_type);
	    return -1;
    }

    return 0;
}


static int disable_exit(struct guest_info * core, v3_exit_type_t exit_type) {
    vmcb_ctrl_t * ctrl_area = GET_VMCB_CTRL_AREA((vmcb_t *)(core->vmm_data));

    switch (exit_type) {

	case V3_EXIT_RDTSC:
	    ctrl_area->instrs.RDTSC = 0;
	    break;
	case V3_EXIT_RDTSCP:
	    ctrl_area->svm_instrs.RDTSCP = 0;
	    break;

	default:
	    PrintError("Unhandled Exit Type (%d)\n", exit_type);
	    return -1;
    }

    return 0;

}


int v3_init_svm_exits(struct v3_vm_info * vm) {

    int ret = 0;

    ret |= v3_register_exit(vm, V3_EXIT_RDTSC, enable_exit, disable_exit);
    ret |= v3_register_exit(vm, V3_EXIT_RDTSCP, enable_exit, disable_exit);
    
    return ret;
}
