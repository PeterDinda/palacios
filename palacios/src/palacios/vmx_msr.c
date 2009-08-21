/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Andy Gocke <agocke@gmail.com>
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Andy Gocke <agocke@gmail.com>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>
#include <palacios/vm_guest.h>

/* Same as SVM */
static int update_map(struct guest_info * info, uint_t msr, int hook_reads, int hook_writes) {

#if 0
    int index = get_bitmap_index(msr);
    uint_t major = index / 4;
    uint_t minor = (index % 4) * 2;
    uchar_t val = 0;
    uchar_t mask = 0x3;
    uint8_t * bitmap = (uint8_t *)(info->msr_map.arch_data);

    if (hook_reads) {
	val |= 0x1;
    } 
    
    if (hook_writes) {
	val |= 0x2;
    }

    *(bitmap + major) &= ~(mask << minor);
    *(bitmap + major) |= (val << minor);
#endif
    
    return 0;
}

int v3_init_vmx_msr_map(struct guest_info * info) {
   struct v3_msr_map * msr_map = &(info->msr_map);

   msr_map->update_map = update_map;
   
   msr_map->arch_data = V3_VAddr(V3_AllocPages(1));
   memset(msr_map->arch_data, 0, PAGE_SIZE_4KB);

   return 0;
}
