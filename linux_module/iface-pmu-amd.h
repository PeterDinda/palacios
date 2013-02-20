// Utility functions for AMD

/*
 * defines
 */

/*
 * should be changed
 *
 *
 */

#define AMD_NUM_PMU_COUNTERS 4
#define AMD_NUM_PMU_CONTROLS 4


/*
 * MSR OFFSETS FOR PMU RELATED:
 */

/*
 * MSR_K7_EVNTSEL0-3 is in msr-index.h
 *
 * MSR_K7_PERFCTR0-3 is also in msr-index.h
 */

/*
 * bit info according to AMD manual
 performance monitoring counters: core performance event-select registers
 *
 */
#define AMD_USR_BIT 16
#define AMD_OS_BIT 17
#define AMD_EDGE_BIT 18
#define AMD_INT_BIT 20
#define AMD_EN_BIT 22
#define AMD_INV_BIT 23
#define AMD_CMASK_BIT 24
#define AMD_UMASK_BIT 8
#define AMD_EVENT_BIT 0

/*
 * SOME MACROS
 */

#define AMD_CTR_READ(msrs, c) do {rdmsrl((MSR_K7_PERFCTR0 + (c)), (msrs).q);} while (0)
#define AMD_CTR_WRITE(msrs, c) do {wrmsrl((MSR_K7_PERFCTR0 + (c)), (msrs).q);} while (0)

#define AMD_CTRL_READ(msrs, c) do {rdmsrl((MSR_K7_EVNTSEL0 + (c)), (msrs).q);} while (0)
#define AMD_CTRL_WRITE(msrs, c) \
({ \
	(msrs).q |= 0x1<<21; \
	wrmsrl((MSR_K7_EVNTSEL0 + (c)), (msrs).q); \
})

// given even and mask, make it to track it on all ring levels
#define AMD_CTRL_START(event, mask, i) \
({  \
	uint64_t tmp = 0x0; \
	tmp |= (mask)<<AMD_UMASK_BIT; \
	tmp |= (event)<<AMD_EVENT_BIT; \
	tmp |= 0x3<<AMD_USR_BIT; \
	tmp |= 0x1<<AMD_EN_BIT; \
	wrmsrl((MSR_K7_EVNTSEL0 + (i)), tmp); \
    	wrmsrl((MSR_K7_PERFCTR0 + (i)), 0x0); \
})

#define AMD_CTRL_STOP(i) do { wrmsrl((MSR_K7_EVNTSEL0 + (i)), 0x0); } while(0) \

/*
 * SELECTED PMU EVENTS AND UMASKS
 * BIOS and Kernel Developer's Guid (BKDG) For AMD Family 11h Processors, July 2008
 * 3.14 Performance Counter Events
 *
 */

// CLK and INSTRUCTIONS events
#define AMD_CLK_NOT_HALTED 0x76 // event
#define AMD_RETIRED_INSTRUCTIONS 0xc0 // event

// MEM INST events
#define AMD_PREFETCH_INST_DISPATCHED 0x4B// event
#define AMD_LOAD 0x1 // umask
#define AMD_STORE 0x2 // umask

// MEM LOAD events and umasks
#define AMD_DATA_CACHE_ACCESSES 0x40 // event
#define AMD_DATA_CACHE_MISSES 0x41 // event
#define AMD_L1_DTLB_AND_L2_DTLB_MISS 0x46 // event

// LLC MISSES events: per intel doc, it is advised to use this for perf measures
//#define INTEL_LLC_MISSES 0x2E
//#define INTEL_LLC_MISSES_UMASK 0x41
