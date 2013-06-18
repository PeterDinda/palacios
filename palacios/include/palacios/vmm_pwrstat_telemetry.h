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
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *         Chang S. Bae <chang.bae@eecs.northwestern.edu>
 *         Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __VMM_PWRSTAT_TELEMETRY_H__
#define __VMM_PWRSTAT_TELEMETRY_H__

#ifdef __V3VEE__

#ifdef V3_CONFIG_PWRSTAT_TELEMETRY

#include <interfaces/vmm_pwrstat.h>
#include <palacios/vmm_list.h>

struct guest_info;

#define PWRSTAT_NUM_COUNTERS 4

struct v3_core_pwrstat_telemetry {
  enum {PWR_AWAIT_FIRST_ENTRY=0, PWR_AWAIT_ENTRY, PWR_AWAIT_EXIT} state;
  uint8_t       active_counters[PWRSTAT_NUM_COUNTERS];
  uint64_t      guest_counts[PWRSTAT_NUM_COUNTERS];
  uint64_t      host_counts[PWRSTAT_NUM_COUNTERS];
  uint64_t      last_snapshot[PWRSTAT_NUM_COUNTERS];
};


void v3_pwrstat_telemetry_start(struct guest_info *info);
void v3_pwrstat_telemetry_enter(struct guest_info *info);
void v3_pwrstat_telemetry_exit(struct guest_info *info);
void v3_pwrstat_telemetry_end(struct guest_info *info);


#endif
#endif
#endif
