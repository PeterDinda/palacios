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

#include <palacios/svm_msr.h>
#include <palacios/vmm_msr.h>
#include <palacios/vmm_sprintf.h>
#include <palacios/vmm_list.h>
#include <palacios/vm_guest.h>

#define PENTIUM_MSRS_START            0x00000000
#define PENTIUM_MSRS_END              0x00001fff
#define AMD_6_GEN_MSRS_START          0xc0000000
#define AMD_6_GEN_MSRS_END            0xc0001fff
#define AMD_7_8_GEN_MSRS_START        0xc0010000
#define AMD_7_8_GEN_MSRS_END          0xc0011fff

#define PENTIUM_MSRS_INDEX            (0)
#define AMD_6_GEN_MSRS_INDEX          (0x2000)
#define AMD_7_8_GEN_MSRS_INDEX        (0x4000)



static int get_bitmap_index(uint_t msr) {
    if ((msr >= PENTIUM_MSRS_START) && 
	(msr <= PENTIUM_MSRS_END)) {
	return (PENTIUM_MSRS_INDEX + (msr - PENTIUM_MSRS_START));
    } else if ((msr >= AMD_6_GEN_MSRS_START) && 
	       (msr <= AMD_6_GEN_MSRS_END)) {
	return (AMD_6_GEN_MSRS_INDEX + (msr - AMD_6_GEN_MSRS_START));
    } else if ((msr >= AMD_7_8_GEN_MSRS_START) && 
	       (msr <= AMD_7_8_GEN_MSRS_END)) {
	return (AMD_7_8_GEN_MSRS_INDEX + (msr - AMD_7_8_GEN_MSRS_START));
    } else {
	PrintError("MSR out of range (MSR=0x%x)\n", msr);
	return -1;
    }
}


static int update_map(struct v3_vm_info * vm, uint_t msr, int hook_reads, int hook_writes) {
    int index = get_bitmap_index(msr);
    uint_t major = index / 4;
    uint_t minor = (index % 4) * 2;
    uchar_t val = 0;
    uchar_t mask = 0x3;
    uint8_t * bitmap = (uint8_t *)(vm->msr_map.arch_data);

    if (hook_reads) {
	val |= 0x1;
    } 
    
    if (hook_writes) {
	val |= 0x2;
    }

    *(bitmap + major) &= ~(mask << minor);
    *(bitmap + major) |= (val << minor);

    return 0;
}


int v3_init_svm_msr_map(struct v3_vm_info * vm) {
    struct v3_msr_map * msr_map = &(vm->msr_map);
  
    msr_map->update_map = update_map;

    msr_map->arch_data = V3_VAddr(V3_AllocPages(2));  
    memset(msr_map->arch_data, 0xff, PAGE_SIZE_4KB * 2);

    v3_refresh_msr_map(vm);

    return 0;
}

int v3_deinit_svm_msr_map(struct v3_vm_info * vm) {
    V3_FreePages(V3_PAddr(vm->msr_map.arch_data), 2);
    return 0;
}
