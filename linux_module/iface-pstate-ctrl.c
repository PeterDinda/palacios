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
#include <linux/cpufreq.h>
#include <linux/kernel.h>
#include <linux/kmod.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <asm/processor.h>
#include <asm/msr.h>
#include <asm/msr-index.h>

// Used to determine the appropriate pstates values on Intel
#include <linux/acpi.h>
#include <acpi/processor.h>

#include <interfaces/vmm_pstate_ctrl.h>

#include "palacios.h"
#include "iface-pstate-ctrl.h"

#include "linux-exts.h"

/*
   This P-STATE control implementation includes the following modes.
   You can switch between modes at any time.

   - Internal control of processor states in Palacios (handoff from Linux)
     When Palacios acuires this control, this module disables Linux cpufreq control
     and allows code within Palacios unfettered access to the DVFS hardware. 
   - Direct control of Intel and AMD processor pstates using code in this module
     When you acquire this control, this module disables Linux cpufreq control
     and directly programs the processor itself in response to your requests
   - External control of processor states via Linux 
     When you acuire this control, this module uses the Linux cpufreq control
     to program the processor on your behelf
   - Host control of processor stastes
     This is the normal mode of DVFS control (e.g., Linux cpufreq)

   Additionally, it provides a user-space interface for manipulating
   p-state regardless of the host's functionality.  This includes
   an ioctl for commanding the implementation and a /proc file for 
   showing current status and capabilities.  From user space, you can
   use the Direct, External, and Host modes.  

   What we mean by "p-state" here is the processor's internal
   configuration.   For AMD, this is defined as being the same as
   the ACPI-defined p-state.  For Intel, it is not.  There, it is the 
   contents of the perf ctl MSR, which is opaque.   We try hard to 
   provide "p-states" that go from 0...max, by analogy or equivalence
   to the ACPI p-states. 

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
    uint64_t cur_pstate;
    uint64_t max_pstate;
    uint64_t min_pstate;

    uint64_t cur_hw_pstate;

    // Apply if we are under the EXTERNAL state
    uint64_t set_freq_khz; // this is the frequency we're hoping to get
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
    uint64_t (*get_min_pstate)(void);
    uint64_t (*get_max_pstate)(void);
    uint64_t (*get_pstate)(void);
    void    (*set_pstate)(uint64_t pstate);
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
    int i;
    int mapwrong=0;
    int amd_num_pstates;

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

    amd_num_pstates = get_cpu_var(processors)->performance->state_count;
    if (amd_num_pstates) { 
	for (i=0;i<amd_num_pstates;i++) { 
	    INFO("P-State: %u: freq=%llu ctrl=%llx%s\n",
		 i, 
		 get_cpu_var(processors)->performance->states[i].core_frequency*1000,
		 get_cpu_var(processors)->performance->states[i].control,
		 get_cpu_var(processors)->performance->states[i].control != i ? (mapwrong=1, " ALERT - CTRL MAPPING NOT 1:1") : "");
	}
    }
    if (mapwrong) { 
	ERROR("P-State: AMD: mapping of pstate and control is not 1:1 on this processor - we will probably not work corrrectly\n");
    }

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


static uint64_t get_pstate_amd(void) 
{
    struct p_state_stat_reg_amd pstat;

    rdmsrl(MSR_PSTATE_STAT_REG_AMD, pstat.val);

    get_cpu_var(core_state).cur_pstate=pstat.reg.pstate;
    put_cpu_var(core_state);

    return pstat.reg.pstate;
}


static void set_pstate_amd(uint64_t p)
{
    struct p_state_ctl_reg_amd pctl;

    if (p>get_cpu_var(core_state).max_pstate) { 
	p=get_cpu_var(core_state).max_pstate;
    }
    put_cpu_var(core_state);

    pctl.val = 0;
    pctl.reg.cmd = p;

    wrmsrl(MSR_PSTATE_CTL_REG_AMD, pctl.val);

    get_cpu_var(core_state).cur_pstate=p;
    put_cpu_var(core_state);
}


/*
 * NOTE: HW may change this value at runtime
 */
static uint64_t get_max_pstate_amd(void)
{
    struct p_state_limit_reg_amd plimits;

    rdmsrl(MSR_PSTATE_LIMIT_REG_AMD, plimits.val);

    return plimits.reg.pstate_max;
}


