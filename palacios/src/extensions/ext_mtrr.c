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

#include <palacios/vmm.h>
#include <palacios/vmm_extensions.h>
#include <palacios/vmm_msr.h>



#define MTRR_CAP          0xfe

#define MTRR_PHYS_BASE_0  0x200
#define MTRR_PHYS_MASK_0  0x201
#define MTRR_PHYS_BASE_1  0x202
#define MTRR_PHYS_MASK_1  0x203
#define MTRR_PHYS_BASE_2  0x204
#define MTRR_PHYS_MASK_2  0x205
#define MTRR_PHYS_BASE_3  0x206
#define MTRR_PHYS_MASK_3  0x207
#define MTRR_PHYS_BASE_4  0x208
#define MTRR_PHYS_MASK_4  0x209
#define MTRR_PHYS_BASE_5  0x20a
#define MTRR_PHYS_MASK_5  0x20b
#define MTRR_PHYS_BASE_6  0x20c
#define MTRR_PHYS_MASK_6  0x20d
#define MTRR_PHYS_BASE_7  0x20e
#define MTRR_PHYS_MASK_7  0x20f

#define MTRR_FIX_64K_00000 0x250
#define MTRR_FIX_16K_80000 0x258
#define MTRR_FIX_16K_A0000 0x259
#define MTRR_FIX_4K_C0000  0x268
#define MTRR_FIX_4K_C8000  0x269
#define MTRR_FIX_4K_D0000  0x26a
#define MTRR_FIX_4K_D8000  0x26b
#define MTRR_FIX_4K_E0000  0x26c
#define MTRR_FIX_4K_E8000  0x26d
#define MTRR_FIX_4K_F0000  0x26e
#define MTRR_FIX_4K_F8000  0x26f

#define PAT                0x277

#define MTRR_DEF_TYPE      0x2ff




struct ia32_pat {
    union {
	uint64_t value;

	struct {
	    uint64_t pa_0            : 3;
	    uint64_t rsvd0           : 5;
	    uint64_t pa_1            : 3;
	    uint64_t rsvd1           : 5;
	    uint64_t pa_2            : 3;
	    uint64_t rsvd2           : 5;
	    uint64_t pa_3            : 3;
	    uint64_t rsvd3           : 5;
	    uint64_t pa_4            : 3;
	    uint64_t rsvd4           : 5;
	    uint64_t pa_5            : 3;
	    uint64_t rsvd5           : 5;
	    uint64_t pa_6            : 3;
	    uint64_t rsvd6           : 5;
	    uint64_t pa_7            : 3;
	    uint64_t rsvd7           : 5;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


struct mtrr_cap {
    union {
	uint64_t value;

	struct {
	    uint64_t var_reg_cnt     : 8;
	    uint64_t fix             : 1;
	    uint64_t rsvd0           : 1;
	    uint64_t wr_combine      : 1;
	    uint64_t rsvd1           : 53;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

struct mtrr_def_type {
    union {
	uint64_t value;

	struct {
	    uint64_t def_type        : 8;
	    uint64_t rsvd0           : 2;
	    uint64_t fixed_enable    : 1;
	    uint64_t mtrr_emable     : 1;
	    uint64_t rsvd1           : 52;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


struct mtrr_phys_base {
    union {
	uint64_t value;
	
	struct {
	    uint64_t type            : 8;
	    uint64_t rsvd0           : 4;
	    uint64_t base            : 40;
	    uint64_t rsvd1           : 12;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

    
struct mtrr_phys_mask {
    union {
	uint64_t value;
	
	struct {
	    uint64_t rsvd0           : 11;
	    uint64_t valid           : 1;
	    uint64_t mask            : 40;
	    uint64_t rsvd1           : 12;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

struct mtrr_fixed {
    union {
	uint64_t value;
	uint8_t types[8];
    } __attribute__((packed));
} __attribute__((packed));



/* AMD Specific Registers */
#define SYSCONFIG          0xc0010010
#define TOP_MEM            0xc001001a
#define TOP_MEM2           0xc001001d

#define IORR_BASE0         0xc0010016
#define IORR_MASK0         0xc0010017
#define IORR_BASE1         0xc0010018
#define IORR_MASK1         0xc0010019

struct syscfg_reg {
    union {
	uint64_t value;

