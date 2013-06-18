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
 * 		   Chang S. Bae <chang.bae@eecs.northwestern.edu>
 *         Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */
#include <palacios/vm_guest.h>
#include <palacios/vmm_telemetry.h>
#include <palacios/vmm_pwrstat_telemetry.h>
#include <palacios/vmm_sprintf.h>
#include <interfaces/vmm_pwrstat.h>

/*
  We will try to track:

	V3_PWRSTAT_PKG_ENERGY,
	V3_PWRSTAT_CORE_ENERGY,
	V3_PWRSTAT_EXT_ENERGY, 
	V3_PWRSTAT_DRAM_ENERGY,
*/

#define HAVE(WHAT) (info->pwrstat_telem.active_counters[WHAT])

#define GUEST(WHAT)  do { if (HAVE(WHAT)) { V3_Print(info->vm_info, info, "%sGUEST:%u:%u:%s = %llu\n", hdr, info->vcpu_id, info->pcpu_id, #WHAT, info->pwrstat_telem.guest_counts[WHAT]); } } while (0)
#define HOST(WHAT)   do { if (HAVE(WHAT)) { V3_Print(info->vm_info, info, "%sHOST:%u:%u:%s = %llu\n", hdr, info->vcpu_id, info->pcpu_id, #WHAT, info->pwrstat_telem.host_counts[WHAT]); } } while (0)

#define START(WHAT) \
do { \
  if(v3_pwrstat_ctr_valid(WHAT)) { \
    info->pwrstat_telem.active_counters[WHAT]=1; \
  } else { \
  info->pwrstat_telem.active_counters[WHAT]=0;\
  } \
 } while (0) 

#define STOP(WHAT) info->pwrstat_telem.active_counters[WHAT]=0;

static int print_pwrstat_data(struct guest_info *info, char * hdr) 
{
  GUEST(V3_PWRSTAT_PKG_ENERGY);
  GUEST(V3_PWRSTAT_CORE_ENERGY);
  GUEST(V3_PWRSTAT_EXT_ENERGY);
  GUEST(V3_PWRSTAT_DRAM_ENERGY);

  HOST(V3_PWRSTAT_PKG_ENERGY);
  HOST(V3_PWRSTAT_CORE_ENERGY);
  HOST(V3_PWRSTAT_EXT_ENERGY);
  HOST(V3_PWRSTAT_DRAM_ENERGY);

  return 0;
}

  
static void telemetry_pwrstat (struct v3_vm_info * vm, void * private_data, char * hdr) 
{
  int i;
  struct guest_info *core = NULL;
  
  /*
   * work through each pcore (vcore for now per excluding oversubscription) and gathering info
   */
  for(i=0; i<vm->num_cores; i++) {
    core = &(vm->cores[i]);
    if((core->core_run_state != CORE_RUNNING)) continue;
    print_pwrstat_data(core, hdr);
  }
}


void v3_pwrstat_telemetry_start (struct guest_info *info)
{
  if (!info->vm_info->enable_telemetry) {
    return;
  }

  memset(&(info->pwrstat_telem),0,sizeof(struct v3_core_pwrstat_telemetry));

  v3_pwrstat_init();
  
  START(V3_PWRSTAT_PKG_ENERGY);
  START(V3_PWRSTAT_CORE_ENERGY);
  START(V3_PWRSTAT_EXT_ENERGY);
  START(V3_PWRSTAT_DRAM_ENERGY);

  info->pwrstat_telem.state=PWR_AWAIT_FIRST_ENTRY;

  if (info->vcpu_id==0) { 
    v3_add_telemetry_cb(info->vm_info, telemetry_pwrstat, NULL);
  }
}


static void inline snapshot(uint64_t vals[]) {
  vals[V3_PWRSTAT_PKG_ENERGY] = v3_pwrstat_get_value(V3_PWRSTAT_PKG_ENERGY);
  vals[V3_PWRSTAT_CORE_ENERGY] = v3_pwrstat_get_value(V3_PWRSTAT_CORE_ENERGY);
  vals[V3_PWRSTAT_EXT_ENERGY] = v3_pwrstat_get_value(V3_PWRSTAT_EXT_ENERGY);
  vals[V3_PWRSTAT_DRAM_ENERGY] = v3_pwrstat_get_value(V3_PWRSTAT_DRAM_ENERGY);
}  


void v3_pwrstat_telemetry_enter(struct guest_info *info)
{
  if (!info->vm_info->enable_telemetry) {
    return;
  }

  switch (info->pwrstat_telem.state) { 
  case PWR_AWAIT_FIRST_ENTRY:
    snapshot(info->pwrstat_telem.last_snapshot);
    info->pwrstat_telem.state=PWR_AWAIT_EXIT;
    break;
  
  case PWR_AWAIT_ENTRY: {
    // AWAIT_ENTRY - the snapshot in the struct is from the last exit
    uint64_t snap[PWRSTAT_NUM_COUNTERS];
    int i;

    snapshot(snap);

    for (i=0;i<PWRSTAT_NUM_COUNTERS;i++) { 
      info->pwrstat_telem.host_counts[i] += snap[i] - info->pwrstat_telem.last_snapshot[i];
    }

    for (i=0;i<PWRSTAT_NUM_COUNTERS;i++) { 
      info->pwrstat_telem.last_snapshot[i] = snap[i];
    }

    info->pwrstat_telem.state = PWR_AWAIT_EXIT;
  }
    break;

  default:
    PrintError(info->vm_info, info, "Impossible state on pwrstat telemetry entry\n");
    break;
  }
  
}


void v3_pwrstat_telemetry_exit (struct guest_info *info)
{
  if (!info->vm_info->enable_telemetry) {
    return;
  }

  switch (info->pwrstat_telem.state) { 
  case PWR_AWAIT_EXIT: {
    // AWAIT_EXIT - the snapshot in the struct is from the last entryx
    uint64_t snap[PWRSTAT_NUM_COUNTERS];
    int i;

    snapshot(snap);

    for (i=0;i<PWRSTAT_NUM_COUNTERS;i++) { 
      info->pwrstat_telem.guest_counts[i] += snap[i] - info->pwrstat_telem.last_snapshot[i];
    }

    for (i=0;i<PWRSTAT_NUM_COUNTERS;i++) { 
      info->pwrstat_telem.last_snapshot[i] = snap[i];
    }

    info->pwrstat_telem.state = PWR_AWAIT_ENTRY;
  }
    break;
  default:
    PrintError(info->vm_info, info, "Impossible state on pwrstat telemetry exit\n");
    break;
  }
  
}


void v3_pwrstat_telemetry_end (struct guest_info *info)
{
  if (!info->vm_info->enable_telemetry) {
    return;
  }

  v3_pwrstat_deinit();
  
  info->pwrstat_telem.state=PWR_AWAIT_FIRST_ENTRY;

}
