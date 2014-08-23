/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2014, the V3VEE Project <http://www.v3vee.org>
 * all rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *         Shiva Rao <shiva.rao.717@gmail.com>
 *         Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  you are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/export.h>
#include <linux/cpufreq.h>
#include <linux/kernel.h>
#include <linux/kmod.h>
#include <linux/string.h>
#include <asm/processor.h>
#include <asm/msr.h>
#include <asm/msr-index.h>

#include <interfaces/vmm_pstate_ctrl.h>

#include "palacios.h"
#include "iface-pstate-ctrl.h"

#include "linux-exts.h"

/*
   This P-STATE control implementation includes:

   - Direct control of Intel and AMD processor pstates
   - External control of processor states via Linux (unimplemented)
   - Internal control of processor states in Palacios (handoff from Linux)

   Additionally, it provides a user-space interface for manipulating
   p-state regardless of the host's functionality.  This includes
   an ioctl for commanding the implementation and a /proc file for 
   showing current status and capabilities.

*/


#define PALACIOS_GOVNAME "v3vee"
#define MAX_PATH_LEN     128
#define MAX_GOV_NAME_LEN 16


struct pstate_core_info {
    // Here we have the notion of host control
#define V3_PSTATE_HOST_CONTROL 0
    // and all the modes from the Palacios interface:
    // V3_PSTATE_EXTERNAL_CONTROL
    // V3_PSTATE_DIRECT_CONTROL
    // V3_PSTATE_INTERNAL_CONTROL
    uint32_t mode;

    // Apply if we are under the DIRECT state
    uint8_t cur_pstate;
    uint8_t max_pstate;
    uint8_t min_pstate;

    uint8_t cur_hw_pstate;

    // Apply if we are under the EXTERNAL state
    uint64_t cur_freq_khz;
    uint64_t max_freq_khz;
    uint64_t min_freq_khz;

    // Intel-specific
    uint8_t prior_speedstep;
    uint8_t turbo_disabled;
    uint8_t no_turbo;

    int have_cpufreq;

    // This is where we stash Linux's governor when we make a mode switch
    char * linux_governor;
    // We have this so we can restore the original frequency when we started
    uint64_t original_hz; 

};


static DEFINE_PER_CPU(struct pstate_core_info, core_state);



// These are used to assert DIRECT control over the core pstates
struct pstate_core_funcs {
    void    (*arch_init)(void);
    void    (*arch_deinit)(void);
    uint8_t (*get_min_pstate)(void);
    uint8_t (*get_max_pstate)(void);
    uint8_t (*get_pstate)(void);
    void    (*set_pstate)(uint8_t pstate);
};

struct pstate_machine_info {
    enum {INTEL, AMD, OTHER } arch;
    int supports_pstates;


    // For AMD
    int have_pstate;
    int have_coreboost;
    int have_feedback;  

    // For Intel
    int have_speedstep;
    int have_opportunistic; // this means "Turbo Boost" or "IDA"
    int have_policy_hint;
    int have_hwp;       // hardware-controlled performance states
    int have_hdc;       // hardware duty cycling
    int have_mwait_ext; // mwait power extensions
    int have_mwait_int; // mwait wakes on interrupt

    // for both
    int have_pstate_hw_coord;  // mperf/aperf

    // used for DIRECT control
    struct pstate_core_funcs *funcs;

};

static struct pstate_machine_info machine_state;


/****************************************************
  AMD  DIRECT CONTROL
 ***************************************************/

/* AMD Programmer's Manual Vol 2 (Rev 3, 2013), Sec. 17.1, pp.557 */
#define MSR_PSTATE_LIMIT_REG_AMD 0xc0010061
#define MSR_PSTATE_CTL_REG_AMD   0xc0010062
#define MSR_PSTATE_STAT_REG_AMD  0xc0010063

struct p_state_limit_reg_amd {
    union {
        uint64_t val;
        struct {
            uint8_t  pstate_limit : 4; /* lowest P-state value (highest perf.) supported currently (this can change at runtime) */
            uint8_t  pstate_max   : 4; /* highest P-state value supported  (lowest perf) */
            uint64_t rsvd         : 56;
        } reg;
    } __attribute__((packed));
} __attribute__((packed));


struct p_state_stat_reg_amd {
    union {
        uint64_t val;
        struct {
            uint8_t  pstate  : 4;
            uint64_t rsvd    : 60;
        } reg;
    } __attribute__((packed));
} __attribute__((packed));


struct p_state_ctl_reg_amd {
    union {
        uint64_t val;
        struct {
            uint8_t  cmd  : 4;
            uint64_t rsvd : 60;
        } reg;
    } __attribute__((packed));
} __attribute__((packed));


/* CPUID Fn8000_0007_EDX[HwPstate(7)] = 1 */
static uint8_t supports_pstates_amd (void)
{
    uint32_t eax, ebx, ecx, edx;

    cpuid(0x80000007, &eax, &ebx, &ecx, &edx);
    machine_state.have_pstate = !!(edx & (1 << 7));
    machine_state.have_coreboost = !!(edx & (1<<9));
    machine_state.have_feedback = !!(edx & (1<<11));

    cpuid(0x6, &eax, &ebx, &ecx, &edx);
    machine_state.have_pstate_hw_coord =  !!(ecx & 1); 

    INFO("P-State: AMD: Pstates=%d Coreboost=%d Feedback=%d PstateHWCoord=%d\n",
            machine_state.have_pstate, 
            machine_state.have_coreboost, 
            machine_state.have_feedback,
            machine_state.have_pstate_hw_coord);

    return machine_state.have_pstate;


}