	struct {
	    uint64_t rsvd0          : 18;
	    uint64_t mfde           : 1; // 1 = enables RdMem and WrMem bits in fixed-range MTRRs
	    uint64_t mfdm           : 1; // 1 = software can modify RdMem and WrMem bits
	    uint64_t mvdm           : 1; // 1 = enables TOP_MEM reg and var range MTRRs
	    uint64_t tom2           : 1; // 1 = enables TOP_MEM2 reg
	    uint64_t tom2_force_wb  : 1; // 1 = enables default mem type for 4GB-TOP_MEM2 range
	    uint64_t rsvd1          : 41;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

struct top_of_mem_reg {
    union {
	uint64_t value;
	
	struct {
	    uint64_t rsvd0          : 23;
	    uint64_t phys_addr      : 29;
	    uint64_t rsvd1          : 12;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


struct iorr_base {
    union {
	uint64_t value;
	
	struct {
	    uint64_t rsvd0          : 3;
	    uint64_t wrmem          : 1; // 1 = writes go to memory, 0 = writes go to mmap IO
	    uint64_t rdmem          : 1; // 1 = reads go to memory, 0 = reads go to mmap IO
	    uint64_t rsvd1          : 7;
	    uint64_t base           : 40;
	    uint64_t rsvd2          : 12;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

struct iorr_mask {
    union {
	uint64_t value;

	struct {
	    uint64_t rsvd0          : 11;
	    uint64_t valid          : 1;
	    uint64_t mask           : 40;
	    uint64_t rsvd1          : 12;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

/* Intel Specific Registers */
#define SMRR_PHYS_BASE 0x1f2
#define SMRR_PHYS_MASK 0x1f3

struct smrr_phys_base {
    union {
	uint64_t value;

	struct {
	    uint64_t type            : 8;
	    uint64_t rsvd0           : 4;
	    uint64_t base            : 20;
	    uint64_t rsvd1           : 32;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

struct smrr_phys_mask {
    union {
	uint64_t value;

	struct {
	    uint64_t rsvd0           : 11;
	    uint64_t valid           : 1;
	    uint64_t mask            : 20;
	    uint64_t rsvd1           : 32;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));



struct mtrr_state {
    struct ia32_pat pat;
    struct mtrr_cap cap;
    struct mtrr_def_type def_type;
    struct mtrr_phys_base bases[8];
    struct mtrr_phys_mask masks[8];

    struct mtrr_fixed fixed_64k;
    struct mtrr_fixed fixed_16k[2];
    struct mtrr_fixed fixed_4k[8];

    /* AMD specific registers */
    struct syscfg_reg amd_syscfg;
    struct top_of_mem_reg amd_tom;
    struct top_of_mem_reg amd_tom2;

    struct iorr_base iorr_bases[2];
    struct iorr_mask iorr_masks[2];

