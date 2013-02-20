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
 * Author: Chang S. Bae  <chang.bae@eecs.northwestern.edu>
 * 
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#include <palacios/vmm.h>
#include <palacios/vmm_debug.h>
#include <palacios/vmm_types.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_lowlevel.h>
#include <interfaces/vmm_pmu.h>

struct v3_pmu_iface * palacios_pmu = 0;

/*
 * functions
 */

/*
 * description: init pmu through the interface on this core
 */

void v3_pmu_init(void) {

    V3_ASSERT(VM_NONE, VCORE_NONE, palacios_pmu != NULL);
    V3_ASSERT(VM_NONE, VCORE_NONE, palacios_pmu->init != NULL);

    palacios_pmu->init();
}

/*
 * description: actually stop pmu running through the interface
 *              on this core
 */
void v3_pmu_deinit(void) {

    V3_ASSERT(VM_NONE, VCORE_NONE, palacios_pmu != NULL);
    V3_ASSERT(VM_NONE, VCORE_NONE, palacios_pmu->deinit != NULL);

    palacios_pmu->deinit();
}

/*
 * description: init pmu through the interface
 */

int v3_pmu_start_tracking(v3_pmon_ctr_t ctr) {

    V3_ASSERT(VM_NONE, VCORE_NONE, palacios_pmu != NULL);
    V3_ASSERT(VM_NONE, VCORE_NONE, palacios_pmu->start_tracking != NULL);

    return palacios_pmu->start_tracking(ctr);
}

/*
 * description: actually stop pmu running through the interface
 */
int v3_pmu_stop_tracking(v3_pmon_ctr_t ctr) {

    V3_ASSERT(VM_NONE, VCORE_NONE, palacios_pmu != NULL);
    V3_ASSERT(VM_NONE, VCORE_NONE, palacios_pmu->stop_tracking != NULL);

    return palacios_pmu->stop_tracking(ctr);
}

/*
 * description: actually stop pmu running through the interface
 */
uint64_t v3_pmu_get_value(v3_pmon_ctr_t ctr) {

    V3_ASSERT(VM_NONE, VCORE_NONE, palacios_pmu != NULL);
    V3_ASSERT(VM_NONE, VCORE_NONE, palacios_pmu->get_value != NULL);

    return palacios_pmu->get_value(ctr);
}


/*
 * description: init whole interface to pmu 
 */

void V3_Init_PMU(struct v3_pmu_iface * pmu_iface) {
    palacios_pmu = pmu_iface;

    return;
}


