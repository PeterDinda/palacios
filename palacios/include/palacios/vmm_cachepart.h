/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2015, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#ifndef __VMM_CACHEPART_H
#define __VMM_CACHEPART_H


#ifdef __V3VEE__ 

#include <palacios/vmm_types.h>

typedef struct v3_cachepart {
    uint64_t  mem_block_size;
    uint64_t  expected_num_colors;
    uint64_t  actual_num_colors;
    uint64_t  min_color;           // with respect to actual colors
    uint64_t  max_color;
    uint64_t  color_shift;
    uint64_t  color_mask;
} v3_cachepart_t ;


struct v3_xml;

int v3_init_cachepart();
int v3_deinit_cachepart();

int v3_init_cachepart_vm(struct v3_vm_info *vm, struct v3_xml *config);
int v3_deinit_cachepart_vm(struct v3_vm_info *vm);

int v3_init_cachepart_core(struct guest_info *core);
int v3_deinit_cachepart_core(struct guest_info *core);

int v3_cachepart_filter(void *paddr, struct v3_cachepart *p);

#endif /* ! __V3VEE__ */


#endif
