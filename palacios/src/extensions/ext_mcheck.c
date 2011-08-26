/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National
 * Science Foundation and the Department of Energy.
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org>
 * All rights reserved.
 *
 * Copyright (c) 2008, Philip Soltero <psoltero@cs.unm.edu>
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org>
 * All rights reserved.
 *
 * Author: Philip Soltero <psoltero@cs.unm.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

/**
 * @file Virtualized machine-check architecture.
 *
 * @author <a HREF="mailto:psoltero@cs.unm.edu.us">Philip Soltero</a>
 */

// TODO: CPUID field add failure.
// IF adding CPUID fields fails on the second hook there is no way to back out of the first add.

#include <palacios/vm_guest.h>
#include <palacios/vmm.h>
#include <palacios/vmm_cpuid.h>
#include <palacios/vmm_excp.h>
#include <palacios/vmm_extensions.h>
#include <palacios/vmm_lowlevel.h>
#include <palacios/vmm_msr.h>
#include <palacios/vmm_string.h>

#ifndef V3_CONFIG_DEGUB_EXT_MACH_CHECK
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

#define MCHECK "MCHECK"

#define CPUID_0000_0001 0x00000001
#define CPUID_8000_0001 0x80000001

// Bit 7 - MCE availability.  Bit 14 - MCA availability.
#define CPUID_FIELDS 0x00004080

// 6 error reporting banks. This may be configurable in the future.
#define MC_REG_BANKS 6
#define MCE_INTERRUPT 18

#define MSG_PRE MCHECK": "

#define MCG_CAP  0x0179
#define MCG_STAT 0x017A
#define MCG_CTRL  0x017B



/* I have no idea what Intel was thinking (or maybe they just weren't)
 * but the MCi registers are completely non-standard across Intel's platforms and are a total mess.
 * Any derivative of the pentium-M (i.e. all Core CPU lines) completely disregard the
 * architectural standard that Intel itself created...
 * For these CPUs: the MC4 MSRs switch locations with the MC3s,
 * also every MCi below MC3 (including MC4) does not have a MCi_MISC MSR.
 *
 * So for now, screw it, we'll use AMD's standard
 */

/* AMD MC Banks:
   Bank 0 : Data Cache.
   Bank 1 : Instruction Cache.
   Bank 2 : Bus Unit.
   Bank 3 : Load Store Unit.
   Bank 4 : Northbridge and DRAM.
*/

static const uint32_t amd_mci_bases[] = {0x0400, 0x0404, 0x0408, 0x040c, 0x0410, 0x0414};
static const uint32_t pentium_6_mci_bases[] = {0x0400, 0x0404, 0x0408, 0x040c, 0x0410, 0x0414};
static const uint32_t pentium_m_mci_bases[] = {0x0400, 0x0404, 0x0408, 0x0410, 0x040c, 0x0414};
static const uint32_t ia32_mci_bases[] = { 0x0400, 0x0404, 0x0408, 0x040c,
                       0x0410, 0x0414, 0x0418, 0x041c,
                       0x0420, 0x0424, 0x0428, 0x042c,
                       0x0430, 0x0434, 0x0438, 0x043c,
                       0x0440, 0x0444, 0x0448, 0x044c,
                       0x0450, 0x0454 };
#define MCi_MASK  0xfffffffc
#define MCi_CTRL  0x00
#define MCi_STAT  0x01
#define MCi_ADDR  0x02
#define MCi_MISC  0x03




/**
 * MCA status low and high registers, MC4_STAT, MSR0000_0411.
 */
struct mc4_stat_msr {
     union {
        uint64_t value;
        struct {
        uint_t syndrome            : 8;
        uint_t reserved            : 3;
        uint_t error_code_ext      : 5;
        uint_t error_code          : 16;
        uint_t err_cpu             : 4;
        uint_t ltd_link            : 4;
        uint_t scrub               : 1;
        uint_t sublink             : 1;
        uint_t mca_stat_sub_cache  : 2;
        uint_t reserved_01         : 1;
        uint_t uecc                : 1;
        uint_t cecc                : 1;
        uint_t syndrome2           : 8;
        uint_t reserved_02         : 1;
        uint_t err_cpu_val         : 1;
        uint_t pcc                 : 1;
        uint_t addr_v              : 1;
        uint_t misc_v              : 1;
        uint_t en                  : 1;
        uint_t uc                  : 1;
        uint_t over                : 1;
        uint_t val                 : 1;
        }__attribute__((packed));
     }__attribute__((packed));
} __attribute__((packed));