static void init_arch_amd(void)
{
    /* KCH: nothing to do here */
}


static void deinit_arch_amd(void)
{
    /* KCH: nothing to do here */
}


static uint8_t get_pstate_amd(void) 
{
    struct p_state_stat_reg_amd pstat;

    rdmsrl(MSR_PSTATE_STAT_REG_AMD, pstat.val);

    get_cpu_var(core_state).cur_pstate=pstat.reg.pstate;
    put_cpu_var(core_state);

    return pstat.reg.pstate;
}


static void set_pstate_amd(uint8_t p)
{
    struct p_state_ctl_reg_amd pctl;
    pctl.val = 0;
    pctl.reg.cmd = p;

    wrmsrl(MSR_PSTATE_CTL_REG_AMD, pctl.val);

    get_cpu_var(core_state).cur_pstate=p;
    put_cpu_var(core_state);
}


/*
 * NOTE: HW may change this value at runtime
 */
static uint8_t get_max_pstate_amd(void)
{
    struct p_state_limit_reg_amd plimits;

    rdmsrl(MSR_PSTATE_LIMIT_REG_AMD, plimits.val);

    return plimits.reg.pstate_max;
}


static uint8_t get_min_pstate_amd(void)
{
    struct p_state_limit_reg_amd plimits;

    rdmsrl(MSR_PSTATE_LIMIT_REG_AMD, plimits.val);

    return plimits.reg.pstate_limit;
}


static struct pstate_core_funcs amd_funcs =
{
    .arch_init        = init_arch_amd,
    .arch_deinit      = deinit_arch_amd,
    .get_pstate       = get_pstate_amd,
    .set_pstate       = set_pstate_amd,
    .get_max_pstate   = get_max_pstate_amd,
    .get_min_pstate   = get_min_pstate_amd,
};



/***********************************************************
  INTEL DIRECT CONTROL
 **********************************************************/


/*
   This implementation uses SpeedStep, but does check
   to see if the other features (MPERF/APERF, Turbo/IDA, HWP)
   are available.
   */

/* Intel System Programmer's Manual Vol. 3B, 14-2 */
#define MSR_MPERF_IA32         0x000000e7
#define MSR_APERF_IA32         0x000000e8
#define MSR_MISC_ENABLE_IA32   0x000001a0
#define MSR_NHM_TURBO_RATIO_LIMIT   0x000001ad
#define MSR_PLATFORM_INFO_IA32 0x000000ce
#define MSR_PERF_CTL_IA32      0x00000199
#define MSR_PERF_STAT_IA32     0x00000198
#define MSR_ENERY_PERF_BIAS_IA32 0x000001b0


/* Note that the actual  meaning of the pstate
   in the control and status registers is actually
   implementation dependent, unlike AMD.   The "official"
   way to figure it out the mapping from pstate to 
   these values is via ACPI.  What is written in the register
   is an "id" of an operation point

   "Often", the 16 bit field consists of a high order byte
   which is the frequency (the multiplier) and the low order
   byte is the voltage. 
   */
// MSR_PERF_CTL_IA32  r/w
struct perf_ctl_reg_intel {
    union {
        uint64_t val;
        struct {
            // This is the target
            // Note, not the ACPI pstate, but
            // Intel's notion of pstate is that it's opaque
            // for lots of implementations it seems to be
            // frequency_id : voltage_id
            // where frequency_id is typically the multiplier
            uint16_t pstate                 : 16;
            uint16_t reserved               : 16;
            // set to 1 to *disengage* dynamic acceleration
            // Note that "IDA" and "Turbo" use the same interface
            uint16_t dynamic_accel_disable  : 1;
            uint32_t reserved2              : 31;
        } reg;
    } __attribute__((packed));
} __attribute__((packed));

// MSR_PERF_STAT_IA32 r
struct perf_stat_reg_intel {
    union {
        uint64_t val;
        struct {
            // this is the current
            uint16_t pstate                 : 16;
            uint64_t reserved               : 48;
        } reg;
    } __attribute__((packed));
} __attribute__((packed));

// MSR_ENERGY_PERF_BIAS_IA32 r/w
struct enery_perf_bias_reg_intel {
    union {
        uint64_t val;
        struct {
            // this is the current
            uint8_t  policy_hint            : 4;
            uint64_t reserved               : 60;
        } reg;
    } __attribute__((packed));
} __attribute__((packed));

// MSR_PLATFORM_INFO
struct turbo_mode_info_reg_intel {
    union {
        uint64_t val;
        struct {
            uint8_t  rsvd0                  : 8;
            uint8_t  max_noturbo_ratio      : 8;
            uint8_t  rsvd1                  : 7;
            uint8_t  ppin_cap               : 1;
            uint8_t  rsvd2                  : 4;
            uint8_t  ratio_limit            : 1; 
            uint8_t  tdc_tdp_limit          : 1;
            uint16_t rsvd3                  : 10;
            uint8_t  min_ratio              : 8;
            uint16_t rsvd4                  : 16;
        } reg;
    } __attribute__((packed));
} __attribute__((packed));


