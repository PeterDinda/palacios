// Intel Utility Functions

/*
 * defines
 */

#define INTEL_IDX_INST_IN_FPMU 0
#define INTEL_IDX_CLK_IN_FPMU 2

/*
 * REFERENCE CPU MODEL:
 *   Intel Xeon E5620  (family_cpuid: 06_1CH)
 *   Architectural Performance Monitoring Version: 3
 *   Number of general-purpose performance counters: 4
 *
 */

#define INTEL_NUM_PMU_COUNTERS 4
#define INTEL_NUM_PMU_CONTROLS 4
#define INTEL_NUM_FIXED_PMU_COUNTERS 3
#define INTEL_NUM_PMU_GLOBAL 3

/*
 * NOTICE currently there are 7 counters in a total (IA32_PMC0-3 + IA32_FIXED_CTR0-2)
 * but, only 4 of those are being used if you will
 * since hard-programmed now for CPI and LLCache Miss Rate are only events that are requested
 *
 * define NUM_USED_COUNTERS 4 at vmm_pmu.h
 */

/*
 * MSR OFFSETS FOR PMU RELATED:
 */

#define INTEL_IA32_PMC0 0xc1
#define INTEL_IA32_PMC1 0xc2
#define INTEL_IA32_PMC2 0xc3
#define INTEL_IA32_PMC3 0xc4

#define INTEL_IA32_PERFEVTSEL0 0x186
#define INTEL_IA32_PERFEVTSEL1 0x187
#define INTEL_IA32_PERFEVTSEL2 0x188
#define INTEL_IA32_PERFEVTSEL3 0x189

/*
 * 0x309 INTEL_IA32_FIXED_CTR0: counts Instr_Retired.Any
 * 0x30A INTEL_IA32_FIXED_CTR1: counts CPU_CLK_Unhalted.Core
 * 0x30B INTEL_IA32_FIXED_CTR2: counts CPU_CLK_Unhalted.Ref
 */
#define INTEL_IA32_FIXED_CTR0 0x309
#define INTEL_IA32_FIXED_CTR1 0x30a
#define INTEL_IA32_FIXED_CTR2 0x30b

#define INTEL_IA32_FIXED_CTR_CTRL 0x38d

#define INTEL_IA32_PERF_GLOBAL_STATUS 0x38e
#define INTEL_IA32_PERF_GLOBAL_CTRL 0x38f
#define INTEL_IA32_PERF_GLOBAL_OVF_CTRL 0x390

#define INTEL_IA32_PERF_GLOBAL_STATUS_ORDER 0
#define INTEL_IA32_PERF_GLOBAL_CTRL_ORDER 1
#define INTEL_IA32_PERF_GLOBAL_OVF_CTRL_ORDER 2

// bit for relevant configs for PEREVTSEL (perf event selection)
#define INTEL_USR_BIT 16
#define INTEL_OS_BIT 17
#define INTEL_EDGE_BIT 18
#define INTEL_PIN_BIT 19
#define INT_BIT 20
#define INTEL_ANY_BIT 21
#define INTEL_EN_BIT 22
#define INTEL_INV_BIT 23
#define INTEL_CMASK_BIT 24
#define INTEL_UMASK_BIT 8
#define INTEL_EVENT_BIT 0



/*
 * SOME MACROS
 */

#define INTEL_MSR_OFFSET_PERF(val) (val & 0x3)

#define INTEL_CTR_READ(msrs, c) do {rdmsrl((INTEL_IA32_PMC0 + (c)), (msrs).q);} while (0)
#define INTEL_CTR_WRITE(msrs, c) do {wrmsrl((INTEL_IA32_PMC0 + (c)), (msrs).q);} while (0)

#define INTEL_FIXED_CTR_READ(msrs, c) do {rdmsrl((INTEL_IA32_FIXED_CTR0 + (c)), (msrs).q);} while (0)
#define INTEL_FIXED_CTR_WRITE(msrs, c) do {wrmsrl((INTEL_IA32_FIXED_CTR0 + (c)), (msrs).q);} while (0)

#define INTEL_CTRL_READ(msrs, c) do {rdmsrl((INTEL_IA32_PERFEVTSEL0 + (c)), (msrs).q);} while (0)
#define INTEL_CTRL_WRITE(msrs, c) do {wrmsrl((INTEL_IA32_PERFEVTSEL0 + (c)), (msrs).q);} while (0)

// given even and mask, make it to track it on all ring levels
#define INTEL_CTRL_START(event, mask, i) \
({  \
	uint64_t tmp = 0x0; \
	tmp |= (mask)<<INTEL_UMASK_BIT; \
	tmp |= (event)<<INTEL_EVENT_BIT; \
	tmp |= 0x3<<INTEL_USR_BIT; \
	tmp |= 0x1<<INTEL_EN_BIT; \
	wrmsrl((INTEL_IA32_PERFEVTSEL0 + (i)), tmp); \
        wrmsrl((INTEL_IA32_PMC0 + (i)), 0x0); \
})

#define INTEL_CTRL_STOP(i) do { wrmsrl((INTEL_IA32_PERFEVTSEL0 + (i)), 0x0); } while(0) \

#define INTEL_FIXED_CTRL_READ(msrs) do {rdmsrl(INTEL_IA32_FIXED_CTR_CTRL, (msrs).q);} while (0)
#define INTEL_FIXED_CTRL_WRITE(msrs) do {wrmsrl(INTEL_IA32_FIXED_CTR_CTRL, (msrs).q);} while (0)

/*
 * SELECTED PMU EVENTS AND UMASKS
 * Intel 64 and IA-32 Arthitectures Software Developer's Manual, Jan 2013
 * Chap 19 Performance-Monitoring Events
 */

// CLK and INSTRUCTIONS events
#define INTEL_CLK_NOT_HALTED 0x3C // event
#define INTEL_RETIRED_INSTRUCTIONS 0xC0 // event

// MEM INST events
#define INTEL_MEM_INST_RETIRED 0x0B // event
	#define INTEL_LOADS 0x1 // umask
	#define INTEL_STORES 0x2 // umask

// MEM LOAD events and umasks
#define INTEL_MEM_LOAD_RETIRED 0xCB // event
	#define INTEL_L1D_HIT 0x1 // umask
	#define INTEL_L2_HIT 0x2 // umask
	#define INTEL_L3_UNSHARED_HIT 0x4 // umask
	#define INTEL_OTHER_CORE_L2_HIT_HITM 0x8 // umask
	#define INTEL_L3_MISS 0x10 // umask
	#define INTEL_HIT_LFB 0x40 // umask
	#define INTEL_DTLB_MISS 0x80 // umask