static uint64_t get_min_pstate_amd(void)
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

// This replicates the critical information in Linux's struct acpi_processor_px
// To make it easier to port to other OSes.    
struct intel_pstate_info {
    uint64_t freq;  // KHz
    uint64_t ctrl;  // What to write into the _CTL MSR to get this
};

// The internal array will be used if we cannot build the table locally
static struct intel_pstate_info *intel_pstate_to_ctrl_internal=0;
static int intel_num_pstates_internal=0;

// These will either point to the internal array or to a constructed array
static struct intel_pstate_info *intel_pstate_to_ctrl=0;
static int intel_num_pstates=0;


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


    // Note we test all the available hardware features documented as of August 2014
    // We are only currently using speed_step, however.

    INFO("P-State: Intel: Speedstep=%d, PstateHWCoord=%d, Opportunistic=%d PolicyHint=%d HWP=%d HDC=%d, MwaitExt=%d MwaitInt=%d \n",
            machine_state.have_speedstep, 
            machine_state.have_pstate_hw_coord, 
            machine_state.have_opportunistic,
            machine_state.have_policy_hint,
            machine_state.have_hwp,
            machine_state.have_hdc,
            machine_state.have_mwait_ext,
            machine_state.have_mwait_int );


    if (machine_state.have_speedstep) {
	uint32_t i;
	// Build mapping table (from "pstate" (0..) to ctrl value for MSR
	if (!(get_cpu_var(processors)) || !(get_cpu_var(processors)->performance) ) { 
	    put_cpu_var(processors);
	    // no acpi...  revert to internal table
	    intel_pstate_to_ctrl=intel_pstate_to_ctrl_internal;
	    intel_num_pstates=intel_num_pstates_internal;
	} else {
	    intel_num_pstates = get_cpu_var(processors)->performance->state_count;
	    if (intel_num_pstates) { 
		intel_pstate_to_ctrl = palacios_alloc(sizeof(struct intel_pstate_info)*intel_num_pstates);
		if (!intel_pstate_to_ctrl) { 
		    ERROR("P-State: Cannot allocate space for mapping...\n");
		    intel_num_pstates=0;
		}
		for (i=0;i<intel_num_pstates;i++) { 
		    intel_pstate_to_ctrl[i].freq = get_cpu_var(processors)->performance->states[i].core_frequency*1000;
		    intel_pstate_to_ctrl[i].ctrl = get_cpu_var(processors)->performance->states[i].control;
		}
		    
	    } else {
		ERROR("P-State: Strange, machine has ACPI DVFS but no states...\n");
	    }
	}
	put_cpu_var(processors);
	INFO("P-State: Intel - State Mapping (%u states) follows\n",intel_num_pstates);
	for (i=0;i<intel_num_pstates;i++) {
	    INFO("P-State: Intel Mapping %u:  freq=%llu  ctrl=%llx\n",
		 i, intel_pstate_to_ctrl[i].freq,intel_pstate_to_ctrl[i].ctrl);
	}
    } else {
	INFO("P-State: Intel:  No speedstep here\n");
    }
	

    return machine_state.have_speedstep;
}


static void init_arch_intel(void)
{
    uint64_t val;

    rdmsrl(MSR_MISC_ENABLE_IA32, val);

    //INFO("P-State: prior ENABLE=%llx\n",val);

    // store prior speedstep setting
    get_cpu_var(core_state).prior_speedstep=(val >> 16) & 0x1;
    put_cpu_var(core_state);

    // enable speedstep (probably already on)
    val |= 1 << 16;
    wrmsrl(MSR_MISC_ENABLE_IA32, val);

    //INFO("P-State: write ENABLE=%llx\n",val);

}

static void deinit_arch_intel(void)
{
    uint64_t val;

    rdmsrl(MSR_MISC_ENABLE_IA32, val);

    //INFO("P-State: deinit: ENABLE=%llx\n",val);

    val &= ~(1ULL << 16);
    val |= get_cpu_var(core_state).prior_speedstep << 16;
    put_cpu_var(core_state);

    wrmsrl(MSR_MISC_ENABLE_IA32, val);

    //INFO("P-state: deinit ENABLE=%llx\n",val);

}

