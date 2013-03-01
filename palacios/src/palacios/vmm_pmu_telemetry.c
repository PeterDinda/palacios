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
 *         Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */
#include <palacios/vm_guest.h>
#include <palacios/vmm_telemetry.h>
#include <palacios/vmm_pmu_telemetry.h>
#include <palacios/vmm_sprintf.h>

/*
  We will try to track:

  V3_PMON_RETIRED_INST_COUNT,
  V3_PMON_CLOCK_COUNT,
  V3_PMON_MEM_LOAD_COUNT,
  V3_PMON_MEM_STORE_COUNT,
  V3_PMON_CACHE_MISS_COUNT,
  V3_PMON_TLB_MISS_COUNT

  and to derive:

  CPI
  cache Misses per instruction
*/

#define HAVE(WHAT) (info->pmu_telem.active_counters[WHAT])

#define GUEST(WHAT)  do { if (HAVE(WHAT)) { V3_Print(info->vm_info, info, "%sGUEST:%u:%u:%s = %llu\n", hdr, info->vcpu_id, info->pcpu_id, #WHAT, info->pmu_telem.guest_counts[WHAT]); } } while (0)
#define HOST(WHAT)   do { if (HAVE(WHAT)) { V3_Print(info->vm_info, info, "%sHOST:%u:%u:%s = %llu\n", hdr, info->vcpu_id, info->pcpu_id, #WHAT, info->pmu_telem.host_counts[WHAT]); } } while (0)


static int print_pmu_data(struct guest_info *info, char * hdr) 
{
  GUEST(V3_PMON_RETIRED_INST_COUNT);
  GUEST(V3_PMON_CLOCK_COUNT);
  GUEST(V3_PMON_MEM_LOAD_COUNT);
  GUEST(V3_PMON_MEM_STORE_COUNT);
  GUEST(V3_PMON_CACHE_MISS_COUNT);
  GUEST(V3_PMON_TLB_MISS_COUNT);
  V3_Print(info->vm_info, info, "%sGUEST:%u:%u:%s = %llu\n", hdr, info->vcpu_id, info->pcpu_id, "UCPI",info->pmu_telem.guest_ucpi_estimate);
  V3_Print(info->vm_info, info, "%sGUEST:%u:%u:%s = %llu\n", hdr, info->vcpu_id, info->pcpu_id, "UMPL", info->pmu_telem.guest_umpl_estimate);

  HOST(V3_PMON_RETIRED_INST_COUNT);
  HOST(V3_PMON_CLOCK_COUNT);
  HOST(V3_PMON_MEM_LOAD_COUNT);
  HOST(V3_PMON_MEM_STORE_COUNT);
  HOST(V3_PMON_CACHE_MISS_COUNT);
  HOST(V3_PMON_TLB_MISS_COUNT);
  V3_Print(info->vm_info, info, "%sHOST:%u:%u:%s = %llu\n", hdr, info->vcpu_id, info->pcpu_id, "UCPI",info->pmu_telem.host_ucpi_estimate);
  V3_Print(info->vm_info, info, "%sHOST:%u:%u:%s = %llu\n", hdr, info->vcpu_id, info->pcpu_id, "UMPL", info->pmu_telem.host_umpl_estimate);

  return 0;
}

  
static void telemetry_pmu(struct v3_vm_info * vm, void * private_data, char * hdr) 
{
  int i;
  struct guest_info *core = NULL;
  
  /*
   * work through each pcore (vcore for now per excluding oversubscription) and gathering info
   */
  for(i=0; i<vm->num_cores; i++) {
    core = &(vm->cores[i]);
    if((core->core_run_state != CORE_RUNNING)) continue;
    print_pmu_data(core, hdr);
  }
}