    /* Intel Specific registers */
    struct smrr_phys_base intel_smrr_base;
    struct smrr_phys_mask intel_smrr_mask;

};

static void init_state(struct mtrr_state * state) {
    state->pat.value = 0x0007040600070406LL;
    state->cap.value = 0x0000000000000508LL;

    state->amd_syscfg.value = 0x0000000000020601LL;
    state->amd_tom.value = 0x0000000004000000LL;

    return;
}

static int mtrr_cap_read(struct guest_info * core, uint_t msr, v3_msr_t * dst, void * priv_data) {
    struct mtrr_state * state = (struct mtrr_state *)priv_data;
    dst->value = state->cap.value;
    return 0;
}

static int mtrr_cap_write(struct guest_info * core, uint_t msr, v3_msr_t src, void * priv_data) {
    struct mtrr_state * state = (struct mtrr_state *)priv_data;
    state->cap.value = src.value;
    return 0;
}

static int pat_read(struct guest_info * core, uint_t msr, v3_msr_t * dst, void * priv_data) {
    struct mtrr_state * state = (struct mtrr_state *)priv_data;
    dst->value = state->pat.value;
    return 0;
}

static int pat_write(struct guest_info * core, uint_t msr, v3_msr_t src, void * priv_data) {
    struct mtrr_state * state = (struct mtrr_state *)priv_data;
    state->pat.value = src.value;
    return 0;
}

static int def_type_read(struct guest_info * core, uint_t msr, v3_msr_t * dst, void * priv_data) {
    struct mtrr_state * state = (struct mtrr_state *)priv_data;
    dst->value = state->def_type.value;
    return 0;
}

static int def_type_write(struct guest_info * core, uint_t msr, v3_msr_t src, void * priv_data) {
    struct mtrr_state * state = (struct mtrr_state *)priv_data;
    state->def_type.value = src.value;
    return 0;
}


static int mtrr_phys_base_read(struct guest_info * core, uint_t msr, v3_msr_t * dst, void * priv_data) {
    struct mtrr_state * state = (struct mtrr_state *)priv_data;
    int base_index = (msr - MTRR_PHYS_BASE_0) / 2;
    dst->value = state->bases[base_index].value;
    return 0;
}

static int mtrr_phys_base_write(struct guest_info * core, uint_t msr, v3_msr_t src, void * priv_data) {
    struct mtrr_state * state = (struct mtrr_state *)priv_data;
    int base_index = (msr - MTRR_PHYS_BASE_0) / 2;
    state->bases[base_index].value = src.value;
    return 0;
}

static int mtrr_phys_mask_read(struct guest_info * core, uint_t msr, v3_msr_t * dst, void * priv_data) {
    struct mtrr_state * state = (struct mtrr_state *)priv_data;
    int mask_index = (msr - MTRR_PHYS_MASK_0) / 2;
    dst->value = state->masks[mask_index].value;
    return 0;
}

static int mtrr_phys_mask_write(struct guest_info * core, uint_t msr, v3_msr_t src, void * priv_data) {
    struct mtrr_state * state = (struct mtrr_state *)priv_data;
    int mask_index = (msr - MTRR_PHYS_MASK_0) / 2;
    state->masks[mask_index].value = src.value;
    return 0;
}

static int mtrr_fix_64k_read(struct guest_info * core, uint_t msr, v3_msr_t * dst, void * priv_data) {
    struct mtrr_state * state = (struct mtrr_state *)priv_data;
    dst->value = state->fixed_64k.value;
    return 0;
}

static int mtrr_fix_64k_write(struct guest_info * core, uint_t msr, v3_msr_t src, void * priv_data) {
    struct mtrr_state * state = (struct mtrr_state *)priv_data;
    state->fixed_64k.value = src.value;
    return 0;
}

static int mtrr_fix_16k_read(struct guest_info * core, uint_t msr, v3_msr_t * dst, void * priv_data) {
    struct mtrr_state * state = (struct mtrr_state *)priv_data;
    int index = msr - MTRR_FIX_16K_80000;
    dst->value = state->fixed_16k[index].value;
    return 0;
}

static int mtrr_fix_16k_write(struct guest_info * core, uint_t msr, v3_msr_t src, void * priv_data) {
    struct mtrr_state * state = (struct mtrr_state *)priv_data;
    int index = msr - MTRR_FIX_16K_80000;
    state->fixed_16k[index].value = src.value;
    return 0;
}

static int mtrr_fix_4k_read(struct guest_info * core, uint_t msr, v3_msr_t * dst, void * priv_data) {
    struct mtrr_state * state = (struct mtrr_state *)priv_data;
    int index = msr - MTRR_FIX_4K_C0000;
    dst->value = state->fixed_4k[index].value;
    return 0;
}

static int mtrr_fix_4k_write(struct guest_info * core, uint_t msr, v3_msr_t src, void * priv_data) {
    struct mtrr_state * state = (struct mtrr_state *)priv_data;
    int index = msr - MTRR_FIX_4K_C0000;
    state->fixed_4k[index].value = src.value;
    return 0;
}

/* AMD specific registers */
static int amd_syscfg_read(struct guest_info * core, uint_t msr, v3_msr_t * dst, void * priv_data) {
    struct mtrr_state * state = (struct mtrr_state *)priv_data;
    dst->value = state->amd_syscfg.value;
    return 0;
}

static int amd_syscfg_write(struct guest_info * core, uint_t msr, v3_msr_t src, void * priv_data) {
    struct mtrr_state * state = (struct mtrr_state *)priv_data;
    state->amd_syscfg.value = src.value;
    return 0;
}

static int amd_top_mem_read(struct guest_info * core, uint_t msr, v3_msr_t * dst, void * priv_data) {
    struct mtrr_state * state = (struct mtrr_state *)priv_data;

    if (msr == TOP_MEM) {
	dst->value = state->amd_tom.value;
    } else if (msr == TOP_MEM2) {
	dst->value = state->amd_tom2.value;
    }

    return 0;
}

static int amd_top_mem_write(struct guest_info * core, uint_t msr, v3_msr_t src, void * priv_data) {
    struct mtrr_state * state = (struct mtrr_state *)priv_data;
    
    if (msr == TOP_MEM) {
	state->amd_tom.value = src.value;
    } else if (msr == TOP_MEM2) {
	state->amd_tom2.value = src.value;
    }

    return 0;
}


static int amd_iorr_base_read(struct guest_info * core, uint_t msr, v3_msr_t * dst, void * priv_data) {
    struct mtrr_state * state = (struct mtrr_state *)priv_data;
    int base_index = (msr - IORR_BASE0) / 2;
    dst->value = state->iorr_bases[base_index].value;
    return 0;
}

static int amd_iorr_base_write(struct guest_info * core, uint_t msr, v3_msr_t src, void * priv_data) {
    struct mtrr_state * state = (struct mtrr_state *)priv_data;
    int base_index = (msr - IORR_BASE0) / 2;
    state->iorr_bases[base_index].value = src.value;
    return 0;
}

static int amd_iorr_mask_read(struct guest_info * core, uint_t msr, v3_msr_t * dst, void * priv_data) {
    struct mtrr_state * state = (struct mtrr_state *)priv_data;
    int mask_index = (msr - IORR_MASK0) / 2;
    dst->value = state->iorr_masks[mask_index].value;
    return 0;
}

static int amd_iorr_mask_write(struct guest_info * core, uint_t msr, v3_msr_t src, void * priv_data) {
    struct mtrr_state * state = (struct mtrr_state *)priv_data;
    int mask_index = (msr - IORR_MASK0) / 2;
    state->iorr_masks[mask_index].value = src.value;
    return 0;
}


static int intel_smrr_base_read(struct guest_info * core, uint_t msr, v3_msr_t * dst, void * priv_data) {
    struct mtrr_state * state = (struct mtrr_state *)priv_data;
    dst->value = state->intel_smrr_base.value;
    return 0;
}

static int intel_smrr_base_write(struct guest_info * core, uint_t msr, v3_msr_t src, void * priv_data) {
    struct mtrr_state * state = (struct mtrr_state *)priv_data;
    state->intel_smrr_base.value = src.value;
    return 0;
}

static int intel_smrr_mask_read(struct guest_info * core, uint_t msr, v3_msr_t * dst, void * priv_data) {
    struct mtrr_state * state = (struct mtrr_state *)priv_data;
    dst->value = state->intel_smrr_mask.value;
    return 0;
}

static int intel_smrr_mask_write(struct guest_info * core, uint_t msr, v3_msr_t src, void * priv_data) {
    struct mtrr_state * state = (struct mtrr_state *)priv_data;
    state->intel_smrr_mask.value = src.value;
    return 0;
}


static int deinit_mtrrs(struct v3_vm_info * vm, void * priv_data) {
    struct mtrr_state * state = (struct mtrr_state *)priv_data;
    
    v3_unhook_msr(vm, MTRR_CAP);
    v3_unhook_msr(vm, PAT);
    v3_unhook_msr(vm, MTRR_DEF_TYPE);

    v3_unhook_msr(vm, MTRR_PHYS_BASE_0);
    v3_unhook_msr(vm, MTRR_PHYS_BASE_1);
    v3_unhook_msr(vm, MTRR_PHYS_BASE_2);
    v3_unhook_msr(vm, MTRR_PHYS_BASE_3);
    v3_unhook_msr(vm, MTRR_PHYS_BASE_4);
    v3_unhook_msr(vm, MTRR_PHYS_BASE_5);
    v3_unhook_msr(vm, MTRR_PHYS_BASE_6);
    v3_unhook_msr(vm, MTRR_PHYS_BASE_7);
    v3_unhook_msr(vm, MTRR_PHYS_MASK_0);
    v3_unhook_msr(vm, MTRR_PHYS_MASK_1);
    v3_unhook_msr(vm, MTRR_PHYS_MASK_2);
    v3_unhook_msr(vm, MTRR_PHYS_MASK_3);
    v3_unhook_msr(vm, MTRR_PHYS_MASK_4);
    v3_unhook_msr(vm, MTRR_PHYS_MASK_5);
    v3_unhook_msr(vm, MTRR_PHYS_MASK_6);
    v3_unhook_msr(vm, MTRR_PHYS_MASK_7);

    v3_unhook_msr(vm, MTRR_FIX_64K_00000);
    v3_unhook_msr(vm, MTRR_FIX_16K_80000);
    v3_unhook_msr(vm, MTRR_FIX_16K_A0000);
    v3_unhook_msr(vm, MTRR_FIX_4K_C0000);
    v3_unhook_msr(vm, MTRR_FIX_4K_C8000);
    v3_unhook_msr(vm, MTRR_FIX_4K_D0000);
    v3_unhook_msr(vm, MTRR_FIX_4K_D8000);
    v3_unhook_msr(vm, MTRR_FIX_4K_E0000);
    v3_unhook_msr(vm, MTRR_FIX_4K_E8000);
    v3_unhook_msr(vm, MTRR_FIX_4K_F0000);
    v3_unhook_msr(vm, MTRR_FIX_4K_F8000);

    /* AMD specific */
    v3_unhook_msr(vm, SYSCONFIG);
    v3_unhook_msr(vm, TOP_MEM);
    v3_unhook_msr(vm, TOP_MEM2);

    v3_unhook_msr(vm, IORR_BASE0);
    v3_unhook_msr(vm, IORR_BASE1);
    v3_unhook_msr(vm, IORR_MASK0);
    v3_unhook_msr(vm, IORR_MASK1);
	    
    /* Intel Specfic */
    v3_unhook_msr(vm, SMRR_PHYS_BASE);
    v3_unhook_msr(vm, SMRR_PHYS_MASK);


    V3_Free(state);
    return 0;
}


static int init_mtrrs(struct v3_vm_info * vm, v3_cfg_tree_t * cfg, void ** priv_data) {
    struct mtrr_state * state = NULL;
    int ret = 0;

    state = V3_Malloc(sizeof(struct mtrr_state));
    memset(state, 0, sizeof(struct mtrr_state));

    *priv_data = state;

    init_state(state);
    
    // hook MSRs
    ret |= v3_hook_msr(vm, MTRR_CAP, mtrr_cap_read, mtrr_cap_write, state);
    ret |= v3_hook_msr(vm, PAT, pat_read, pat_write, state);
    ret |= v3_hook_msr(vm, MTRR_DEF_TYPE, def_type_read, def_type_write, state);

    ret |= v3_hook_msr(vm, MTRR_PHYS_BASE_0, mtrr_phys_base_read, mtrr_phys_base_write, state);
    ret |= v3_hook_msr(vm, MTRR_PHYS_BASE_1, mtrr_phys_base_read, mtrr_phys_base_write, state);
    ret |= v3_hook_msr(vm, MTRR_PHYS_BASE_2, mtrr_phys_base_read, mtrr_phys_base_write, state);
    ret |= v3_hook_msr(vm, MTRR_PHYS_BASE_3, mtrr_phys_base_read, mtrr_phys_base_write, state);
    ret |= v3_hook_msr(vm, MTRR_PHYS_BASE_4, mtrr_phys_base_read, mtrr_phys_base_write, state);
    ret |= v3_hook_msr(vm, MTRR_PHYS_BASE_5, mtrr_phys_base_read, mtrr_phys_base_write, state);
    ret |= v3_hook_msr(vm, MTRR_PHYS_BASE_6, mtrr_phys_base_read, mtrr_phys_base_write, state);
    ret |= v3_hook_msr(vm, MTRR_PHYS_BASE_7, mtrr_phys_base_read, mtrr_phys_base_write, state);
    ret |= v3_hook_msr(vm, MTRR_PHYS_MASK_0, mtrr_phys_mask_read, mtrr_phys_mask_write, state);
    ret |= v3_hook_msr(vm, MTRR_PHYS_MASK_1, mtrr_phys_mask_read, mtrr_phys_mask_write, state);
    ret |= v3_hook_msr(vm, MTRR_PHYS_MASK_2, mtrr_phys_mask_read, mtrr_phys_mask_write, state);
    ret |= v3_hook_msr(vm, MTRR_PHYS_MASK_3, mtrr_phys_mask_read, mtrr_phys_mask_write, state);
    ret |= v3_hook_msr(vm, MTRR_PHYS_MASK_4, mtrr_phys_mask_read, mtrr_phys_mask_write, state);
    ret |= v3_hook_msr(vm, MTRR_PHYS_MASK_5, mtrr_phys_mask_read, mtrr_phys_mask_write, state);
    ret |= v3_hook_msr(vm, MTRR_PHYS_MASK_6, mtrr_phys_mask_read, mtrr_phys_mask_write, state);
    ret |= v3_hook_msr(vm, MTRR_PHYS_MASK_7, mtrr_phys_mask_read, mtrr_phys_mask_write, state);

    ret |= v3_hook_msr(vm, MTRR_FIX_64K_00000, mtrr_fix_64k_read, mtrr_fix_64k_write, state);
    ret |= v3_hook_msr(vm, MTRR_FIX_16K_80000, mtrr_fix_16k_read, mtrr_fix_16k_write, state);
    ret |= v3_hook_msr(vm, MTRR_FIX_16K_A0000, mtrr_fix_16k_read, mtrr_fix_16k_write, state);
    ret |= v3_hook_msr(vm, MTRR_FIX_4K_C0000, mtrr_fix_4k_read, mtrr_fix_4k_write, state);
    ret |= v3_hook_msr(vm, MTRR_FIX_4K_C8000, mtrr_fix_4k_read, mtrr_fix_4k_write, state);
    ret |= v3_hook_msr(vm, MTRR_FIX_4K_D0000, mtrr_fix_4k_read, mtrr_fix_4k_write, state);
    ret |= v3_hook_msr(vm, MTRR_FIX_4K_D8000, mtrr_fix_4k_read, mtrr_fix_4k_write, state);
    ret |= v3_hook_msr(vm, MTRR_FIX_4K_E0000, mtrr_fix_4k_read, mtrr_fix_4k_write, state);
    ret |= v3_hook_msr(vm, MTRR_FIX_4K_E8000, mtrr_fix_4k_read, mtrr_fix_4k_write, state);
    ret |= v3_hook_msr(vm, MTRR_FIX_4K_F0000, mtrr_fix_4k_read, mtrr_fix_4k_write, state);
    ret |= v3_hook_msr(vm, MTRR_FIX_4K_F8000, mtrr_fix_4k_read, mtrr_fix_4k_write, state);
    
    /* AMD Specific */
    ret |= v3_hook_msr(vm, SYSCONFIG, amd_syscfg_read, amd_syscfg_write, state);
    ret |= v3_hook_msr(vm, TOP_MEM, amd_top_mem_read, amd_top_mem_write, state);
    ret |= v3_hook_msr(vm, TOP_MEM2, amd_top_mem_read, amd_top_mem_write, state);

    ret |= v3_hook_msr(vm, IORR_BASE0, amd_iorr_base_read, amd_iorr_base_write, state);
    ret |= v3_hook_msr(vm, IORR_BASE1, amd_iorr_base_read, amd_iorr_base_write, state);
    ret |= v3_hook_msr(vm, IORR_MASK0, amd_iorr_mask_read, amd_iorr_mask_write, state);
    ret |= v3_hook_msr(vm, IORR_MASK1, amd_iorr_mask_read, amd_iorr_mask_write, state);
    

    /* INTEL specific */
    ret |= v3_hook_msr(vm, SMRR_PHYS_BASE, intel_smrr_base_read, intel_smrr_base_write, state);
    ret |= v3_hook_msr(vm, SMRR_PHYS_MASK, intel_smrr_mask_read, intel_smrr_mask_write, state);

    if (ret != 0) {
	PrintError("Failed to hook all MTRR MSRs. Aborting...\n");
	deinit_mtrrs(vm, state);
	return -1;
    }


    return 0;
}




static struct v3_extension_impl mtrr_impl = {
    .name = "MTRRS",
    .init = init_mtrrs,
    .deinit = deinit_mtrrs,
    .core_init = NULL,
    .core_deinit = NULL,
    .on_entry = NULL,
    .on_exit = NULL
};

register_extension(&mtrr_impl);
