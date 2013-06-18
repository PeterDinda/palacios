/*
 *
 * Intel RAPL (Sandy Bridge and above) Accessor
 * (c) Kyle C. Hale, Chang Bae 2013
 *
 *
 */
#include <asm/msr.h>
#include <asm/msr-index.h>
#include <linux/math64.h>

#include <palacios/vmm_types.h>
#include <palacios/vmm_util.h>

#include <interfaces/vmm_pwrstat.h>

#include "vm.h"
#include "palacios.h"
#include "util-queue.h"
#include "linux-exts.h"
#include "iface-pwrstat.h"


enum rapl_domain_id {
	RAPL_DOMAIN_PKG,
	RAPL_DOMAIN_PP0,
	RAPL_DOMAIN_PP1,
	RAPL_DOMAIN_DRAM,
	RAPL_DOMAIN_MAX
};


struct rapl_domain_msr {
	int	limit;
	int	status;
};


struct rapl_domain {
	enum rapl_domain_id domain_id;
	struct rapl_domain_msr msrs;
	int valid;
};


static struct rapl_domain rapl_domains[] = {
	[RAPL_DOMAIN_PKG] = {
		.domain_id = RAPL_DOMAIN_PKG,
		.msrs	= {
			.limit	= MSR_PKG_RAPL_POWER_LIMIT,
			.status	= MSR_PKG_ENERGY_STATUS,
		},
		.valid	= 1,
	},
	[RAPL_DOMAIN_PP0] = {
		.domain_id = RAPL_DOMAIN_PP0,
		.msrs	= {
			.limit	= MSR_PP0_POWER_LIMIT,
			.status	= MSR_PP0_ENERGY_STATUS,
		},
		.valid	= 1,
	},
	[RAPL_DOMAIN_PP1] = {
		.domain_id = RAPL_DOMAIN_PP1,
		.msrs	= {
			.limit	= MSR_PP1_POWER_LIMIT,
			.status	= MSR_PP1_ENERGY_STATUS,
		},
	},
	[RAPL_DOMAIN_DRAM] = {
		.domain_id = RAPL_DOMAIN_DRAM,
		.msrs	= {
			.limit	= MSR_DRAM_POWER_LIMIT,
			.status	= MSR_DRAM_ENERGY_STATUS,
		},
	},
};

static unsigned int power_unit_divisor;
static unsigned int energy_unit_divisor;
static unsigned int time_unit_divisor;

enum unit_type {
	POWER_UNIT,
	ENERGY_UNIT,
	TIME_UNIT
};

static inline void cpuid_string(u32 id, u32 dest[4]) {
	asm volatile("cpuid"
		   :"=a"(*dest),"=b"(*(dest+1)),"=c"(*(dest+2)),"=d"(*(dest+3))
		   :"a"(id));
}


static int rapl_check_unit (void)
{
	u64 output;
	u32 value;

	rdmsrl(MSR_RAPL_POWER_UNIT, output);

	/* energy unit: 1/enery_unit_divisor Joules */
	value = (output & ENERGY_UNIT_MASK) >> ENERGY_UNIT_OFFSET;
	energy_unit_divisor = 1 << value;

	/* power unit: 1/power_unit_divisor Watts */
	value = (output & POWER_UNIT_MASK) >> POWER_UNIT_OFFSET;
	power_unit_divisor = 1 << value;

	/* time unit: 1/time_unit_divisor Seconds */
	value =(output & TIME_UNIT_MASK) >> TIME_UNIT_OFFSET;
	time_unit_divisor = 1 << value;

	return 0;
}


static u64 rapl_unit_xlate(enum unit_type type, u64 value, int action)
{
	u64 divisor;

	switch (type) {
	case POWER_UNIT:
		divisor = power_unit_divisor;
		break;
	case ENERGY_UNIT:
		divisor = energy_unit_divisor;
		break;
	case TIME_UNIT:
		divisor = time_unit_divisor;
		break;
	default:
		return 0;
	};

	if (action)
		return value * divisor; /* value is from users */
	else
		return div64_u64(value, divisor); /* value is from MSR */
}


static int rapl_read_energy(struct rapl_domain * domain)
{
	u64 value;
	u32 msr = domain->msrs.status;

	rdmsrl(msr, value);
	return rapl_unit_xlate(ENERGY_UNIT, value, 0);
}