#define START(WHAT) \
do { \
  if(v3_pmu_start_tracking(WHAT) == -1) { \
    PrintError(info->vm_info, info, "Failed to start tracking of %s\n", #WHAT); \
    info->pmu_telem.active_counters[WHAT]=0; \
  } else { \
  info->pmu_telem.active_counters[WHAT]=1;\
  } \
 } while (0) 

#define STOP(WHAT) \
do { \
  if(info->pmu_telem.active_counters[WHAT]) {				\
    if (v3_pmu_stop_tracking(WHAT) == -1) {				\
      PrintError(info->vm_info, info, "Failed to stop tracking of %s\n", #WHAT); \
    }									\
    info->pmu_telem.active_counters[WHAT]=0; \
   }	   \
 } while (0) 


void v3_pmu_telemetry_start(struct guest_info *info)
{
  if (!info->vm_info->enable_telemetry) {
    return;
  }

  memset(&(info->pmu_telem),0,sizeof(struct v3_core_pmu_telemetry));

  v3_pmu_init();
  
  START(V3_PMON_RETIRED_INST_COUNT);
  START(V3_PMON_CLOCK_COUNT);
  START(V3_PMON_MEM_LOAD_COUNT);
  START(V3_PMON_MEM_STORE_COUNT);
  START(V3_PMON_CACHE_MISS_COUNT);
  START(V3_PMON_TLB_MISS_COUNT);


  info->pmu_telem.state=AWAIT_FIRST_ENTRY;


  if (info->vcpu_id==0) { 
    v3_add_telemetry_cb(info->vm_info, telemetry_pmu, NULL);
  }

}

static void inline snapshot(uint64_t vals[]) {
  vals[V3_PMON_RETIRED_INST_COUNT] = v3_pmu_get_value(V3_PMON_RETIRED_INST_COUNT); 
  vals[V3_PMON_CLOCK_COUNT] = v3_pmu_get_value(V3_PMON_CLOCK_COUNT);
  vals[V3_PMON_MEM_LOAD_COUNT] = v3_pmu_get_value(V3_PMON_MEM_LOAD_COUNT);
  vals[V3_PMON_MEM_STORE_COUNT] = v3_pmu_get_value(V3_PMON_MEM_STORE_COUNT);
  vals[V3_PMON_CACHE_MISS_COUNT] = v3_pmu_get_value(V3_PMON_CACHE_MISS_COUNT);
  vals[V3_PMON_TLB_MISS_COUNT] = v3_pmu_get_value(V3_PMON_TLB_MISS_COUNT);
}  


#define ALPHA_DENOM         8  // we are counting in 8ths
#define ALPHA_NUM           1  // 1/8 to estimate
#define OM_ALPHA_NUM        7  // 7/8 to the update

static inline void update_ucpi_estimate(uint64_t *estimate, uint64_t cur[], uint64_t last[])
{
  // 1e6 times the number of cycles since last
  uint64_t ucycles = 1000000 * (cur[V3_PMON_CLOCK_COUNT] - last[V3_PMON_CLOCK_COUNT]);
  uint64_t insts = cur[V3_PMON_RETIRED_INST_COUNT] - last[V3_PMON_RETIRED_INST_COUNT];

  if (insts==0) { 
    return;
  }


  *estimate = ((ALPHA_NUM * (*estimate)) + (OM_ALPHA_NUM * ((ucycles/insts)))) / ALPHA_DENOM;
                  
}

static inline void update_umpl_estimate(uint64_t *estimate, uint64_t cur[], uint64_t last[])
{
  // 1e6 times the number of misses since the last time
  uint64_t umisses = 1000000 * (cur[V3_PMON_CACHE_MISS_COUNT] - last[V3_PMON_CACHE_MISS_COUNT]);
  uint64_t loads = cur[V3_PMON_MEM_LOAD_COUNT] - last[V3_PMON_MEM_LOAD_COUNT];

  if (loads==0) {
    return;
  }
 
  *estimate = ((ALPHA_NUM * (*estimate)) + (OM_ALPHA_NUM * ((umisses/loads)))) / ALPHA_DENOM;
                  
}

void v3_pmu_telemetry_enter(struct guest_info *info)
{
  if (!info->vm_info->enable_telemetry) {
    return;
  }

  switch (info->pmu_telem.state) { 
  case AWAIT_FIRST_ENTRY:
    snapshot(info->pmu_telem.last_snapshot);
    info->pmu_telem.state=AWAIT_EXIT;
    break;
  
  case AWAIT_ENTRY: {
    // AWAIT_ENTRY - the snapshot in the struct is from the last exit
    uint64_t snap[PMU_NUM_COUNTERS];
    int i;

    snapshot(snap);

    for (i=0;i<PMU_NUM_COUNTERS;i++) { 
      info->pmu_telem.host_counts[i] += snap[i] - info->pmu_telem.last_snapshot[i];
    }

    if (HAVE(V3_PMON_CLOCK_COUNT) && HAVE(V3_PMON_RETIRED_INST_COUNT)) { 
      update_ucpi_estimate(&(info->pmu_telem.host_ucpi_estimate), snap, info->pmu_telem.last_snapshot);
    }
    if (HAVE(V3_PMON_CACHE_MISS_COUNT) && HAVE(V3_PMON_MEM_LOAD_COUNT)) { 
      update_umpl_estimate(&(info->pmu_telem.host_umpl_estimate), snap, info->pmu_telem.last_snapshot);
    }

    for (i=0;i<PMU_NUM_COUNTERS;i++) { 
      info->pmu_telem.last_snapshot[i] = snap[i];
    }

    info->pmu_telem.state = AWAIT_EXIT;
  }
    break;

  default:
    PrintError(info->vm_info, info, "Impossible state in on pmu telemetry entry\n");
    break;
  }
  
}



void v3_pmu_telemetry_exit(struct guest_info *info)
{
  if (!info->vm_info->enable_telemetry) {
    return;
  }

  switch (info->pmu_telem.state) { 
  case AWAIT_EXIT: {
    // AWAIT_EXIT - the snapshot in the struct is from the last entryx
    uint64_t snap[PMU_NUM_COUNTERS];
    int i;

    snapshot(snap);

    for (i=0;i<PMU_NUM_COUNTERS;i++) { 
      info->pmu_telem.guest_counts[i] += snap[i] - info->pmu_telem.last_snapshot[i];
    }

    if (HAVE(V3_PMON_CLOCK_COUNT) && HAVE(V3_PMON_RETIRED_INST_COUNT)) { 
      update_ucpi_estimate(&(info->pmu_telem.guest_ucpi_estimate), snap, info->pmu_telem.last_snapshot);
    } 

    if (HAVE(V3_PMON_CACHE_MISS_COUNT) && HAVE(V3_PMON_MEM_LOAD_COUNT)) { 
      update_umpl_estimate(&(info->pmu_telem.guest_umpl_estimate), snap, info->pmu_telem.last_snapshot);
    }

    for (i=0;i<PMU_NUM_COUNTERS;i++) { 
      info->pmu_telem.last_snapshot[i] = snap[i];
    }

    info->pmu_telem.state = AWAIT_ENTRY;
  }
    break;
  default:
    PrintError(info->vm_info, info, "Impossible state in on pmu telemetry exit\n");
    break;
  }
  
}

void v3_pmu_telemetry_end(struct guest_info *info)
{
  if (!info->vm_info->enable_telemetry) {
    return;
  }

  STOP(V3_PMON_RETIRED_INST_COUNT);
  STOP(V3_PMON_CLOCK_COUNT);
  STOP(V3_PMON_MEM_LOAD_COUNT);
  STOP(V3_PMON_MEM_STORE_COUNT);
  STOP(V3_PMON_CACHE_MISS_COUNT);
  STOP(V3_PMON_TLB_MISS_COUNT);

  v3_pmu_deinit();
  
  info->pmu_telem.state=AWAIT_FIRST_ENTRY;

  // Umm.... there is no v3_remove_telemtry_cb ?  WTF? 
}
