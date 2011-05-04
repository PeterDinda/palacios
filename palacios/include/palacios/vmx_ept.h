/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2011, Jack Lange <jacklange@cs.pitt.edu> 
 * All rights reserved.
 *
 * Author: Jack Lange <jacklange@cs.pitt.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#ifndef __VMX_EPT_H__
#define __VMX_EPT_H__


#ifdef __V3VEE__

#include <palacios/vmx_hw_info.h>

/* The actual format of these data structures is specified as being machine 
   dependent. Thus the lengths of the base address fields are defined as variable. 
   To be safe we assume the maximum(?) size fields 

   From Intel Manual...
   N is the physical-address width supported by the logical processor. Software can determine a processor's
   physical-address width by executing CPUID with 80000008H in EAX. The physical address
   width is returned in bits 7:0 of EAX.
*/


struct ept_exit_qual {
    union {
	uint64_t value;
	struct {
	    uint64_t rd_op       : 1;
	    uint64_t wr_op       : 1;
	    uint64_t ifetch      : 1;
	    uint64_t present     : 1;
	    uint64_t write       : 1;
	    uint64_t exec        : 1;
	    uint64_t rsvd1       : 1;
	    uint64_t addr_valid  : 1;
	    uint64_t addr_type   : 1;
	    uint64_t rsvd2       : 1;
	    uint64_t nmi_unblock : 1;
	    uint64_t rsvd3      : 53;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));




typedef struct vmx_eptp {
    uint64_t psmt            : 3; /* (0=UC, 6=WB) */
    uint64_t pwl1            : 3; /* 1 less than EPT page-walk length (?)*/
    uint64_t rsvd1           : 6;
    uint64_t pml_base_addr  : 39; 
    uint16_t rsvd2          : 13;
} __attribute__((packed)) vmx_eptp_t;


typedef struct ept_pml4 {
    uint64_t read            : 1;
    uint64_t write           : 1;
    uint64_t exec            : 1;
    uint64_t rsvd1           : 5;
    uint64_t ignore1         : 4;
    uint64_t pdp_base_addr  : 39;
    uint64_t rsvd2           : 1;
    uint64_t ignore2        : 12;
} __attribute__((packed)) ept_pml4_t;


typedef struct ept_pdp_1GB {
    uint64_t read            : 1;
    uint64_t write           : 1;
    uint64_t exec            : 1;
    uint64_t mt              : 3;
    uint64_t ipat            : 1;
    uint64_t large_page      : 1;
    uint64_t ignore1         : 4;
    uint64_t rsvd1          : 18;
    uint64_t page_base_addr : 21;
    uint64_t rsvd2           : 1;
    uint64_t ignore2        : 12;
} __attribute__((packed)) ept_pdp_1GB_t;

typedef struct ept_pdp {
    uint64_t read            : 1;
    uint64_t write           : 1;
    uint64_t exec            : 1;
    uint64_t rsvd1           : 4;
    uint64_t large_page      : 1;
    uint64_t ignore1         : 4;
    uint64_t pd_base_addr   : 39;
    uint64_t rsvd2           : 1;
    uint64_t ignore2        : 12;
} __attribute__((packed)) ept_pdp_t;


typedef struct ept_pde_2MB {
    uint64_t read            : 1;
    uint64_t write           : 1;
    uint64_t exec            : 1;
    uint64_t mt              : 3;
    uint64_t ipat            : 1;
    uint64_t large_page      : 1;
    uint64_t ignore1         : 4;
    uint64_t rsvd1          : 9;
    uint64_t page_base_addr : 30;
    uint64_t rsvd2           : 1;
    uint64_t ignore2        : 12;
} __attribute__((packed)) ept_pde_2MB_t;


typedef struct ept_pde {
    uint64_t read            : 1;
    uint64_t write           : 1;
    uint64_t exec            : 1;
    uint64_t rsvd1           : 4;
    uint64_t large_page      : 1;
    uint64_t ignore1         : 4;
    uint64_t pt_base_addr   : 39;
    uint64_t rsvd2           : 1;
    uint64_t ignore2        : 12;
} __attribute__((packed)) ept_pde_t;



typedef struct ept_pte {
    uint64_t read            : 1;
    uint64_t write           : 1;
    uint64_t exec            : 1;
    uint64_t mt              : 3;
    uint64_t ipat            : 1;
    uint64_t ignore1         : 5;
    uint64_t page_base_addr  : 39;
    uint64_t rsvd2           : 1;
    uint64_t ignore2         : 12;
} __attribute__((packed)) ept_pte_t;

int v3_init_ept(struct guest_info * core, struct vmx_hw_info * hw_info);
int v3_handle_ept_fault(struct guest_info * core, addr_t fault_addr, struct ept_exit_qual * ept_qual);


#endif 

#endif

