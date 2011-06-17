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


#ifndef __VMM_SYMBIOTIC_H__
#define __VMM_SYMBIOTIC_H__

#ifdef __V3VEE__

#include <palacios/vmm.h>

#include <palacios/vmm_symspy.h>

#ifdef V3_CONFIG_SYMCALL
#include <palacios/vmm_symcall.h>
#endif

#ifdef V3_CONFIG_SYMMOD
#include <palacios/vmm_symmod.h>
#endif



struct v3_sym_vm_state {
    struct v3_symspy_global_state symspy_state;

#ifdef V3_CONFIG_SYMMOD
    struct v3_symmod_state symmod_state;
#endif
};


struct v3_sym_core_state {
    struct v3_symspy_local_state symspy_state;
    
#ifdef V3_CONFIG_SYMCALL
    struct v3_symcall_state symcall_state;
#endif

};


int v3_init_symbiotic_vm(struct v3_vm_info * vm);
int v3_deinit_symbiotic_vm(struct v3_vm_info * vm);

int v3_init_symbiotic_core(struct guest_info * core);
int v3_deinit_symbiotic_core(struct guest_info * core);


#endif

#endif