/* CPUID.01:ECX.AES(7) */
static uint8_t supports_pstates_intel(void)
{
    /* NOTE: CPUID.06H:ECX.SETBH[bit 3] is set and it also implies the presence of a new architectural MSR called IA32_ENERGY_PERF_BIAS (1B0H).
    */
    uint32_t eax, ebx, ecx, edx;

    cpuid(0x1, &eax, &ebx, &ecx, &edx);
    machine_state.have_speedstep =  !!(ecx & (1 << 7));

    cpuid(0x6, &eax, &ebx, &ecx, &edx);
    machine_state.have_pstate_hw_coord =  !!(ecx & 1); // ?
    machine_state.have_opportunistic =  !!(eax & 1<<1);
    machine_state.have_policy_hint = !!(ecx & 1<<3);
    machine_state.have_hwp = !!(eax & 1<<7);
    machine_state.have_hdc = !!(eax & 1<<13);

    cpuid(0x5, &eax, &ebx, &ecx, &edx);
    machine_state.have_mwait_ext =  !!(ecx & 1);
    machine_state.have_mwait_int =  !!(ecx & 1<<1);


    INFO("P-State: Intel: Speedstep=%d, PstateHWCoord=%d, Opportunistic=%d PolicyHint=%d HWP=%d HDC=%d, MwaitExt=%d MwaitInt=%d \n",
            machine_state.have_speedstep, 
            machine_state.have_pstate_hw_coord, 
            machine_state.have_opportunistic,
            machine_state.have_policy_hint,
            machine_state.have_hwp,
            machine_state.have_hdc,
            machine_state.have_mwait_ext,
            machine_state.have_mwait_int );

    return machine_state.have_speedstep;
}


static void init_arch_intel(void)
{
    uint64_t val;

    rdmsrl(MSR_MISC_ENABLE_IA32, val);

    // store prior speedstep setting
    get_cpu_var(core_state).prior_speedstep=(val >> 16) & 0x1;
    put_cpu_var(core_state);

    // enable speedstep (probably already on)
    val |= 1 << 16;
    wrmsrl(MSR_MISC_ENABLE_IA32, val);

}

static void deinit_arch_intel(void)
{
    uint64_t val;

    rdmsrl(MSR_MISC_ENABLE_IA32, val);

    val &= ~(1ULL << 16);
    val |= get_cpu_var(core_state).prior_speedstep << 16;
    put_cpu_var(core_state);

    wrmsrl(MSR_MISC_ENABLE_IA32, val);

}

/* TODO: Intel P-states require sampling at intervals... */
static uint8_t get_pstate_intel(void)
{
    uint64_t val;
    uint16_t pstate;

    rdmsrl(MSR_PERF_STAT_IA32,val);

    pstate = val & 0xffff;

    INFO("P-State: Get: 0x%llx\n", val);

    // Assume top byte is the FID
    //if (pstate & 0xff ) { 
    //  ERROR("P-State: Intel returns confusing pstate %u\n",pstate);
    //}

    // should check if turbo is active, in which case 
    // this value is not the whole story

    return (uint8_t) (pstate>>8);
}

static void set_pstate_intel(uint8_t p)
{
    uint64_t val;

    /* ...Intel IDA (dynamic acceleration)
       if (c->no_turbo && !c->turbo_disabled) {
       val |= 1 << 32;
       }
       */
    // leave all bits along expect for the likely
    // fid bits

    rdmsrl(MSR_PERF_CTL_IA32, val);
    val &= ~0xff00ULL;
    val |= ((uint64_t)p)<<8;

    INFO("P-State: Set: 0x%llx\n", val);

    wrmsrl(MSR_PERF_CTL_IA32, val);

    get_cpu_var(core_state).cur_pstate = p;
    put_cpu_var(core_state);
}


static uint8_t get_min_pstate_intel(void)
{
    struct turbo_mode_info_reg_intel t;

    rdmsrl(MSR_PLATFORM_INFO_IA32, t.val);

    return t.reg.min_ratio;
}



static uint8_t get_max_pstate_intel (void)
{
    struct turbo_mode_info_reg_intel t;

    rdmsrl(MSR_PLATFORM_INFO_IA32, t.val);

    return t.reg.max_noturbo_ratio;
}

static struct pstate_core_funcs intel_funcs =
{
    .arch_init        = init_arch_intel,
    .arch_deinit      = deinit_arch_intel,
    .get_pstate       = get_pstate_intel,
    .set_pstate       = set_pstate_intel,
    .get_max_pstate   = get_max_pstate_intel,
    .get_min_pstate   = get_min_pstate_intel,
};



/***********************************************
  Arch determination and setup
 ***********************************************/

static inline void cpuid_string (uint32_t id, uint32_t dest[4]) 
{
    asm volatile("cpuid"
            :"=a"(*dest),"=b"(*(dest+1)),"=c"(*(dest+2)),"=d"(*(dest+3))
            :"a"(id));
}


