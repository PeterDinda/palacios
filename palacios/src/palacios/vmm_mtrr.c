/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2011, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2011, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm_mtrr.h>
#include <palacios/vmm_extensions.h>
#include <palacios/vmm.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_msr.h>


#define MTRR_CAP_MSR       0x00fe
#define MTRR_PHYS_BASE_0   0x0200
#define MTRR_PHYS_BASE_1   0x0202
#define MTRR_PHYS_BASE_2   0x0204
#define MTRR_PHYS_BASE_3   0x0206
#define MTRR_PHYS_BASE_4   0x0208
#define MTRR_PHYS_BASE_5   0x020a
#define MTRR_PHYS_BASE_6   0x020c
#define MTRR_PHYS_BASE_7   0x020e
#define MTRR_PHYS_MASK_0   0x0201
#define MTRR_PHYS_MASK_1   0x0203
#define MTRR_PHYS_MASK_2   0x0205
#define MTRR_PHYS_MASK_3   0x0207
#define MTRR_PHYS_MASK_4   0x0209
#define MTRR_PHYS_MASK_5   0x020b
#define MTRR_PHYS_MASK_6   0x020d
#define MTRR_PHYS_MASK_7   0x020f
#define MTRR_FIX_64K_00000 0x0250
#define MTRR_FIX_16K_80000 0x0258
#define MTRR_FIX_16K_A0000 0x0259
#define MTRR_FIX_4K_C0000  0x0268
#define MTRR_FIX_4K_C8000  0x0269
#define MTRR_FIX_4K_D0000  0x026a
#define MTRR_FIX_4K_D8000  0x026b
#define MTRR_FIX_4K_E0000  0x026c
#define MTRR_FIX_4K_E8000  0x026d
#define MTRR_FIX_4K_F0000  0x026e
#define MTRR_FIX_4K_F8000  0x026f


struct mtrr_cap {


};


struct mtrr_state {
    struct mtrr_cap cap;
    
};


static int mtrr_cap_read(struct guest_info * core, uint32_t msr, struct v3_msr * dst, void * priv_data) {
    return 0;
}

static int mtrr_cap_write(struct guest_info * core, uint32_t msr, struct v3_msr src, void * priv_data) {

    return 0;
}



static int init_mtrrs(struct v3_vm_info * vm, v3_cfg_tree_t * cfg, void ** priv_data) {


    V3_Print("Intializing MTRR extension\n");

    v3_hook_msr(vm, MTRR_CAP_MSR, mtrr_cap_read, mtrr_cap_write, NULL);
        

    return 0;
}


static struct v3_extension_impl mtrr_ext = {
    .name = "MTRRS",
    .init = init_mtrrs,
    .deinit = NULL,
    .core_init = NULL,
    .core_deinit = NULL
};



register_extension(&mtrr_ext);
