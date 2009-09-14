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


#ifndef __VMM_SYM_IFACE_H__
#define __VMM_SYM_IFACE_H__

#ifdef __V3VEE__





struct v3_sym_interface {
    uint64_t magic;


    union {
	uint32_t feature_flags;
	struct {
	    uint_t cur_proc_valid         : 1;
	    uint_t proc_list_valid        : 1;
	} __attribute__((packed));
    } __attribute__((packed));

    addr_t current_proc;
    addr_t proc_list;

    uint8_t pci_pt_map[256 / 8];
} __attribute__((packed));


struct v3_sym_state {
    
    struct v3_sym_interface * sym_page;
    addr_t sym_page_pa;

    uint_t active;
    uint64_t guest_pg_addr;

};

int v3_init_sym_iface(struct guest_info * info);


int v3_sym_map_pci_passthrough(struct guest_info * info, uint_t bus, uint_t dev, uint_t fn);
int v3_sym_unmap_pci_passthrough(struct guest_info * info, uint_t bus, uint_t dev, uint_t fn);



#endif

#endif