/* TODO: Intel P-states require sampling at intervals... */
static uint64_t get_pstate_intel(void)
{
    uint64_t val;

    rdmsrl(MSR_PERF_STAT_IA32,val);

    //INFO("P-State: Get: 0x%llx\n", val);

    // should check if turbo is active, in which case 
    // this value is not the whole story

    return val;
}

static void set_pstate_intel(uint64_t p)
{
    uint64_t val;
    uint64_t ctrl;

    if (intel_num_pstates==0) { 
	return ;
    } else {
	if (p>=intel_num_pstates) { 
	    p=intel_num_pstates-1;
	}
    }

    ctrl=intel_pstate_to_ctrl[p].ctrl;

    /* ...Intel IDA (dynamic acceleration)
       if (c->no_turbo && !c->turbo_disabled) {
       val |= 1 << 32;
       }
       */
    // leave all bits along expect for the likely
    // fid bits

    rdmsrl(MSR_PERF_CTL_IA32, val);
    //INFO("P-State: Pre-Set: 0x%llx\n", val);

    val &= ~0xffffULL;
    val |= ctrl & 0xffffULL;

    //INFO("P-State: Set: 0x%llx\n", val);

    wrmsrl(MSR_PERF_CTL_IA32, val);

    get_cpu_var(core_state).cur_pstate = p;
    put_cpu_var(core_state);
}


static uint64_t get_min_pstate_intel(void)
{
    return 0;
}



