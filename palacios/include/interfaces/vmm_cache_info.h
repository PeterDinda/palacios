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
 * Author: Peter Dinda (pdinda@northwestern.edu)
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __VMM_CACHE_INFO
#define __VMM_CACHE_INFO

#include <palacios/vmm_types.h>


typedef enum {V3_CACHE_CODE, V3_CACHE_DATA, V3_CACHE_COMBINED, V3_TLB_CODE, V3_TLB_DATA, V3_TLB_COMBINED} v3_cache_type_t;

struct v3_cache_info {
    v3_cache_type_t type;
    uint32_t level;         // level
    uint32_t size;          // size in bytes (cache) or entries (tlb)
    uint32_t blocksize;     // block size in bytes (caches, ignore for tlbs)
    uint32_t associativity; // n-way, etc. (for 4K pages in case of TLB)
                            // -1 for fully assoc, 0 for disabled/nonexistent
                            
};
    

struct v3_cache_info_iface {
    // level 1 => L1 ("closest"), level 2=> L2, etc.
    // level 0xffffffff => last level shared cache
    int (*get_cache_level)(v3_cache_type_t type, uint32_t level, struct v3_cache_info *info);
};


extern void V3_Init_Cache_Info(struct v3_cache_info_iface * palacios_cache_info);

#ifdef __V3VEE__

int v3_get_cache_info(v3_cache_type_t type, uint32_t level, struct v3_cache_info *info);

#endif

#endif