static int get_cpu_vendor (char name[13])
{
    uint32_t dest[4];
    uint32_t maxid;

    cpuid_string(0,dest);
    maxid=dest[0];
    ((uint32_t*)name)[0]=dest[1];
    ((uint32_t*)name)[1]=dest[3];
    ((uint32_t*)name)[2]=dest[2];
    name[12]=0;

    return maxid;
}


static int is_intel (void)
{
    char name[13];
    get_cpu_vendor(name);
    return !strcmp(name,"GenuineIntel");
}


static int is_amd (void)
{
    char name[13];
    get_cpu_vendor(name);
    return !strcmp(name,"AuthenticAMD");
}

static int pstate_arch_setup(void)
{

    if (is_amd()) {
        machine_state.arch = AMD;
        machine_state.funcs = &amd_funcs;
        machine_state.supports_pstates = supports_pstates_amd();
        INFO("PSTATE: P-State initialized for AMD\n");
    } else if (is_intel()) {
        machine_state.arch  = INTEL;
        machine_state.funcs = &intel_funcs;
        machine_state.supports_pstates = supports_pstates_intel();
        INFO("PSTATE: P-State initialized for INTEL (Work in progress...)\n");
        return 0;

    } else {
        machine_state.arch = OTHER;
        machine_state.funcs = NULL;
        machine_state.supports_pstates = 0;
        INFO("PSTATE: P-state control: No support for direct control on this architecture\n");
        return 0;
    }

    return 0;
}



/******************************************************************
  Linux Interface
 *****************************************************************/


/* 
 * This stub governor is simply a placeholder for preventing 
 * frequency changes from the Linux side. For now, we simply leave
 * the frequency as is when we acquire control. 
 */
static int governor_run(struct cpufreq_policy *policy, unsigned int event)
{

    switch (event) {
        /* we can't use cpufreq_driver_target here as it can result
         * in a circular dependency, so we'll just do nothing.
         */
        case CPUFREQ_GOV_START:
        case CPUFREQ_GOV_STOP:
        case CPUFREQ_GOV_LIMITS:
            /* do nothing */
            break;
        default:
            ERROR("Undefined governor command\n");
            return -1;
    }				

    return 0;
}


static struct cpufreq_governor stub_governor = 
{
    .name = PALACIOS_GOVNAME,
    .governor = governor_run,
    .owner = THIS_MODULE,
};


static inline void pstate_register_linux_governor(void)
{
    cpufreq_register_governor(&stub_governor);
}


static inline void pstate_unregister_linux_governor(void)
{
    cpufreq_unregister_governor(&stub_governor);
}


static int get_current_governor(char **buf, unsigned int cpu)
{
    struct cpufreq_policy * policy = palacios_alloc(sizeof(struct cpufreq_policy));
    char * govname = NULL;

    if (!policy) {
        ERROR("could not allocate cpufreq_policy\n");
        return -1;
    }
        
    if (cpufreq_get_policy(policy, cpu) != 0) {
        ERROR("Could not get current cpufreq policy\n");
        goto out_err;
    }

    /* We're in interrupt context, should probably not wait here */
    govname = palacios_alloc(MAX_GOV_NAME_LEN);
    if (!govname) {
        ERROR("Could not allocate space for governor name\n");
        goto out_err;
    }

    strncpy(govname, policy->governor->name, MAX_GOV_NAME_LEN);

    get_cpu_var(core_state).linux_governor = govname;
    put_cpu_var(core_state);

    *buf = govname;

    palacios_free(policy);

    return 0;

out_err:
    palacios_free(policy);
    return -1;
}


/* passed to the userspacehelper interface for cleanup */
static void gov_switch_cleanup(struct subprocess_info * s)
{
    palacios_free(s->argv[2]);
    palacios_free(s->argv);
}


/* 
 * Switch governors
 * @s - the governor to switch to 
 */