static uint64_t get_max_pstate_intel (void)
{
    if (intel_num_pstates==0) { 
	return 0;
    } else {
	return intel_num_pstates-1;
    }
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

static unsigned cpus_using_v3_governor;
static DEFINE_MUTEX(v3_governor_mutex);

/* KCH: this will tell us when there is an actual frequency transition */
static int v3_cpufreq_notifier(struct notifier_block *nb, unsigned long val,
        void *data)
{
    struct cpufreq_freqs *freq = data;

    if (per_cpu(core_state, freq->cpu).mode != V3_PSTATE_EXTERNAL_CONTROL) {
        return 0;
    }

    if (val == CPUFREQ_POSTCHANGE) {
        DEBUG("P-State: frequency change took effect on cpu %u (now %u kHz)\n",
                freq->cpu, freq->new);
        per_cpu(core_state, freq->cpu).cur_freq_khz = freq->new;
    }

    return 0;

}


static struct notifier_block v3_cpufreq_notifier_block = {
    .notifier_call = v3_cpufreq_notifier
};


/* 
 * This stub governor is simply a placeholder for preventing 
 * frequency changes from the Linux side. For now, we simply leave
 * the frequency as is when we acquire control. 
 */
static int governor_run(struct cpufreq_policy *policy, unsigned int event)
{
    unsigned cpu = policy->cpu;

    switch (event) {
        /* we can't use cpufreq_driver_target here as it can result
         * in a circular dependency, so we'll keep the current frequency as is
         */
        case CPUFREQ_GOV_START:
            BUG_ON(!policy->cur);

            mutex_lock(&v3_governor_mutex);

            if (cpus_using_v3_governor == 0) {
                cpufreq_register_notifier(&v3_cpufreq_notifier_block,
                        CPUFREQ_TRANSITION_NOTIFIER);
            }

            cpus_using_v3_governor++;

            per_cpu(core_state, cpu).set_freq_khz = policy->cur;
            per_cpu(core_state, cpu).cur_freq_khz = policy->cur;
            per_cpu(core_state, cpu).max_freq_khz = policy->max;
            per_cpu(core_state, cpu).min_freq_khz = policy->min;

            mutex_unlock(&v3_governor_mutex);
            break;
        case CPUFREQ_GOV_STOP:
            mutex_lock(&v3_governor_mutex);

            cpus_using_v3_governor--;

            if (cpus_using_v3_governor == 0) {
                cpufreq_unregister_notifier(
                        &v3_cpufreq_notifier_block,
                        CPUFREQ_TRANSITION_NOTIFIER);
            }

            per_cpu(core_state, cpu).set_freq_khz = 0;
            per_cpu(core_state, cpu).cur_freq_khz = 0;
            per_cpu(core_state, cpu).max_freq_khz = 0;
            per_cpu(core_state, cpu).min_freq_khz = 0;

            mutex_unlock(&v3_governor_mutex);
            break;
        case CPUFREQ_GOV_LIMITS:
            /* do nothing */
            break;
        default:
            ERROR("Undefined governor command (%u)\n", event);
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


static struct workqueue_struct *pstate_wq;

typedef struct {
    struct work_struct work;
    uint64_t freq;
} pstate_work_t;



static inline void pstate_register_linux_governor(void)
{
    cpufreq_register_governor(&stub_governor);
}


static inline void pstate_unregister_linux_governor(void)
{
    cpufreq_unregister_governor(&stub_governor);
}


static int pstate_linux_init(void)
{
    pstate_register_linux_governor();
    pstate_wq = create_workqueue("v3vee_pstate_wq");
    if (!pstate_wq) {
        ERROR("Could not create work queue\n");
        goto out_err;
    }

    return 0;

out_err:
    pstate_unregister_linux_governor();
    return -1;
}


static void pstate_linux_deinit(void)
{
    pstate_unregister_linux_governor();
    flush_workqueue(pstate_wq);
    destroy_workqueue(pstate_wq);
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
 * TODO: this should probably be submitted to a work queue
 * so we don't have to run it in interrupt context
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
    put_cpu();

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



static uint64_t linux_get_pstate(void)
{
    struct cpufreq_policy * policy = NULL;
    struct cpufreq_frequency_table *table;
    unsigned int i = 0;
    unsigned int count = 0;
    unsigned int cpu = get_cpu(); 
    put_cpu();


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

    put_cpu();
    return count;
}


static uint64_t linux_get_freq(void)
{
    uint64_t freq;
    struct cpufreq_policy * policy = NULL;
    unsigned int cpu = get_cpu();
    put_cpu();

    policy = palacios_alloc(sizeof(struct cpufreq_policy));
    if (!policy) {
        ERROR("Could not allocate policy struct\n");
        return -1;
    }

    if (cpufreq_get_policy(policy, cpu)) {
        ERROR("Could not get current policy\n");
        return -1;
    }

    freq=policy->cur;

    palacios_free(policy);

    return freq;
}

static void  
pstate_switch_workfn (struct work_struct *work)
{
    pstate_work_t * pwork = (pstate_work_t*)work;
    struct cpufreq_policy * policy = NULL;
    uint64_t freq; 
    unsigned int cpu = get_cpu();
    put_cpu();

    mutex_lock(&v3_governor_mutex);

    policy = palacios_alloc(sizeof(struct cpufreq_policy));
    if (!policy) {
        ERROR("Could not allocate space for cpufreq policy\n");
        goto out;
    }

    if (cpufreq_get_policy(policy, cpu) != 0) {
        ERROR("Could not get cpufreq policy\n");
        goto out1;
    }

    freq = pwork->freq;
    get_cpu_var(core_state).set_freq_khz = freq;

    if (freq < get_cpu_var(core_state).min_freq_khz) {
        freq = get_cpu_var(core_state).min_freq_khz;
    }
    if (freq > get_cpu_var(core_state).max_freq_khz) {
        freq = get_cpu_var(core_state).max_freq_khz;
    }
    put_cpu_var(core_state);

    INFO("P-state: requesting frequency change on core %u to %llu\n", cpu, freq);
    __cpufreq_driver_target(policy, freq, CPUFREQ_RELATION_L);

out1:
    palacios_free(policy);
out:
    palacios_free(work);
    mutex_unlock(&v3_governor_mutex);
} 


static int linux_set_pstate(uint64_t p)
{
    struct cpufreq_policy * policy = NULL;
    struct cpufreq_frequency_table *table;
    pstate_work_t * work = NULL;
    unsigned int i = 0;
    unsigned int count = 0;
    int state_set = 0;
    int last_valid = 0;
    unsigned int cpu = get_cpu();
    put_cpu();

    policy = palacios_alloc(sizeof(struct cpufreq_policy));
    if (!policy) {
        ERROR("Could not allocate policy struct\n");
        return -1;
    }

    work = (pstate_work_t*)palacios_alloc(sizeof(pstate_work_t));
    if (!work) {
        ERROR("Could not allocate work struct\n");
        goto out_err;
    }

    if (cpufreq_get_policy(policy, cpu)) {
        ERROR("Could not get current policy\n");
        goto out_err1;
    }
    table = cpufreq_frequency_get_table(cpu);

    for (i = 0; (table[i].frequency != CPUFREQ_TABLE_END); i++) {

        if (table[i].frequency == CPUFREQ_ENTRY_INVALID) {
            continue;
        }

        if (count == p) {

            INIT_WORK((struct work_struct*)work, pstate_switch_workfn);
            work->freq = table[i].frequency;
            queue_work(pstate_wq, (struct work_struct*)work);

            state_set = 1;
            break;
        }

        count++;
        last_valid = i;
    }

    /* we need to deal with the case in which we get a number > max pstate */
    if (!state_set) {
        INIT_WORK((struct work_struct*)work, pstate_switch_workfn);
        work->freq = table[last_valid].frequency;
        queue_work(pstate_wq, (struct work_struct*)work);
    }

    palacios_free(policy);
    return 0;

out_err1: 
    palacios_free(work);
out_err:
    palacios_free(policy);
    return -1;
}


static int linux_set_freq(uint64_t f)
{
    struct cpufreq_policy * policy = NULL;
    pstate_work_t * work = NULL;
    uint64_t freq;
    unsigned int cpu = get_cpu();
    put_cpu();

    policy = palacios_alloc(sizeof(struct cpufreq_policy));
    if (!policy) {
        ERROR("Could not allocate policy struct\n");
        return -1;
    }

    work = (pstate_work_t*)palacios_alloc(sizeof(pstate_work_t));
    if (!work) {
        ERROR("Could not allocate work struct\n");
        goto out_err;
    }

    if (cpufreq_get_policy(policy, cpu) != 0) {
        ERROR("Could not get cpufreq policy\n");
        goto out_err1;
    }

    if (f < policy->min) {
        freq = policy->min;
    } else if (f > policy->max) {
        freq = policy->max;
    } else {
        freq = f;
    }

    INIT_WORK((struct work_struct*)work, pstate_switch_workfn);
    work->freq = freq;
    queue_work(pstate_wq, (struct work_struct*)work);

    palacios_free(policy);
    return 0;

out_err1:
    palacios_free(work);
out_err:
    palacios_free(policy);
    return -1;
}


static int linux_restore_defaults(void)
{
    char * gov = NULL;
    unsigned int cpu = get_cpu();
    put_cpu();

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


    //DEBUG("P-State Core Init\n");

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
        get_cpu_var(core_state).cur_freq_khz=p->cur; } cpufreq_cpu_put(p); 
    put_cpu_var(core_state);

    /*
    for (i=0;i<get_cpu_var(processors)->performance->state_count; i++) { 
        INFO("P-State: %u: freq=%llu ctrl=%llx",
		i, 
		get_cpu_var(processors)->performance->states[i].core_frequency*1000,
		get_cpu_var(processors)->performance->states[i].control);
   }
   put_cpu_var(processors);
    */
}


void palacios_pstate_ctrl_release(void);


static void deinit_core(void)
{
    DEBUG("P-State Core Deinit\n");
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


uint64_t palacios_pstate_ctrl_get_pstate(void)
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


void palacios_pstate_ctrl_set_pstate(uint64_t p)
{
    if (get_cpu_var(core_state).mode==V3_PSTATE_DIRECT_CONTROL) { 
        put_cpu_var(core_state);
        machine_state.funcs->set_pstate(p);
    } else if (get_cpu_var(core_state).mode==V3_PSTATE_EXTERNAL_CONTROL) {
        put_cpu_var(core_state);
        linux_set_pstate(p);
    } else {
        put_cpu_var(core_state);
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
    } else {
        put_cpu_var(core_state);
    }
}


static int switch_to_external(void)
{
    DEBUG("switch from host control to external\n");

    if (!(get_cpu_var(core_state).have_cpufreq)) {
        put_cpu_var(core_state);
        ERROR("No cpufreq  - cannot switch to external...\n");
        return -1;
    } 
    put_cpu_var(core_state);

    linux_setup_palacios_governor();

    get_cpu_var(core_state).mode=V3_PSTATE_EXTERNAL_CONTROL;
    put_cpu_var(core_state);

    return 0;
}


static int switch_to_direct(void)
{
    DEBUG("switch from host control to direct\n");

    if (get_cpu_var(core_state).have_cpufreq) { 
        put_cpu_var(core_state);
        DEBUG("switch to direct from cpufreq\n");

        // The implementation would set the policy and governor to peg cpu
        // regardless of load
        linux_setup_palacios_governor();
    } else {
        put_cpu_var(core_state);
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
    DEBUG("switch from host control to internal\n");

    if (get_cpu_var(core_state).have_cpufreq) { 
        put_cpu_var(core_state);
        DEBUG("switch to internal on machine with cpu freq\n");
        linux_setup_palacios_governor();
    } else {
        put_cpu_var(core_state);
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
    put_cpu_var(core_state);

    DEBUG("Switching back to host control from external\n");

    if (get_cpu_var(core_state).have_cpufreq) { 
        put_cpu_var(core_state);
        linux_restore_defaults();
    } else {
        put_cpu_var(core_state);
    }

    get_cpu_var(core_state).mode = V3_PSTATE_HOST_CONTROL;
    put_cpu_var(core_state);

    return 0;
}


static int switch_from_direct(void)
{

    DEBUG("Switching back to host control from direct\n");

    // Set maximum performance, just in case there is no host control
    machine_state.funcs->set_pstate(get_cpu_var(core_state).min_pstate);
    machine_state.funcs->arch_deinit();

    if (get_cpu_var(core_state).have_cpufreq) { 
        put_cpu_var(core_state);
        linux_restore_defaults();
    } else {
        put_cpu_var(core_state);
    }

    get_cpu_var(core_state).mode=V3_PSTATE_HOST_CONTROL;

    put_cpu_var(core_state);

    return 0;
}


static int switch_from_internal(void)
{
    DEBUG("Switching back to host control from internal\n");

    if (get_cpu_var(core_state).have_cpufreq) { 
        put_cpu_var(core_state);
        linux_restore_defaults();
    } else {
        put_cpu_var(core_state);
    }

    get_cpu_var(core_state).mode=V3_PSTATE_HOST_CONTROL;

    put_cpu_var(core_state);

    return 0;
}



void palacios_pstate_ctrl_acquire(uint32_t type)
{
    if (get_cpu_var(core_state).mode != V3_PSTATE_HOST_CONTROL) { 
        put_cpu_var(core_state);
        palacios_pstate_ctrl_release();
    } else {
        put_cpu_var(core_state);
    }

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
    put_cpu_var(core_state);

    switch (get_cpu_var(core_state).mode) { 
        case V3_PSTATE_EXTERNAL_CONTROL:
            put_cpu_var(core_state);
            switch_from_external();
            break;
        case V3_PSTATE_DIRECT_CONTROL:
            put_cpu_var(core_state);
            switch_from_direct();
            break;
        case V3_PSTATE_INTERNAL_CONTROL:
            put_cpu_var(core_state);
            switch_from_internal();
            break;
        default:
            put_cpu_var(core_state);
            ERROR("Unknown pstate control type %u\n",core_state.mode);
            break;
    }
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

    for (cpu=0;cpu<numcpus;cpu++) { 
        struct pstate_core_info *s = &per_cpu(core_state,cpu);
        seq_printf(file,"pcore %u: hw pstate 0x%llx mode %s ",cpu,
                s->cur_hw_pstate,
                s->mode==V3_PSTATE_HOST_CONTROL ? "host" :
                s->mode==V3_PSTATE_EXTERNAL_CONTROL ? "external" :
                s->mode==V3_PSTATE_DIRECT_CONTROL ? "direct" : 
                s->mode==V3_PSTATE_INTERNAL_CONTROL ? "internal" : "UNKNOWN");
        if (s->mode==V3_PSTATE_EXTERNAL_CONTROL) { 
            seq_printf(file,"(min=%llu max=%llu cur=%llu) ", s->min_freq_khz, s->max_freq_khz, s->cur_freq_khz);
        } 
        if (s->mode==V3_PSTATE_DIRECT_CONTROL) { 
            seq_printf(file,"(min=%llu max=%llu cur=%llu) ",s->min_pstate, s->max_pstate, s->cur_pstate);
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

static int pstate_hw_show(struct seq_file * file, void * v)
{
    int numstates;

    seq_printf(file, "V3VEE DVFS Hardware Info\n(all logical cores assumed identical)\n\n");

    seq_printf(file, "Arch:   \t%s\n"
	             "PStates:\t%s\n\n",
            machine_state.arch==INTEL ? "Intel" : 
            machine_state.arch==AMD ? "AMD" : "Other",
            machine_state.supports_pstates ? "Yes" : "No");


#define YN(x) ((x) ? "Y" : "N")

    if (machine_state.arch==INTEL) {
	seq_printf(file,"SpeedStep:           \t%s\n",YN(machine_state.have_speedstep));
	seq_printf(file,"APERF/MPERF:         \t%s\n",YN(machine_state.have_pstate_hw_coord));
	seq_printf(file,"IDA or TurboCore:    \t%s\n",YN(machine_state.have_opportunistic));
	seq_printf(file,"Policy Hint:         \t%s\n",YN(machine_state.have_policy_hint));
	seq_printf(file,"Hardware Policy:     \t%s\n",YN(machine_state.have_hwp));
	seq_printf(file,"Hardware Duty Cycle: \t%s\n",YN(machine_state.have_hdc));
	seq_printf(file,"MWAIT extensions:    \t%s\n",YN(machine_state.have_mwait_ext));
	seq_printf(file,"MWAIT wake on intr:  \t%s\n",YN(machine_state.have_mwait_int));
    } 

    if (machine_state.arch==AMD) { 
	seq_printf(file,"PState:              \t%s\n",YN(machine_state.have_pstate));
	seq_printf(file,"APERF/MPERF:         \t%s\n",YN(machine_state.have_pstate_hw_coord));
	seq_printf(file,"CoreBoost:           \t%s\n",YN(machine_state.have_coreboost));
	seq_printf(file,"Feedback:            \t%s\n",YN(machine_state.have_feedback));
    }


    seq_printf(file,"\nPstate\tCtrl\tKHz\n");
    numstates = get_cpu_var(processors)->performance->state_count;
    if (!numstates) { 
	seq_printf(file,"UNKNOWN\n");
    } else {
	int i;
	for (i=0;i<numstates;i++) { 
	    seq_printf(file,
		       "%u\t%llx\t%llu\n",
		       i, 
		       get_cpu_var(processors)->performance->states[i].control,
		       get_cpu_var(processors)->performance->states[i].core_frequency*1000);
	}
    }
    put_cpu_var(processors);

    seq_printf(file,"\nAvailable Modes:");
    seq_printf(file," host");
    if (get_cpu_var(core_state).have_cpufreq) { 
	seq_printf(file," external");
    }
    put_cpu_var(core_state);
    if (machine_state.supports_pstates) {
	seq_printf(file," direct");
    }
    seq_printf(file," internal\n");

    return 0;
}

static int pstate_hw_open(struct inode * inode, struct file * file) 
{
    return single_open(file, pstate_hw_show, NULL);
}


static struct file_operations pstate_hw_fops = {
    .owner = THIS_MODULE,
    .open = pstate_hw_open, 
    .read = seq_read,
    .llseek = seq_lseek,
    .release = seq_release
};


int pstate_proc_setup(void)
{
    struct proc_dir_entry *proc;
    struct proc_dir_entry *prochw;

    proc = create_proc_entry("v3-dvfs",0444, palacios_get_procdir());

    if (!proc) { 
        ERROR("Failed to create proc entry for p-state control\n");
        return -1;
    }

    proc->proc_fops = &pstate_fops;

    INFO("/proc/v3vee/v3-dvfs successfully created\n");

    prochw = create_proc_entry("v3-dvfs-hw",0444,palacios_get_procdir());


    if (!prochw) { 
        ERROR("Failed to create proc entry for p-state hw info\n");
        return -1;
    }

    prochw->proc_fops = &pstate_hw_fops;

    INFO("/proc/v3vee/v3-dvfs-hw successfully created\n");

    return 0;
}

void pstate_proc_teardown(void)
{
    remove_proc_entry("v3-dvfs-hw",palacios_get_procdir());
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

    pstate_linux_init();

    INFO("P-State Control Initialized\n");

    return 0;
}

static int pstate_ctrl_deinit(void)
{
    unsigned int cpu;
    unsigned int numcpus=num_online_cpus();

    pstate_linux_deinit();

    pstate_user_teardown();

    pstate_proc_teardown();

    // release pstate control if we have it, and we need to do this on each processor
    for (cpu=0;cpu<numcpus;cpu++) { 
        palacios_xcall(cpu,(void (*)(void *))deinit_core,0);
    }


    // Free any mapping table we built for Intel
    if (intel_pstate_to_ctrl && intel_pstate_to_ctrl != intel_pstate_to_ctrl_internal) { 
	palacios_free(intel_pstate_to_ctrl);
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



