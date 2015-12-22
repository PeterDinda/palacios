#ifndef __PALACIOS_PWRSTAT_H__
#define __PALACIOS_PWRSTAT_H__

/* THESE ARE INTEL SPECIFIC (Sandy Bridge and up) */

#define SANDY_BRIDGE_E3_MODEL_NO 0x2A
#define SANDY_BRIDGE_E5_MODEL_NO 0x2D
#define IVY_BRIDGE_MODEL_NO 0x3A
/* WARNING WARNING: this is speculation... */
#define HASWELL_MODEL_NO 0x4A

#ifdef MSR_RAPL_POWER_UNIT
// assume the rest are also defined by the kernel's msr include
// except for special ones here

#else
// assume none are defined by the kernel's msr include
#define MSR_RAPL_POWER_UNIT		0x606
#define MSR_PKG_POWER_LIMIT	        0x610
#define MSR_PKG_ENERGY_STATUS		0x611
#define MSR_PKG_PERF_STATUS		0x613
#define MSR_PKG_POWER_INFO		0x614

/* PP0 RAPL Domain */
#define MSR_PP0_POWER_LIMIT		0x638
#define MSR_PP0_ENERGY_STATUS		0x639
#define MSR_PP0_POLICY			0x63A
#define MSR_PP0_PERF_STATUS		0x63B

/* PP1 RAPL Domain, may reflect to uncore devices */
#define MSR_PP1_POWER_LIMIT		0x640
#define MSR_PP1_ENERGY_STATUS		0x641
#define MSR_PP1_POLICY			0x642

/* DRAM RAPL Domain */
#define MSR_DRAM_POWER_LIMIT		0x618
#define MSR_DRAM_ENERGY_STATUS		0x619
#define MSR_DRAM_PERF_STATUS		0x61B
#define MSR_DRAM_POWER_INFO		0x61C

#endif

/* RAPL UNIT BITMASK */
#define POWER_UNIT_OFFSET	0
#define POWER_UNIT_MASK		0x0F

#define ENERGY_UNIT_OFFSET	0x08
#define ENERGY_UNIT_MASK	0x1F00

#define TIME_UNIT_OFFSET	0x10
#define TIME_UNIT_MASK		0xF0000


#endif