/**
 * MCA address low and high registers, MC4_ADDR, MSR0000_0412.
 */
struct mc4_addr_msr {
    union {
        uint64_t value;

        struct {
        uint64_t addr32    : 36;
        uint32_t reserved  : 28;
        } __attribute__((packed));

    uint64_t addr64;
    } __attribute__((packed));
} __attribute__((packed));

/**
 * Global machine-check capabilities register, MCG_CAP.
 */
struct mcg_cap_msr {
    union {
        uint64_t value;
        struct {
            uint32_t count            : 8;
            uint32_t mcg_ctl_p        : 1;  // CTRL Present
            uint64_t reserved         : 55;
        } __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

/**
 * Global machine-check status register, MCG_STAT.
 */
struct mcg_stat_msr {
    union {
        uint64_t value;
        struct {
            uint32_t ripv             : 1;
            uint32_t eipv             : 1;
            uint32_t mcip             : 1; // Machine-check in progress.
            uint64_t reserved         : 61;
        } __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

/**
 * Global machine-check control register, MCG_CTRL.
 */
struct mcg_ctl_msr {
    union {
        uint64_t value;
        struct {
            uint32_t dce          : 1; // Data cache register bank enable
            uint32_t ice          : 1; // Instruction cache register bank enable
            uint32_t bue          : 1; // Bus unit register bank enable
            uint32_t lse          : 1; // Load-store register bank enable
            uint32_t nbe          : 1; // Northbridge register bank enable
            uint32_t fre          : 1; // Fixed issue reorder buffer register bank enable
            uint64_t unused       : 58;
        } __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

/**
 * A temporary structure for unimplemented machine-check error reporting banks.
 */
struct mci_bank {
    uint32_t base;
    struct v3_msr ctl;
    struct v3_msr stat;
    struct v3_msr addr;
    struct v3_msr misc;
};

struct mcheck_state {
    struct mcg_cap_msr mcg_cap;
    struct mcg_stat_msr mcg_stat;
    struct mcg_ctl_msr mcg_ctl;

    /* Note that these are in logical order not MSR order */
    /* So MC4 is always at mci_regs[4] even if the MSR is before MC3's */
    struct mci_bank mci_regs[MC_REG_BANKS];
};


static inline
void init_state(struct mcheck_state * const state) {
    int i = 0;

    memset(state, 0, sizeof(struct mcheck_state));

    // Set the initial MCI reg base values to the current architecture
    for (i = 0; i < MC_REG_BANKS; i++) {
    state->mci_regs[i].base = amd_mci_bases[i];
    }
}

/**
 * Handles guest writes to MCG MSRs.
 */
static
int mcg_write_handler(struct guest_info * core, uint32_t msr, struct v3_msr src, void * priv_data) {
    struct mcheck_state * state = (struct mcheck_state *)priv_data;

    switch (msr) {
    case MCG_CAP:
        PrintDebug(MSG_PRE "Ignoring write to MCG_CAP MSR.\n");
        break;

    case MCG_STAT:
        state->mcg_stat.value = 0;
        break;

    case MCG_CTRL:
        if (!state->mcg_cap.mcg_ctl_p) {
        PrintDebug(MSG_PRE "Ignoring write to control MSR '0x%x'. Control MSRs not supported.\n", msr);
        break;
        }

        // The upper 58 bits are unused and read-only.
        state->mcg_ctl.value &= ~0x3f;
        state->mcg_ctl.value |= src.value & 0x3f;

        break;

    default:
         PrintError(MSG_PRE "Reading from invalid MSR: %x\n", msr);
         return -1;
    }

    return 0;
}


/**
 * Handles guest reads to MCG MSRs.
 */
static
int mcg_read_handler(struct guest_info * core, uint32_t msr, struct v3_msr * dst, void * priv_data) {
    struct mcheck_state * state = (struct mcheck_state *)priv_data;

     switch(msr) {
    case MCG_CAP:
        dst->value = state->mcg_cap.value;
        break;

    case MCG_STAT:
        dst->value = state->mcg_stat.value;
        break;

    case MCG_CTRL:
        if (!state->mcg_cap.mcg_ctl_p) {
        PrintDebug(MSG_PRE "Ignoring read of control MSR '0x%x'. Control MSRs not supported.\n", msr);
        break;
        }

        dst->value = state->mcg_ctl.value;
        break;

     default:
         PrintError(MSG_PRE "Reading from invalid MSR: %x\n", msr);
         return -1;
     }

     return 0;
}

static struct mci_bank * get_mci_reg(struct mcheck_state * state, uint32_t msr) {
    int i = 0;