static int governor_switch(char * s, unsigned int cpu)
{
    char * path_str = NULL;
    char ** argv = NULL; 

    static char * envp[] = {
        "HOME=/",
        "TERM=linux",
        "PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL };


    argv = palacios_alloc(4*sizeof(char*));
    if (!argv) {
        ERROR("Couldn't allocate argv struct\n");
        return -1;
    }

    path_str = palacios_alloc(MAX_PATH_LEN);
    if (!path_str) {
        ERROR("Couldn't allocate path string\n");
        goto out_freeargv;
    }
    memset(path_str, 0, MAX_PATH_LEN);

    snprintf(path_str, MAX_PATH_LEN, "echo %s > /sys/devices/system/cpu/cpu%u/cpufreq/scaling_governor", s, cpu);

    argv[0] = "/bin/sh";
    argv[1] = "-c";
    argv[2] = path_str;
    argv[3] = NULL;

    /* KCH: we can't wait here to actually see if we succeeded, we're in interrupt context */
    return call_usermodehelper_fns("/bin/sh", argv, envp, UMH_NO_WAIT, NULL, gov_switch_cleanup, NULL);

out_freeargv:
    palacios_free(argv);
    return -1;
}


static inline void free_linux_governor(void)
{
    palacios_free(get_cpu_var(core_state).linux_governor);
    put_cpu_var(core_state);
}


static int linux_setup_palacios_governor(void)
{
    char * gov;
    unsigned int cpu = get_cpu();

    /* KCH:  we assume the v3vee governor is already 
     * registered with kernel by this point 
     */

    if (get_current_governor(&gov, cpu) < 0) {
        ERROR("Could not get current governor\n");
        return -1;
    }

    DEBUG("saving current governor (%s)\n", gov);

    get_cpu_var(core_state).linux_governor = gov;
    put_cpu_var(core_state);
    
    DEBUG("setting the new governor (%s)\n", PALACIOS_GOVNAME);

    /* set the new one to ours */
    if (governor_switch(PALACIOS_GOVNAME, cpu) < 0) {
        ERROR("Could not set governor to (%s)\n", PALACIOS_GOVNAME);
        return -1;
    }

    return 0;
}


#if 0
static int linux_deinit(void)
{
    return 0;
}
#endif


static int linux_get_pstate(void)
{
    struct cpufreq_policy * policy = NULL;
    struct cpufreq_frequency_table *table;
    int cpu = get_cpu();
    unsigned int i = 0;
    unsigned int count = 0;

    policy = palacios_alloc(sizeof(struct cpufreq_policy));
    if (!policy) {
        ERROR("Could not allocate policy struct\n");
        return -1;
    }

    cpufreq_get_policy(policy, cpu);
    table = cpufreq_frequency_get_table(cpu);

    for (i = 0; (table[i].frequency != CPUFREQ_TABLE_END); i++) {

        if (table[i].frequency == CPUFREQ_ENTRY_INVALID) {
            continue;
        }

        if (table[i].frequency == policy->cur) {
            break;
        }

        count++;
    }

    palacios_free(policy);
    return count;
}


static int linux_get_freq(void)
{
    struct cpufreq_policy * policy = NULL;
    int cpu = get_cpu();

    policy = palacios_alloc(sizeof(struct cpufreq_policy));
    if (!policy) {
        ERROR("Could not allocate policy struct\n");
        return -1;
    }

    if (cpufreq_get_policy(policy, cpu)) {
        ERROR("Could not get current policy\n");
        return -1;
    }

    return policy->cur;
}


static int linux_set_pstate(uint8_t p)
{
    struct cpufreq_policy * policy = NULL;
    struct cpufreq_frequency_table *table;
    int cpu = get_cpu();
    unsigned int i = 0;
    unsigned int count = 0;
    int state_set = 0;
    int last_valid = 0;

    policy = palacios_alloc(sizeof(struct cpufreq_policy));
    if (!policy) {
        ERROR("Could not allocate policy struct\n");
        return -1;
    }

    if (cpufreq_get_policy(policy, cpu)) {
        ERROR("Could not get current policy\n");
        goto out_err;
    }
    table = cpufreq_frequency_get_table(cpu);

    for (i = 0; (table[i].frequency != CPUFREQ_TABLE_END); i++) {

        if (table[i].frequency == CPUFREQ_ENTRY_INVALID) {
            continue;
        }

        if (count == p) {
            cpufreq_driver_target(policy, table[i].frequency, CPUFREQ_RELATION_H);
            state_set = 1;
        }

        count++;
        last_valid = i;
    }

    /* we need to deal with the case in which we get a number > max pstate */
    if (!state_set) {
        cpufreq_driver_target(policy, table[last_valid].frequency, CPUFREQ_RELATION_H);
    }

    palacios_free(policy);
    return 0;

out_err:
    palacios_free(policy);
    return -1;
}


static int linux_set_freq(uint64_t f)
{
    struct cpufreq_policy * policy = NULL;
    int cpu = get_cpu();
    uint64_t freq;

    policy = palacios_alloc(sizeof(struct cpufreq_policy));
    if (!policy) {
        ERROR("Could not allocate policy struct\n");
        return -1;
    }

    cpufreq_get_policy(policy, cpu);

    if (f < policy->min) {
        freq = policy->min;
    } else if (f > policy->max) {
        freq = policy->max;
    } else {
        freq = f;
    }

    cpufreq_driver_target(policy, freq, CPUFREQ_RELATION_H);

    palacios_free(policy);
    return 0;
}


static int linux_restore_defaults(void)
{
    unsigned int cpu = get_cpu();
    char * gov = NULL;

    gov = get_cpu_var(core_state).linux_governor;
    put_cpu_var(core_state);

    DEBUG("restoring previous governor (%s)\n", gov);

    if (governor_switch(gov, cpu) < 0) {
        ERROR("Could not restore governor to (%s)\n", gov);
        goto out_err;
    }

    free_linux_governor();
    return 0;

out_err:
    free_linux_governor();
    return -1;
}



/******************************************************************
  Generic Interface as provided to Palacios and to the rest of the
  module
 ******************************************************************/

static void init_core(void)
{
    unsigned cpu;
    struct cpufreq_policy *p;


    DEBUG("P-State Core Init\n");

    get_cpu_var(core_state).mode = V3_PSTATE_HOST_CONTROL;
    get_cpu_var(core_state).cur_pstate = 0;

    if (machine_state.funcs) {
        get_cpu_var(core_state).min_pstate = machine_state.funcs->get_min_pstate();
        get_cpu_var(core_state).max_pstate = machine_state.funcs->get_max_pstate();
    } else {
        get_cpu_var(core_state).min_pstate = 0;
        get_cpu_var(core_state).max_pstate = 0;
    }


    cpu = get_cpu(); put_cpu();

    p = cpufreq_cpu_get(cpu);

    if (!p) { 
        get_cpu_var(core_state).have_cpufreq = 0;
        get_cpu_var(core_state).min_freq_khz=0;
        get_cpu_var(core_state).max_freq_khz=0;
        get_cpu_var(core_state).cur_freq_khz=0;
    } else {
        get_cpu_var(core_state).have_cpufreq = 1;
        get_cpu_var(core_state).min_freq_khz=p->min;
        get_cpu_var(core_state).max_freq_khz=p->max;
        get_cpu_var(core_state).cur_freq_khz=p->cur;
        cpufreq_cpu_put(p);
    }

    put_cpu_var(core_state);

}


void palacios_pstate_ctrl_release(void);


static void deinit_core(void)
{
    int cpu;
    DEBUG("P-State Core Deinit\n");
    cpu = get_cpu();
    palacios_pstate_ctrl_release();
}



void palacios_pstate_ctrl_get_chars(struct v3_cpu_pstate_chars *c) 
{
    memset(c,0,sizeof(struct v3_cpu_pstate_chars));


    c->features = V3_PSTATE_INTERNAL_CONTROL;

    if (get_cpu_var(core_state).have_cpufreq) {
        c->features |= V3_PSTATE_EXTERNAL_CONTROL;
    }

    if (machine_state.arch==AMD || machine_state.arch==INTEL) { 
        c->features |= V3_PSTATE_DIRECT_CONTROL;
    }
    c->cur_mode = get_cpu_var(core_state).mode;
    c->min_pstate = get_cpu_var(core_state).min_pstate;
    c->max_pstate = get_cpu_var(core_state).max_pstate;
    c->cur_pstate = get_cpu_var(core_state).cur_pstate;
    c->min_freq_khz = get_cpu_var(core_state).min_freq_khz;
    c->max_freq_khz = get_cpu_var(core_state).max_freq_khz;
    c->cur_freq_khz = get_cpu_var(core_state).cur_freq_khz;

    put_cpu_var(core_state);



}


uint8_t palacios_pstate_ctrl_get_pstate(void)
{
    if (get_cpu_var(core_state).mode==V3_PSTATE_DIRECT_CONTROL) { 
        put_cpu_var(core_state);
        return machine_state.funcs->get_pstate();
    } else if (get_cpu_var(core_state).mode==V3_PSTATE_EXTERNAL_CONTROL) {
        put_cpu_var(core_state);
        return linux_get_pstate();
    } else {
        put_cpu_var(core_state);
        return 0;
    }
}


void palacios_pstate_ctrl_set_pstate(uint8_t p)
{
    if (get_cpu_var(core_state).mode==V3_PSTATE_DIRECT_CONTROL) { 
        put_cpu_var(core_state);
        machine_state.funcs->set_pstate(p);
    } else if (get_cpu_var(core_state).mode==V3_PSTATE_EXTERNAL_CONTROL) {
        put_cpu_var(core_state);
        linux_set_pstate(p);
    } 
}


void palacios_pstate_ctrl_set_pstate_wrapper(void *p)
{
    palacios_pstate_ctrl_set_pstate((uint8_t)(uint64_t)p);
}


uint64_t palacios_pstate_ctrl_get_freq(void)
{
    if (get_cpu_var(core_state).mode==V3_PSTATE_EXTERNAL_CONTROL) { 
        put_cpu_var(core_state);
        return linux_get_freq();
    } else {
        put_cpu_var(core_state);
        return 0;
    }
}


void palacios_pstate_ctrl_set_freq(uint64_t p)
{
    if (get_cpu_var(core_state).mode==V3_PSTATE_EXTERNAL_CONTROL) { 
        put_cpu_var(core_state);
        linux_set_freq(p);
    } 
    put_cpu_var(core_state);
}


static int switch_to_external(void)
{
    if (!(get_cpu_var(core_state).have_cpufreq)) {
        put_cpu_var(core_state);
        ERROR("No cpufreq  - cannot switch to external...\n");
        return -1;
    }
    put_cpu_var(core_state);

    DEBUG("Switching to external control\n");
    return linux_restore_defaults();
}


static int switch_to_direct(void)
{
    if (get_cpu_var(core_state).have_cpufreq) { 
        put_cpu_var(core_state);
        DEBUG("switch to direct from cpufreq\n");

        // The implementation would set the policy and governor to peg cpu
        // regardless of load
        linux_setup_palacios_governor();
    }

    if (machine_state.funcs && machine_state.funcs->arch_init) {
        get_cpu_var(core_state).mode=V3_PSTATE_DIRECT_CONTROL;

        machine_state.funcs->arch_init();

        put_cpu_var(core_state);
    }

    return 0;
}


static int switch_to_internal(void)
{
    if (get_cpu_var(core_state).have_cpufreq) { 
        put_cpu_var(core_state);
        DEBUG("switch to internal on machine with cpu freq\n");
        linux_setup_palacios_governor();
    }

    get_cpu_var(core_state).mode=V3_PSTATE_INTERNAL_CONTROL;

    put_cpu_var(core_state);

    return 0;
}


static int switch_from_external(void)
{
    if (!(get_cpu_var(core_state).have_cpufreq)) {
        put_cpu_var(core_state);
        ERROR("No cpufreq  - how did we get here... external...\n");
        return -1;
    }

    DEBUG("Switching from external...\n");
    linux_restore_defaults();

    get_cpu_var(core_state).mode = V3_PSTATE_HOST_CONTROL;

    put_cpu_var(core_state);

    return 0;
}


static int switch_from_direct(void)
{
    if (get_cpu_var(core_state).have_cpufreq) { 
        put_cpu_var(core_state);
        DEBUG("Switching back to cpufreq control from direct\n");
        linux_restore_defaults();
    }

    get_cpu_var(core_state).mode=V3_PSTATE_HOST_CONTROL;

    machine_state.funcs->set_pstate(get_cpu_var(core_state).min_pstate);

    machine_state.funcs->arch_deinit();

    put_cpu_var(core_state);

    return 0;
}


static int switch_from_internal(void)
{
    if (get_cpu_var(core_state).have_cpufreq) { 
        put_cpu_var(core_state);
        ERROR("Unimplemented: switch from internal on machine with cpu freq - will just pretend to do so\n");
        // The implementation would switch back to default policy and governor
        linux_restore_defaults();
    }

    get_cpu_var(core_state).mode=V3_PSTATE_HOST_CONTROL;

    put_cpu_var(core_state);

    return 0;
}



void palacios_pstate_ctrl_acquire(uint32_t type)
{
    if (get_cpu_var(core_state).mode != V3_PSTATE_HOST_CONTROL) { 
        palacios_pstate_ctrl_release();
    }

    put_cpu_var(core_state);

    switch (type) { 
        case V3_PSTATE_EXTERNAL_CONTROL:
            switch_to_external();
            break;
        case V3_PSTATE_DIRECT_CONTROL:
            switch_to_direct();
            break;
        case V3_PSTATE_INTERNAL_CONTROL:
            switch_to_internal();
            break;
        default:
            ERROR("Unknown pstate control type %u\n",type);
            break;
    }

}

// Wrappers for xcalls
static void palacios_pstate_ctrl_acquire_external(void)
{
    palacios_pstate_ctrl_acquire(V3_PSTATE_EXTERNAL_CONTROL);
}

static void palacios_pstate_ctrl_acquire_direct(void)
{
    palacios_pstate_ctrl_acquire(V3_PSTATE_DIRECT_CONTROL);
}


void palacios_pstate_ctrl_release(void)
{
    if (get_cpu_var(core_state).mode == V3_PSTATE_HOST_CONTROL) { 
        put_cpu_var(core_state);
        return;
    }

    switch (get_cpu_var(core_state).mode) { 
        case V3_PSTATE_EXTERNAL_CONTROL:
            switch_from_external();
            break;
        case V3_PSTATE_DIRECT_CONTROL:
            switch_from_direct();
            break;
        case V3_PSTATE_INTERNAL_CONTROL:
            switch_from_internal();
            break;
        default:
            ERROR("Unknown pstate control type %u\n",core_state.mode);
            break;
    }

    put_cpu_var(core_state);

}


static void update_hw_pstate(void *arg)
{
    if (machine_state.funcs && machine_state.funcs->get_pstate) {
        get_cpu_var(core_state).cur_hw_pstate = machine_state.funcs->get_pstate();
        put_cpu_var(core_state);
    } else {
        get_cpu_var(core_state).cur_hw_pstate = 0;
        put_cpu_var(core_state);
    }
}


/***************************************************************************
  PROC Interface to expose state
 ***************************************************************************/

static int pstate_show(struct seq_file * file, void * v)
{
    unsigned int cpu;
    unsigned int numcpus = num_online_cpus();

    seq_printf(file, "V3VEE DVFS Status\n\n");

    for (cpu=0;cpu<numcpus;cpu++) { 
        palacios_xcall(cpu,update_hw_pstate,0);
    }

    seq_printf(file, "Arch:\t%s\nPStates:\t%s\n\n",
            machine_state.arch==INTEL ? "Intel" : 
            machine_state.arch==AMD ? "AMD" : "Other",
            machine_state.supports_pstates ? "Yes" : "No");

    for (cpu=0;cpu<numcpus;cpu++) { 
        struct pstate_core_info *s = &per_cpu(core_state,cpu);
        seq_printf(file,"pcore %u: hw pstate %u mode %s of [ host ",cpu,
                s->cur_hw_pstate,
                s->mode==V3_PSTATE_HOST_CONTROL ? "host" :
                s->mode==V3_PSTATE_EXTERNAL_CONTROL ? "external" :
                s->mode==V3_PSTATE_DIRECT_CONTROL ? "direct" : 
                s->mode==V3_PSTATE_INTERNAL_CONTROL ? "internal" : "UNKNOWN");
        if (s->have_cpufreq) { 
            seq_printf(file,"external ");
        }
        if (machine_state.supports_pstates) {
            seq_printf(file,"direct ");
        }
        seq_printf(file,"internal ] ");
        if (s->mode==V3_PSTATE_EXTERNAL_CONTROL) { 
            seq_printf(file,"(min=%llu max=%llu cur=%llu) ", s->min_freq_khz, s->max_freq_khz, s->cur_freq_khz);
        } 
        if (s->mode==V3_PSTATE_DIRECT_CONTROL) { 
            seq_printf(file,"(min=%u max=%u cur=%u) ", (uint32_t)s->min_pstate, (uint32_t)s->max_pstate, (uint32_t)s->cur_pstate);
        }
        seq_printf(file,"\n");
    }
    return 0;
}

static int pstate_open(struct inode * inode, struct file * file) 
{
    return single_open(file, pstate_show, NULL);
}


static struct file_operations pstate_fops = {
    .owner = THIS_MODULE,
    .open = pstate_open, 
    .read = seq_read,
    .llseek = seq_lseek,
    .release = seq_release
};

int pstate_proc_setup(void)
{
    struct proc_dir_entry *proc;

    proc = create_proc_entry("v3-dvfs",0444, palacios_get_procdir());

    if (!proc) { 
        ERROR("Failed to create proc entry for p-state control\n");
        return -1;
    }

    proc->proc_fops = &pstate_fops;

    return 0;
}

void pstate_proc_teardown(void)
{
    remove_proc_entry("v3-dvfs",palacios_get_procdir());
}

/********************************************************************
  User interface (ioctls)
 ********************************************************************/

static int dvfs_ctrl(unsigned int cmd, unsigned long arg) 
{
    struct v3_dvfs_ctrl_request r;

    if (copy_from_user(&r,(void __user*)arg,sizeof(struct v3_dvfs_ctrl_request))) {
        ERROR("Failed to copy DVFS request from user\n");
        return -EFAULT;
    }

    if (r.pcore >= num_online_cpus()) {
        ERROR("Cannot apply DVFS request to pcore %u\n",r.pcore);
        return -EFAULT;
    }

    switch (r.cmd) {
        case V3_DVFS_ACQUIRE: {
                                  switch (r.acq_type) { 
                                      case V3_DVFS_EXTERNAL:
                                          palacios_xcall(r.pcore,(void (*)(void*))palacios_pstate_ctrl_acquire_external, NULL);
                                          return 0;
                                          break;
                                      case V3_DVFS_DIRECT:
                                          palacios_xcall(r.pcore,(void (*)(void*))palacios_pstate_ctrl_acquire_direct, NULL);
                                          return 0;
                                          break;
                                      default:
                                          ERROR("Unknown DVFS acquire type %u\n",r.acq_type);
                                          return -EFAULT;
                                  }
                              }
                              break;
        case V3_DVFS_RELEASE: {
                                  palacios_xcall(r.pcore,(void (*)(void*))palacios_pstate_ctrl_release, NULL);
                                  return 0;
                              }
                              break;
        case V3_DVFS_SETFREQ: {
                                  palacios_xcall(r.pcore,(void (*)(void*))palacios_pstate_ctrl_set_freq,(void*)r.freq_khz);
                                  return 0;
                              }
                              break;
        case V3_DVFS_SETPSTATE: {
                                    palacios_xcall(r.pcore,(void (*)(void*))palacios_pstate_ctrl_set_pstate_wrapper,(void*)(uint64_t)r.pstate);
                                    return 0;
                                }
        default: {
                     ERROR("Unknown DVFS command %u\n",r.cmd);
                     return -EFAULT;
                 }
                 break;
    }
}


void pstate_user_setup(void)
{
    add_global_ctrl(V3_DVFS_CTRL, dvfs_ctrl);
}


void pstate_user_teardown(void)
{
    remove_global_ctrl(V3_DVFS_CTRL);
}

static struct v3_host_pstate_ctrl_iface hooks = {
    .get_chars = palacios_pstate_ctrl_get_chars,
    .acquire = palacios_pstate_ctrl_acquire,
    .release = palacios_pstate_ctrl_release,
    .set_pstate = palacios_pstate_ctrl_set_pstate,
    .get_pstate = palacios_pstate_ctrl_get_pstate,
    .set_freq = palacios_pstate_ctrl_set_freq,
    .get_freq = palacios_pstate_ctrl_get_freq,
};



static int pstate_ctrl_init(void) 
{
    unsigned int cpu;
    unsigned int numcpus = num_online_cpus();

    pstate_arch_setup();

    for (cpu=0;cpu<numcpus;cpu++) { 
        palacios_xcall(cpu,(void ((*)(void*)))init_core,0);
    }

    V3_Init_Pstate_Ctrl(&hooks);  

    if (pstate_proc_setup()) { 
        ERROR("Unable to initialize P-State Control\n");
        return -1;
    }

    pstate_user_setup();

    pstate_register_linux_governor();

    INFO("P-State Control Initialized\n");

    return 0;
}

static int pstate_ctrl_deinit(void)
{
    unsigned int cpu;
    unsigned int numcpus=num_online_cpus();

    pstate_unregister_linux_governor();

    pstate_user_teardown();

    pstate_proc_teardown();

    // release pstate control if we have it, and we need to do this on each processor
    for (cpu=0;cpu<numcpus;cpu++) { 
        palacios_xcall(cpu,(void (*)(void *))deinit_core,0);
    }

    return 0;
}


static struct linux_ext pstate_ext = {
    .name = "PSTATE_CTRL",
    .init = pstate_ctrl_init,
    .deinit = pstate_ctrl_deinit,
    .guest_init = NULL,
    .guest_deinit = NULL,
};


register_extension(&pstate_ext);



