/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2015, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>
#include <interfaces/vmm_cache_info.h>

static struct v3_cache_info_iface *cache_info=0;

void V3_Init_Cache_Info(struct v3_cache_info_iface * palacios_cache_info)
{
    cache_info=palacios_cache_info;
    V3_Print(VM_NONE,VCORE_NONE,"Cache information interface inited\n");
}

int v3_get_cache_info(v3_cache_type_t type, uint32_t level, struct v3_cache_info *info)
{
    if (cache_info && cache_info->get_cache_level) { 
	return cache_info->get_cache_level(type,level,info);
    } else {
	return -1;
    }
}
