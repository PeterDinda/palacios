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

#ifndef __VMM_SYMSPY_H__
#define __VMM_SYMSPY_H__

#ifdef __V3VEE__

#include <palacios/vmm.h>



struct v3_symspy_global_page {
    uint64_t magic;

    union {
	uint32_t feature_flags;
	struct {
	    uint8_t pci_map_valid      : 1;
	    uint8_t symmod_enabled     : 1;
	    uint8_t sec_symmod_enabled : 1;
	} __attribute__((packed));
    } __attribute__((packed));
    
    uint8_t pci_pt_map[(4 * 256) / 8]; // we're hardcoding this: (4 busses, 256 max devs)

} __attribute__((packed));

struct v3_symspy_local_page {
    uint64_t magic;

    union { 
	uint32_t symcall_flags;
	struct {
	    uint32_t sym_call_active        : 1;
	    uint32_t sym_call_enabled       : 1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));



struct v3_symspy_global_state {
    struct v3_symspy_global_page * sym_page;

    addr_t global_page_pa;
    uint64_t global_guest_pa;

    int active; // activated when symbiotic page MSR is written
};


struct v3_symspy_local_state {
    struct v3_symspy_local_page * local_page;

    addr_t local_page_pa;
    uint64_t local_guest_pa;

    int active;  // activated when symbiotic page MSR is written
};




int v3_init_symspy_vm(struct v3_vm_info * vm, struct v3_symspy_global_state * state);
int v3_init_symspy_core(struct guest_info * core, struct v3_symspy_local_state * state);



int v3_sym_map_pci_passthrough(struct v3_vm_info * vm, uint_t bus, uint_t dev, uint_t fn);
int v3_sym_unmap_pci_passthrough(struct v3_vm_info * vm, uint_t bus, uint_t dev, uint_t fn);




struct v3_symspy_global_page * v3_sym_get_symspy_vm(struct v3_vm_info * vm);
struct v3_symspy_local_page * v3_sym_get_symspy_core(struct guest_info * core);

#endif

#endif
