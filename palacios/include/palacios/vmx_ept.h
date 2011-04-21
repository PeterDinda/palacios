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

/* The actual format of these data structures is specified as being machine 
   dependent. Thus the lengths of the base address fields are defined as variable. 
   To be safe we assume the maximum(?) size fields 
*/


typedef struct vmx_eptp {
    uint8_t psmt            : 3;
    uint8_t pwl1            : 3;
    uint8_t rsvd1           : 6;
    uint64_t pml_base_addr  : 39;
    uint16_t rsvd2          : 13;
} __attribute__((packed)) vmx_eptp_t;


typedef struct vmx_pml4 {
    uint8_t read            : 1;
    uint8_t write           : 1;
    uint8_t exec            : 1;
    uint8_t rsvd1           : 5;
    uint8_t ignore1         : 4;
    uint64_t pdp_base_addr  : 39;
    uint8_t rsvd2           : 1;
    uint32_t ignore2        : 12;
} __attribute__((packed)) vmx_pml4_t;


typedef struct vmx_pdp_1GB {
    uint8_t read            : 1;
    uint8_t write           : 1;
    uint8_t exec            : 1;
    uint8_t mt              : 3;
    uint8_t ipat            : 1;
    uint8_t large_page      : 1;
    uint8_t ignore1         : 4;
    uint32_t rsvd1          : 18;
    uint32_t page_base_addr : 21;
    uint8_t rsvd2           : 1;
    uint32_t ignore2        : 12;
} __attribute__((packed)) vmx_pdp_1GB_t;

typedef struct vmx_pdp {
    uint8_t read            : 1;
    uint8_t write           : 1;
    uint8_t exec            : 1;
    uint8_t rsvd1           : 4;
    uint8_t large_page      : 1;
    uint8_t ignore1         : 4;
    uint32_t page_base_addr : 39;
    uint8_t rsvd2           : 1;
    uint32_t ignore2        : 12;
} __attribute__((packed)) vmx_pdp_t;


typedef struct vmx_pde_2MB {
    uint8_t read            : 1;
    uint8_t write           : 1;
    uint8_t exec            : 1;
    uint8_t mt              : 3;
    uint8_t ipat            : 1;
    uint8_t large_page      : 1;
    uint8_t ignore1         : 4;
    uint32_t rsvd1          : 9;
    uint32_t page_base_addr : 30;
    uint8_t rsvd2           : 1;
    uint32_t ignore2        : 12;
} __attribute__((packed)) vmx_pde_2MB_t;


typedef struct vmx_pde {
    uint8_t read            : 1;
    uint8_t write           : 1;
    uint8_t exec            : 1;
    uint8_t rsvd1           : 4;
    uint8_t large_page      : 1;
    uint8_t ignore1         : 4;
    uint32_t page_base_addr : 39;
    uint8_t rsvd2           : 1;
    uint32_t ignore2        : 12;
} __attribute__((packed)) vmx_pde_t;



typedef struct vmx_pte {
    uint8_t read            : 1;
    uint8_t write           : 1;
    uint8_t exec            : 1;
    uint8_t mt              : 3;
    uint8_t ipat            : 1;
    uint8_t ignore1         : 5;
    uint32_t page_base_addr : 39;
    uint8_t rsvd2           : 1;
    uint32_t ignore2        : 12;
} __attribute__((packed)) vmx_pte_t;

#endif 

#endif

