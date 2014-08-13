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
   
    // Intel-specific for DIRECT state
    uint8_t turbo_disabled;
    uint8_t no_turbo;
    
    int have_cpufreq;
    
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
    return !!(edx & (1 << 7));
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


/* Intel System Programmer's Manual Vol. 3B, 14-2 */
#define MSR_MPERF_IA32         0x000000e7
#define MSR_APERF_IA32         0x000000e8
#define MSR_MISC_ENABLE_IA32   0x000001a0
#define MSR_NHM_TURBO_RATIO_LIMIT   0x000001ad
#define MSR_PLATFORM_INFO_IA32 0x000000ce
#define MSR_PERF_CTL_IA32      0x00000199



struct turbo_mode_info_reg_intel {
    union {
        uint64_t val;
        struct {
            uint8_t  rsvd0;
            uint8_t  max_noturbo_ratio;
            uint16_t rsvd1                  : 12;
            uint8_t  ratio_limit            : 1;
            uint8_t  tdc_tdp_limit          : 1;
            uint16_t rsvd2                  : 10;
            uint8_t  min_ratio;
            uint16_t rsvd3;
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
    return !!(ecx & (1 << 7));
}


static void init_arch_intel(void)
{
    uint64_t val;

    rdmsrl(MSR_MISC_ENABLE_IA32, val);

    val |= 1 << 16;

    wrmsrl(MSR_MISC_ENABLE_IA32, val);

}

static void deinit_arch_intel(void)
{
    // ??
}

/* TODO: Intel P-states require sampling at intervals... */
static uint8_t get_pstate_intel(void)
{
    uint8_t pstate;

    // This should read the HW... 
    pstate=get_cpu_var(core_state).cur_pstate;
    put_cpu_var(core_state);
    return pstate;
}
    
static void set_pstate_intel(uint8_t p)
{
    uint64_t val = ((uint64_t)p) << 8;

    /* ...Intel IDA (dynamic acceleration)
    if (c->no_turbo && !c->turbo_disabled) {
        val |= 1 << 32;
    }
    */

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

#if 0
// The purpose of the stub governor is the pretend to keep
// the processor at the maximum frequency, while we manipulate he
// processor ccre directly
static int governor_run(struct cpufreq_policy *policy, unsigned int event)
{
    switch (event) {
	case CPUFREQ_GOV_START:
	case CPUFREQ_GOV_STOP:
	    cpu_freq_driver_target(policy, policy->max_freq);

	case CPUFREQ_GOV_LIMITS:
    }				
}

static struct cpufreq_governor stub_governor = 
{
    .name="PALACIOS_STUB",
    .governor=governor_run,
    .owner=.THIS_MODULE,
}

static void linux_init(void)
{
    // get_policy
    //
    // change to userspace governor - or change to our do nothing governor? (call set_speed)
    // stash the old governor
    // tell governor to do max freq

}

static void linux_deinit(void)
{
}

static uint8_t linux_get_pstate(void)
{
    return 0;
}

static void linux_set_pstate(uint8_t p)
{
}

static void linux_restore_defaults(void)
{
}

#endif


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


uint8_t palacios_pstate_ctrl_get_pstate(void)
{
    if (get_cpu_var(core_state).mode==V3_PSTATE_DIRECT_CONTROL) { 
	put_cpu_var(core_state);
	return machine_state.funcs->get_pstate();
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
	ERROR("Unimplemented get freq\n");
	return 0;
    } else {
	put_cpu_var(core_state);
	return 0;
    }
}

void palacios_pstate_ctrl_set_freq(uint64_t p)
{
    if (get_cpu_var(core_state).mode==V3_PSTATE_EXTERNAL_CONTROL) { 
	put_cpu_var(core_state);
	ERROR("Unimplemented set freq\n");
    } 
    put_cpu_var(core_state);

}


static void switch_to_external(void)
{
    if (!(get_cpu_var(core_state).have_cpufreq)) {
	put_cpu_var(core_state);
	ERROR("No cpufreq  - cannot switch to external...\n");
	return;
    }
    put_cpu_var(core_state);

    ERROR("Unimplemented switch to external...\n");
}
 
static void switch_to_direct(void)
{
    if (get_cpu_var(core_state).have_cpufreq) { 
	put_cpu_var(core_state);
	ERROR("Unimplemented: switch to direct on machine with cpu freq\n");
	// The implementation would set the policy and governor to peg cpu
	// regardless of load
    }

    if (machine_state.funcs && machine_state.funcs->arch_init) {
       get_cpu_var(core_state).mode=V3_PSTATE_DIRECT_CONTROL;
    
       machine_state.funcs->arch_init();

       put_cpu_var(core_state);
    }

}
    

static void switch_to_internal(void)
{
    if (get_cpu_var(core_state).have_cpufreq) { 
	put_cpu_var(core_state);
	ERROR("Unimplemented: switch to internal on machine with cpu freq\n");
	return;
	// The implementation would set the policy and governor to peg cpu
	// regardless of load - exactly like direct
    }

    get_cpu_var(core_state).mode=V3_PSTATE_INTERNAL_CONTROL;
    
    put_cpu_var(core_state);

    return;
}


static void switch_from_external(void)
{
    if (!(get_cpu_var(core_state).have_cpufreq)) {
	put_cpu_var(core_state);
	ERROR("No cpufreq  - how did we get here... external...\n");
	return;
    }

    ERROR("Unimplemented switch from external...\n");
    
    get_cpu_var(core_state).mode = V3_PSTATE_HOST_CONTROL;

    put_cpu_var(core_state);

}
 
static void switch_from_direct(void)
{
     
    if (get_cpu_var(core_state).have_cpufreq) { 
	put_cpu_var(core_state);
	ERROR("Unimplemented: switch from direct on machine with cpu freq - will just pretend to do so\n");
	// The implementation would switch back to default policy and governor
    }

    get_cpu_var(core_state).mode=V3_PSTATE_HOST_CONTROL;


    machine_state.funcs->set_pstate(get_cpu_var(core_state).min_pstate);

    machine_state.funcs->arch_deinit();

    put_cpu_var(core_state);
}
    

static void switch_from_internal(void)
{
    if (get_cpu_var(core_state).have_cpufreq) { 
	put_cpu_var(core_state);
	ERROR("Unimplemented: switch from internal on machine with cpu freq - will just pretend to do so\n");
	// The implementation would switch back to default policy and governor
    }

    get_cpu_var(core_state).mode=V3_PSTATE_HOST_CONTROL;

    put_cpu_var(core_state);
    
    return;
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
	    seq_printf(file," external ");
	}
	if (machine_state.arch==AMD || machine_state.arch==INTEL) { 
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
		    palacios_xcall(r.pcore,(void (*)(void*))palacios_pstate_ctrl_acquire_external,0);
		    return 0;
		    break;
		case V3_DVFS_DIRECT:
		    palacios_xcall(r.pcore,(void (*)(void*))palacios_pstate_ctrl_acquire_direct,0);
		    return 0;
		    break;
		default:
		    ERROR("Unknown DVFS acquire type %u\n",r.acq_type);
		    return -EFAULT;
	    }
	}
	    break;
	case V3_DVFS_RELEASE: {
	    palacios_xcall(r.pcore,(void (*)(void*))palacios_pstate_ctrl_release,0);
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

    INFO("P-State Control Initialized\n");

    return 0;
}

static int pstate_ctrl_deinit(void)
{
    unsigned int cpu;
    unsigned int numcpus=num_online_cpus();


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