    for (i = 0; i < MC_REG_BANKS; i++) {
    if (state->mci_regs[i].base == (msr & MCi_MASK)) {
        return &(state->mci_regs[i]);
    }
    }

    return NULL;
}


/**
 * Handles guest reads to MCi MSRs.
 */
static
int mci_read_handler(struct guest_info * const core,
                     const uint32_t msr,
                     struct v3_msr * const dst,
                     void * const priv_data) {
    struct mcheck_state * const state = (struct mcheck_state *)priv_data;
    struct mci_bank * mci = get_mci_reg(state, msr);

    PrintDebug(MSG_PRE "Reading value '0x%llx' for MSR '0x%x'.\n", dst->value, msr);

    if (mci == NULL) {
    PrintError(MSG_PRE " MSR read for invalid MCI register 0x%x\n", msr);
    return -1;
    }

    switch (msr & ~MCi_MASK) {
    case MCi_CTRL:
        if (!state->mcg_cap.mcg_ctl_p) {
        PrintDebug(MSG_PRE "Ignoring read of control MSR '0x%x'. Control MSRs not supported.\n", msr);
        break;
        }

        dst->value = mci->ctl.value;
        break;

    case MCi_STAT:
        dst->value = mci->stat.value;
        break;

    case MCi_ADDR:
        dst->value = mci->addr.value;
        break;

    case MCi_MISC:
        dst->value = mci->misc.value;
        break;

    default:
        PrintError(MSG_PRE "Ignoring read of unhooked MSR '0x%x'. This is a bug.\n", msr);
        break;
    }

    return 0;
}

/**
 * Handles guest writes to MCi MSRs.
 */
static
int mci_write_handler(struct guest_info * const core,
              const uint_t msr,
              const struct v3_msr src,
              void * const priv_data) {
    struct mcheck_state * const state = (struct mcheck_state *)priv_data;
    struct mci_bank * mci = get_mci_reg(state, msr);

    PrintDebug(MSG_PRE "Writing value '0x%llx' for MSR '0x%x'.\n", src.value, msr);

    switch (msr & ~MCi_MASK) {
    case MCi_CTRL:
        if (!state->mcg_cap.mcg_ctl_p) {
        PrintDebug(MSG_PRE "Ignoring read of control MSR '0x%x'. Control MSRs not supported.\n", msr);
        break;
        }

        mci->ctl.value = src.value;
        break;

    case MCi_STAT:
        if (src.value != 0) {
        // Should be a GPF.
        PrintError(MSG_PRE "Ignoring write of illegal value '0x%llx'.\n", src.value);
        return -1;
        }

        mci->stat.value = 0;
        break;

    case MCi_ADDR:
        mci->addr.value = src.value;
        break;

    case MCi_MISC:
        V3_Print(MSG_PRE "Ignoring write to read only miscellaneous MSR '0x%x'.\n", msr);
        break;

    default:
        PrintError(MSG_PRE "Ignoring write of unhooked MSR '0x%x'. This is a bug.\n", msr);
        break;
    }

    return 0;
}

/**
 * Add machine-check-related CPUID fields for CPUID functions 0000_0001 and 8000_0001.
 *
 * @return 0 for success and -1 for failure.
 */
static inline
int add_cpuid_fields(struct v3_vm_info * const vm,
        struct mcheck_state * const state) {
    int ret = 0;

    ret = v3_cpuid_add_fields(vm, CPUID_0000_0001,
                              0, 0,
                              0, 0,
                              0, 0,
                              CPUID_FIELDS, CPUID_FIELDS);

    if (ret == -1) {
    PrintError(MSG_PRE "Failed to add CPUID fields for function 0000_0001.\n");
    return -1;
    }

    // Add bit 7, MCE availability, and bit 14, MCE availability.
    ret = v3_cpuid_add_fields(vm, CPUID_8000_0001,
                              0, 0,
                              0, 0,
                              0, 0,
                              CPUID_FIELDS, CPUID_FIELDS);

    if (ret == -1) {
    PrintError(MSG_PRE "Failed to add CPUID fileds for function 8000_0001.\n");
    return -1;
    }

