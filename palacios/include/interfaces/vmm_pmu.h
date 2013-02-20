/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National
 * Science Foundation and the Department of Energy.
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at
 * http://www.v3vee.org
 *
 * Copyright (c) 2013, The V3VEE Project <http://www.v3vee.org>
 * All rights reserved.
 *
 * Author: Chang S. Bae <chang.bae@eecs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __VMM_PMU
#define __VMM_PMU

#include <palacios/vmm_types.h>

/*
 * defines
 */

/*
 * The following is the set of performance
 * counters available in Palacios.  The implementation
 * of the interface translates these names to
 * the underlying counters needed on the specific platform
 */
typedef enum {
  V3_PMON_RETIRED_INST_COUNT=0,
  V3_PMON_CLOCK_COUNT,
  V3_PMON_MEM_LOAD_COUNT,
  V3_PMON_MEM_STORE_COUNT,
  V3_PMON_CACHE_MISS_COUNT,
  V3_PMON_TLB_MISS_COUNT

  //V3_PMON_TLB_INVALIDATION_COUNT
  /* you can add more here, but then need to implement them
     in the interface */

} v3_pmon_ctr_t;


struct v3_pmu_iface {
    /* init/deinit pmu data on the core on which it is called */


    void  (*init)(void);
    void  (*deinit)(void);

    // Request tracking of a counter, returns -1 if it is not possible
    int (*start_tracking)(v3_pmon_ctr_t ctr);
    // Get the counter value, provided it being tracked
    uint64_t (*get_value)(v3_pmon_ctr_t ctr);
    // Stop tracking a counter
    int (*stop_tracking)(v3_pmon_ctr_t ctr);

};


/*
 *  function prototypes
 */

extern void V3_Init_PMU(struct v3_pmu_iface * palacios_pmu);

#ifdef __V3VEE__

/* This is a PER PHYSICAL CORE init/deinit */
void v3_pmu_init(void);

// Call these after an init
int v3_pmu_start_tracking(v3_pmon_ctr_t ctr);
int v3_pmu_stop_tracking(v3_pmon_ctr_t ctr);
uint64_t v3_pmu_get_value(v3_pmon_ctr_t ctr);

// Call this after you are done with the PMU
void v3_pmu_deinit(void);



#endif

#endif
