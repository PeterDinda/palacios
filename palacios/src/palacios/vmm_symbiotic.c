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


#include <palacios/vmm_symbiotic.h>
#include <palacios/vm_guest.h>


#define SYM_CPUID_NUM 0x90000000

static int cpuid_fn(struct guest_info * core, uint32_t cpuid, 
		    uint32_t * eax, uint32_t * ebx,
		    uint32_t * ecx, uint32_t * edx,
		    void * private_data) {
    extern v3_cpu_arch_t v3_cpu_types[];

    *eax = *(uint32_t *)"V3V";

    if ((v3_cpu_types[0] == V3_SVM_CPU) || 
	(v3_cpu_types[0] == V3_SVM_REV3_CPU)) {
	*ebx = *(uint32_t *)"SVM";
    } else if ((v3_cpu_types[0] == V3_VMX_CPU) || 
	       (v3_cpu_types[0] == V3_VMX_EPT_CPU)) {
	*ebx = *(uint32_t *)"VMX";
    }


    return 0;
}


int v3_init_symbiotic_vm(struct v3_vm_info * vm) {
    struct v3_sym_vm_state * vm_state = &(vm->sym_vm_state);
    memset(vm_state, 0, sizeof(struct v3_sym_vm_state));

    v3_hook_cpuid(vm, SYM_CPUID_NUM, cpuid_fn, NULL);

    if (v3_init_symspy_vm(vm, &(vm_state->symspy_state)) == -1) {
	PrintError("Error initializing global SymSpy state\n");
	return -1;
    }

#ifdef V3_CONFIG_SYMCALL
    if (v3_init_symcall_vm(vm) == -1) {
	PrintError("Error intializing global SymCall state\n");
	return -1;
    }
#endif

#ifdef V3_CONFIG_SYMMOD
    if (v3_init_symmod_vm(vm, vm->cfg_data->cfg) == -1) {
	PrintError("Error initializing global SymMod state\n");
	return -1;
    }
#endif


    return 0;
}


int v3_deinit_symbiotic_vm(struct v3_vm_info * vm) {

#ifdef V3_CONFIG_SYMMOD
    if (v3_deinit_symmod_vm(vm) == -1) {
	PrintError("Error deinitializing global SymMod state\n");
	return -1;
    }
#endif

    v3_unhook_cpuid(vm, SYM_CPUID_NUM);

    
    return 0;
}



int v3_init_symbiotic_core(struct guest_info * core) {
    struct v3_sym_core_state * core_state = &(core->sym_core_state);
    memset(core_state, 0, sizeof(struct v3_sym_core_state));
    

    if (v3_init_symspy_core(core, &(core_state->symspy_state)) == -1) {
	PrintError("Error intializing local SymSpy state\n");
	return -1;
    }

    return 0;
}


int v3_deinit_symbiotic_core(struct guest_info * core) {

    return 0;
}