    return 0;
}


static int deinit_mcheck(struct v3_vm_info * vm, void * priv_data) {
    struct mcheck_state * state = (struct mcheck_state *)v3_get_extension_state(vm, priv_data);
    if (state == NULL) {
        PrintError(MSG_PRE "Failed to get machine-check architecture extension state.\n");
        return -1;
    }

    V3_Free(state);
    return 0;
}

static int init_mcheck(struct v3_vm_info * vm, v3_cfg_tree_t * cfg, void ** priv_data) {
    struct mcheck_state * state = NULL;
    int ret = 0;
    int i = 0;

    state = (struct mcheck_state *)V3_Malloc(sizeof(struct mcheck_state));

    if (state == NULL) {
    PrintError(MSG_PRE "Failed to allocate machine-check architecture state.\n");
    return -1;
    }


    init_state(state);

    state->mcg_cap.count = MC_REG_BANKS;
    state->mcg_cap.mcg_ctl_p = 1;

    ret |= add_cpuid_fields(vm, state);

    /* Hook the MSRs */
    ret |= v3_hook_msr(vm, MCG_CAP, mcg_read_handler, mcg_write_handler, state);
    ret |= v3_hook_msr(vm, MCG_STAT, mcg_read_handler, mcg_write_handler, state);
    ret |= v3_hook_msr(vm, MCG_CTRL, mcg_read_handler, mcg_write_handler, state);

    for (i = 0; i < MC_REG_BANKS; i++) {
    ret |= v3_hook_msr(vm, state->mci_regs[i].base, mci_read_handler, mci_write_handler, state);
    ret |= v3_hook_msr(vm, state->mci_regs[i].base + 1, mci_read_handler, mci_write_handler, state);
    ret |= v3_hook_msr(vm, state->mci_regs[i].base + 2, mci_read_handler, mci_write_handler, state);
    ret |= v3_hook_msr(vm, state->mci_regs[i].base + 3, mci_read_handler, mci_write_handler, state);
    }

    if (ret == -1) {
    PrintError(MSG_PRE "Error hooking machine-check architecture resources.\n");
    V3_Free(state);
    return -1;
    }

    *priv_data = state;

    PrintDebug(MSG_PRE "Initialized machine-check architecture.\n");

    return 0;
}

int v3_mcheck_inject_nb_mce(struct v3_vm_info * const vm, const uint32_t cpu,
                const struct mc4_stat_msr stat,
                const struct mc4_addr_msr addr) {
    struct mcheck_state * state = (struct mcheck_state *)v3_get_extension_state(vm, MCHECK);
    int ret;

    if (state == NULL) {
    PrintError(MSG_PRE "Machine-check architecture extension state not found.\n");
    return -1;
    }

    // For now only MCE injection on cpu 0 is supported.
    if (cpu != 0) {
    PrintError(MSG_PRE "Injecting MCE on cpu %u not supported.\n", cpu);
    return -1;
    }

    // Is the Northbridge bank enabled?
    if (state->mcg_ctl.nbe != 1) {
    PrintDebug(MSG_PRE "Northbridge register bank disabled. Ignoring Northbridge MCE.\n");
    return 0;
    }

    state->mci_regs[4].stat.value = stat.value;
    state->mci_regs[4].addr.value = addr.value;

    state->mcg_stat.value = 0;
    state->mcg_stat.ripv = 1;
    state->mcg_stat.mcip = 1;

    PrintDebug(MSG_PRE "Injecting NB MCE on core %u.\n", 0);

    // Raise on core 0.
    ret = v3_raise_exception(&(vm->cores[0]), MCE_INTERRUPT);

    if (ret == -1) {
    PrintError(MSG_PRE "Failed to raise MCE.\n");
    return -1;
    }

    return 0;
}

int v3_mcheck_inject_scrubber_mce(struct v3_vm_info *info, int cpu, uint64_t dst)
{
    struct mc4_stat_msr stat;
    struct mc4_addr_msr addr;

    stat.value = 0;
    stat.error_code = 0x810;
    stat.error_code_ext = 0x8;
    stat.uecc = 1;
    stat.addr_v = 1;
    stat.en = 1;
    stat.uc = 1;
    stat.val = 1;

    addr.addr64 = dst;

    return v3_mcheck_inject_nb_mce(info, cpu, stat, addr);
}

static struct v3_extension_impl mcheck_impl = {
    .name = MCHECK,
    .init = init_mcheck,
    .deinit = deinit_mcheck,
    .core_init = NULL,
    .core_deinit = NULL,
    .on_entry = NULL,
    .on_exit = NULL
};

register_extension(&mcheck_impl);