static int rapl_ctr_valid (v3_pwrstat_ctr_t ctr) 
{
	switch (ctr) {
		case V3_PWRSTAT_PKG_ENERGY:
			return rapl_domains[RAPL_DOMAIN_PKG].valid;
		case V3_PWRSTAT_CORE_ENERGY:
			return rapl_domains[RAPL_DOMAIN_PP0].valid;
		case V3_PWRSTAT_EXT_ENERGY:
			return rapl_domains[RAPL_DOMAIN_PP1].valid;
		case V3_PWRSTAT_DRAM_ENERGY:
			return rapl_domains[RAPL_DOMAIN_DRAM].valid;
		default:
			return 0;
	}
	
	return 0;
}


static uint64_t rapl_get_value (v3_pwrstat_ctr_t ctr)
{
	switch (ctr) {
		case V3_PWRSTAT_PKG_ENERGY:
			return rapl_read_energy(&rapl_domains[RAPL_DOMAIN_PKG]);
		case V3_PWRSTAT_CORE_ENERGY:
			return rapl_read_energy(&rapl_domains[RAPL_DOMAIN_PP0]);
		case V3_PWRSTAT_EXT_ENERGY:
			return rapl_read_energy(&rapl_domains[RAPL_DOMAIN_PP1]);
		case V3_PWRSTAT_DRAM_ENERGY:
			return rapl_read_energy(&rapl_domains[RAPL_DOMAIN_DRAM]);
		default:
			ERROR("Invalid Powerstat Counter\n");
			return -1;
	}
	
	return -1;
}


static int get_cpu_vendor (char name[13])
{
	u32 dest[4];
	u32 maxid;

	cpuid_string(0,dest);
	maxid=dest[0];
	((u32*)name)[0]=dest[1];
	((u32*)name)[1]=dest[3];
	((u32*)name)[2]=dest[2];
	name[12]=0;

	return maxid;
}


static int get_cpu_model (void) 
{
	u32 dest[4] = {0, 0, 0, 0};
	u32 ex_model, model;
	cpuid_string(1, dest);
	ex_model = (dest[0] & 0xF0000U) >> 16;
	model = (dest[0] & 0xF0U) >> 4;
    return (model + (ex_model << 4));
}


static int supports_rapl (void) 
{
	unsigned int model = get_cpu_model();
	switch (model) {
		case SANDY_BRIDGE_E3_MODEL_NO:
			return 1;
		case SANDY_BRIDGE_E5_MODEL_NO:
			return 1;
		case IVY_BRIDGE_MODEL_NO:
			return 1;
		case HASWELL_MODEL_NO:
			return 1;
		default:
			break;
	}

	return 0;
}


static int is_intel(void)
{
	char name[13];
	get_cpu_vendor(name);
	return !strcmp(name,"GenuineIntel");
}


static int rapl_init (void) 
{
	if (supports_rapl()) {
		INFO("Intel RAPL featureset detected\n");
	} else {
		ERROR("This machine does not support Intel RAPL functionality\n");
		return -1;
	}

	if (get_cpu_model() == SANDY_BRIDGE_E3_MODEL_NO) {
		INFO("Sandy Bridge E3, supports PP1 (no DRAM)\n");
		rapl_domains[RAPL_DOMAIN_PP1].valid = 1;
	} else if (get_cpu_model() == SANDY_BRIDGE_E5_MODEL_NO) {
		INFO("Sandy Bridge E5, supports DRAM domain\n");
		rapl_domains[RAPL_DOMAIN_DRAM].valid = 1;
	}

	if (rapl_check_unit()) {
		return -1;
	}

	return 0;
}


static int rapl_deinit (void) 
{
	INFO("Intel RAPL deinit\n");
	return 0;
}


static struct v3_pwrstat_iface intel_rapl_pwrstat = {
	.init      = rapl_init,
	.deinit    = rapl_deinit,
	.ctr_valid = rapl_ctr_valid,
	.get_value = rapl_get_value,
};


static int pwrstat_init (void)
{
	if (is_intel()) {
	    if  (supports_rapl()) { 
		INFO("Palacios Power stats using Intel RAPL\n");
		V3_Init_Pwrstat(&intel_rapl_pwrstat);
	    } else {
		INFO("Intel machine, but no RAPL functionality, so no power statistics available\n");
		return -1;
	    }
	} else {
		ERROR("Not an Intel Machine, no power statistics available\n");
		return -1;
	}
	
	return 0;
}

/* if AMD comes up with something it can go here */

static struct linux_ext pwrstat_ext = {
  .name = "POWERSTAT",
  .init = pwrstat_init,
  .deinit = NULL,
  .guest_init = NULL,
  .guest_deinit = NULL
};

register_extension(&pwrstat_ext);
