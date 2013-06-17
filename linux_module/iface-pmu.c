/*
 * PMU
 * (c) Chang S. Bae, 2013
 */

#include <linux/cdev.h>
#include <linux/errno.h>
#include <asm/msr.h>
#include <asm/msr-index.h>

#include <palacios/vmm_types.h>
#include <palacios/vmm_util.h>
#include <interfaces/vmm_pmu.h>

#include "vm.h"
#include "palacios.h"
#include "iface-pmu-intel.h"
#include "iface-pmu-amd.h"
#include "util-queue.h"
#include "linux-exts.h"


// Number of inits/deinits we have seen (inc on init, dec on deinit)
// This is per CPU - init/deinit mean init/deinit PMU
// tracking ON THE CURRENT CORE
static DEFINE_PER_CPU(u32, pmu_refcount) = 0;


/*
 * some macros may be commonly used
 */
#define MSR_READ(msrs, c) do {rdmsrl((c), (msrs).q);} while (0)
#define MSR_WRITE(msrs, c) do {wrmsrl((c), (msrs).q);} while (0)
#define SET_BIT(val, i) ((val) |= (1 << i))
#define CLEAR_BIT(val, u, i) ((val) &= ~((u&1) << i))
#define SET_BYTE(val, u, i) ((val) |= ((u&255) << i))
#define CHECK_BIT(val, i) ((val) & (1U << i))


static inline void cpuid_string(u32 id, u32 dest[4]) {
  asm volatile("cpuid"
	       :"=a"(*dest),"=b"(*(dest+1)),"=c"(*(dest+2)),"=d"(*(dest+3))
	       :"a"(id));
}


static int get_cpu_vendor(char name[13])
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

static int is_intel(void)
{
  char name[13];
  get_cpu_vendor(name);
  return !strcmp(name,"GenuineIntel");
}

static int is_amd(void)
{
  char name[13];
  get_cpu_vendor(name);
  return !strcmp(name,"AuthenticAMD");
}



/*
 * AMD and Intel implementations are distinguished by prefix: INTEL or AMD
 */

/*
 * name: *_get_slot
 * description: check available slots in pmu
 * return: -1 if none, else returns index: 0 ... 3
 */

static int intel_get_slot(void) {

  int i, slot;
  struct msr control;

  slot = -1;
  control.q = 0x0;

  for (i=0; i<INTEL_NUM_PMU_CONTROLS; i++) {
    INTEL_CTRL_READ(control, i);
    if(control.q & (0x1<<INTEL_EN_BIT)) {
      continue;
    } else {
      slot = i;
      break;
    }
  }

  return slot;
}

static int amd_get_slot(void) {
  int i, slot;
  struct msr control;

  slot = -1;
  control.q = 0x0;

  for (i=0; i<AMD_NUM_PMU_CONTROLS; i++) {
    AMD_CTRL_READ(control, i);
    if(control.q & (0x1<<AMD_EN_BIT)) {
      continue;
    } else {
      slot = i;
      break;
    }
  }

  return slot;
  return -1;
}

/*
 * name: *_find_idx
 * description: find index of pmu register that is available
 */
static int intel_find_idx(uint8_t event, uint8_t mask) {
  int i;

  struct msr control;

  control.q = 0x0;

  for (i=0; i<INTEL_NUM_PMU_COUNTERS; i++) {
    INTEL_CTRL_READ(control, i);
    if((((control.l>>INTEL_EVENT_BIT) & 0xff) == event) &&
       (((control.l>>INTEL_UMASK_BIT) & 0xff) == mask)) {
      return i;
    }
  }

  return -1;
}


/*
 * following implementations : init, deinit, start_tracking, stop_track and get_value
 * specifically fit into the pmu interface
 */

static uint64_t intel_get_value(v3_pmon_ctr_t ctr) {
  /*
   * local variables
   */
  int ctr_idx;
  struct msr count;

  count.q = 0x0;

  switch(ctr) {
  case V3_PMON_CLOCK_COUNT:
    INTEL_FIXED_CTR_READ(count, INTEL_IDX_CLK_IN_FPMU);
    break;
  case V3_PMON_RETIRED_INST_COUNT:
    INTEL_FIXED_CTR_READ(count, INTEL_IDX_INST_IN_FPMU);
    break;
  case V3_PMON_MEM_LOAD_COUNT:
    if((ctr_idx = intel_find_idx(INTEL_MEM_INST_RETIRED, INTEL_LOADS)) >= 0) {
      INTEL_CTR_READ(count, ctr_idx);
    } else {
      goto INTEL_READ_FAILED;
    }
    break;
  case V3_PMON_MEM_STORE_COUNT:
    if((ctr_idx = intel_find_idx(INTEL_MEM_INST_RETIRED, INTEL_STORES)) >= 0) {
      INTEL_CTR_READ(count, ctr_idx);
    } else {
      goto INTEL_READ_FAILED;
    }
    break;
  case V3_PMON_CACHE_MISS_COUNT:
    if((ctr_idx = intel_find_idx(INTEL_MEM_LOAD_RETIRED, INTEL_L3_MISS)) >= 0) {
      INTEL_CTR_READ(count, ctr_idx);
    } else {
      goto INTEL_READ_FAILED;
    }
    break;
  case V3_PMON_TLB_MISS_COUNT:
    if((ctr_idx = intel_find_idx(INTEL_MEM_LOAD_RETIRED, INTEL_DTLB_MISS)) >= 0) {
      INTEL_CTR_READ(count, ctr_idx);
    } else {
      goto INTEL_READ_FAILED;
    }
    break;
  }

  return (uint64_t)count.q;

 INTEL_READ_FAILED:
  return 0;
}


static int intel_start_tracking(v3_pmon_ctr_t ctr) {
  /*
   * local variables
   */
  int ctr_idx;
  struct msr msrs;

  /*
   * check if available slot in PMU, except for fixed counters (Intel specific)
   */

  switch(ctr) {
  case V3_PMON_CLOCK_COUNT:
    INTEL_FIXED_CTRL_READ(msrs);
    msrs.l |= 0x3<<8;
    INTEL_FIXED_CTRL_WRITE(msrs);
    break;
  case V3_PMON_RETIRED_INST_COUNT:
    INTEL_FIXED_CTRL_READ(msrs);
    msrs.l |= 0x3;
    INTEL_FIXED_CTRL_WRITE(msrs);
    break;
  case V3_PMON_MEM_LOAD_COUNT:
    if((ctr_idx = intel_get_slot()) >= 0) {
      INTEL_CTRL_START(INTEL_MEM_INST_RETIRED, INTEL_LOADS, ctr_idx);
    } else {
      goto INTEL_START_FAILED;
    }
    break;
  case V3_PMON_MEM_STORE_COUNT:
    if((ctr_idx = intel_get_slot()) >= 0) {
      INTEL_CTRL_START(INTEL_MEM_INST_RETIRED, INTEL_STORES, ctr_idx);
    } else {
      goto INTEL_START_FAILED;
    }
    break;
  case V3_PMON_CACHE_MISS_COUNT:
    if((ctr_idx = intel_get_slot()) >= 0) {
      INTEL_CTRL_START(INTEL_MEM_LOAD_RETIRED, INTEL_L3_MISS, ctr_idx);
    } else {
      goto INTEL_START_FAILED;
    }
    break;
  case V3_PMON_TLB_MISS_COUNT:
    if((ctr_idx = intel_get_slot()) >= 0) {
      INTEL_CTRL_START(INTEL_MEM_LOAD_RETIRED, INTEL_DTLB_MISS, ctr_idx);
    } else {
      goto INTEL_START_FAILED;
    }
    break;
  }

  return 0;

 INTEL_START_FAILED:
  ERROR("ERROR: no more slot remains for pmon events\n");
  return -1;
}

/*
 * descript: disabling pmu event counts
 */

static int intel_stop_tracking(v3_pmon_ctr_t ctr) {
  /*
   * local variables
   */
  int ctr_idx = -1;
  struct msr msrs;

  /*
   * check if available slot in PMU, except
   */

  switch(ctr) {
  case V3_PMON_CLOCK_COUNT:
    INTEL_FIXED_CTRL_READ(msrs);
    msrs.l &= ~(0xf<<8);
    INTEL_FIXED_CTRL_WRITE(msrs);
    break;
  case V3_PMON_RETIRED_INST_COUNT:
    INTEL_FIXED_CTRL_READ(msrs);
    msrs.l &= ~(0xf);
    INTEL_FIXED_CTRL_WRITE(msrs);
    break;
  case V3_PMON_MEM_LOAD_COUNT:
    if((ctr_idx = intel_find_idx(INTEL_MEM_INST_RETIRED, INTEL_LOADS)) >= 0) {
      INTEL_CTRL_STOP(ctr_idx);
    } else {
      goto INTEL_STOP_FAILED;
    }
    break;
  case V3_PMON_MEM_STORE_COUNT:
    if((ctr_idx = intel_find_idx(INTEL_MEM_INST_RETIRED, INTEL_STORES)) >= 0) {
      INTEL_CTRL_STOP(ctr_idx);
    } else {
      goto INTEL_STOP_FAILED;
    }
    break;
  case V3_PMON_CACHE_MISS_COUNT:
    if((ctr_idx = intel_find_idx(INTEL_MEM_LOAD_RETIRED, INTEL_L3_MISS)) >= 0) {
      INTEL_CTRL_STOP(ctr_idx);
    } else {
      goto INTEL_STOP_FAILED;
    }
    break;
  case V3_PMON_TLB_MISS_COUNT:
    if((ctr_idx = intel_find_idx(INTEL_MEM_LOAD_RETIRED, INTEL_DTLB_MISS)) >= 0) {
      INTEL_CTRL_STOP(ctr_idx);
    } else {
      goto INTEL_STOP_FAILED;
    }
    break;
  }

  return 0;

 INTEL_STOP_FAILED:
  ERROR("ERROR: no more slot remains for pmon events\n");
  return -1;
}

static void intel_pmu_init(void) {
  int i;
  struct msr control;

  if ((get_cpu_var(pmu_refcount)++) > 1) {
    put_cpu_var(pmu_refcount);
    // only the first init clears the pmu
    return;
  }
  put_cpu_var(pmu_refcount);
    

  control.q=0x0;

  /*
   * per Intel PMU architecture,
   * there are two class of counters
   * fixed ones (3 counters) and programmable ones (4 counters)
   * events for fixed coutners are determined, so enabling or not is the option
   * whereas, programmable ones are litterally programmable.
   */

  /*
   * enable fixed counters in global
   */
  MSR_READ(control, INTEL_IA32_PERF_GLOBAL_CTRL);
  control.q |= 0x70000000f; // enable fix counters (3 for the intel model)
  MSR_WRITE(control, INTEL_IA32_PERF_GLOBAL_CTRL);

  /*
   * disable in fixed counters control
   */

  INTEL_FIXED_CTRL_WRITE(control);

  /*
   * clean up programmable counter control
   */
  for (i=0; i<INTEL_NUM_PMU_CONTROLS; i++) {
    INTEL_CTRL_WRITE(control, i);
  }
}

static void intel_pmu_deinit(void) {
  if ((get_cpu_var(pmu_refcount)--)==0) {
    put_cpu_var(pmu_refcount);
    // actually deinit
    return;
  }
  put_cpu_var(pmu_refcount);
}





static int amd_find_idx(uint8_t event, uint8_t mask) {
  int i;

  struct msr control;

  control.q = 0x0;

  for (i=0; i<AMD_NUM_PMU_COUNTERS; i++) {
    AMD_CTRL_READ(control, i);
    if((((control.l>>AMD_EVENT_BIT) & 0xff) == event) &&
       (((control.l>>AMD_UMASK_BIT) & 0xff) == mask)) {
      return i;
    }
  }

  return -1;
}


static uint64_t amd_get_value(v3_pmon_ctr_t ctr) {
  int ctr_idx;
  struct msr count;

  count.q = 0x0;

  switch(ctr) {
  case V3_PMON_CLOCK_COUNT:
    if((ctr_idx = amd_find_idx(AMD_CLK_NOT_HALTED, 0x0)) >= 0) {
      AMD_CTR_READ(count, ctr_idx);
    } else {
      goto AMD_READ_FAILED;
    }
    break;
  case V3_PMON_RETIRED_INST_COUNT:
    if((ctr_idx = amd_find_idx(AMD_RETIRED_INSTRUCTIONS, 0x0)) >= 0) {
      AMD_CTR_READ(count, ctr_idx);
    } else {
      goto AMD_READ_FAILED;
    }
    break;
  case V3_PMON_MEM_LOAD_COUNT:
    if((ctr_idx = amd_find_idx(AMD_DATA_CACHE_ACCESSES, 0x0)) >= 0) {
      AMD_CTR_READ(count, ctr_idx);
    } else {
      goto AMD_READ_FAILED;
    }
    break;
  case V3_PMON_MEM_STORE_COUNT:
    if((ctr_idx = amd_find_idx(AMD_DATA_CACHE_ACCESSES, 0x0)) >= 0) {
      AMD_CTR_READ(count, ctr_idx);
    } else {
      goto AMD_READ_FAILED;
    }
    break;
  case V3_PMON_CACHE_MISS_COUNT:
    if((ctr_idx = amd_find_idx(AMD_DATA_CACHE_MISSES, 0x0)) >= 0) {
      AMD_CTR_READ(count, ctr_idx);
    } else {
      goto AMD_READ_FAILED;
    }
    break;
  case V3_PMON_TLB_MISS_COUNT:
    if((ctr_idx = amd_find_idx(AMD_L1_DTLB_AND_L2_DTLB_MISS, 0x7)) >= 0) {
      AMD_CTR_READ(count, ctr_idx);
    } else {
      goto AMD_READ_FAILED;
    }
    break;
  }

  return (uint64_t)count.q;

 AMD_READ_FAILED:
  return 0;
}

static int amd_start_tracking(v3_pmon_ctr_t ctr) {

  int ctr_idx;

  switch(ctr) {
  case V3_PMON_CLOCK_COUNT:
    if((ctr_idx = amd_get_slot()) >= 0) {
      AMD_CTRL_START(AMD_CLK_NOT_HALTED, 0x0, ctr_idx);
    } else {
      goto AMD_START_FAILED;
    }
    break;
  case V3_PMON_RETIRED_INST_COUNT:
    if((ctr_idx = amd_get_slot()) >= 0) {
      AMD_CTRL_START(AMD_RETIRED_INSTRUCTIONS, 0x0, ctr_idx);
    } else {
      goto AMD_START_FAILED;
    }
    break;
  case V3_PMON_MEM_LOAD_COUNT:
    if((ctr_idx = amd_get_slot()) >= 0) {
      AMD_CTRL_START(AMD_DATA_CACHE_ACCESSES, 0x0, ctr_idx);
    } else {
      goto AMD_START_FAILED;
    }
    break;
  case V3_PMON_MEM_STORE_COUNT:
    if((ctr_idx = amd_get_slot()) >= 0) {
      AMD_CTRL_START(AMD_DATA_CACHE_ACCESSES, 0x0, ctr_idx);
    } else {
      goto AMD_START_FAILED;
    }
    break;
  case V3_PMON_CACHE_MISS_COUNT:
    if((ctr_idx = amd_get_slot()) >= 0) {
      AMD_CTRL_START(AMD_DATA_CACHE_MISSES, 0x0, ctr_idx);
    } else {
      goto AMD_START_FAILED;
    }
    break;
  case V3_PMON_TLB_MISS_COUNT:
    if((ctr_idx = amd_get_slot()) >= 0) {
      AMD_CTRL_START(AMD_L1_DTLB_AND_L2_DTLB_MISS, 0x7, ctr_idx);
    } else {
      goto AMD_START_FAILED;
    }
    break;
  }

  return 0;

 AMD_START_FAILED:
  ERROR("ERROR: no more slot remains for pmon events\n");
  return -1;
}


static int amd_stop_tracking(v3_pmon_ctr_t ctr) {

  int ctr_idx = -1;


  switch(ctr) {
  case V3_PMON_CLOCK_COUNT:
    if((ctr_idx = amd_find_idx(AMD_CLK_NOT_HALTED, 0x0)) >= 0) {
      AMD_CTRL_STOP(ctr_idx);
    } else {
      goto AMD_STOP_FAILED;
    }
    break;
  case V3_PMON_RETIRED_INST_COUNT:
    if((ctr_idx = amd_find_idx(AMD_RETIRED_INSTRUCTIONS, 0x0)) >= 0) {
      AMD_CTRL_STOP(ctr_idx);
    } else {
      goto AMD_STOP_FAILED;
    }
    break;
  case V3_PMON_MEM_LOAD_COUNT:
    if((ctr_idx = amd_find_idx(AMD_DATA_CACHE_ACCESSES, 0x0)) >= 0) {
      AMD_CTRL_STOP(ctr_idx);
    } else {
      goto AMD_STOP_FAILED;
    }
    break;
  case V3_PMON_MEM_STORE_COUNT:
    if((ctr_idx = amd_find_idx(AMD_DATA_CACHE_ACCESSES, 0x0)) >= 0) {
      AMD_CTRL_STOP(ctr_idx);
    } else {
      goto AMD_STOP_FAILED;
    }
    break;
  case V3_PMON_CACHE_MISS_COUNT:
    if((ctr_idx = amd_find_idx(AMD_DATA_CACHE_MISSES, 0x0)) >= 0) {
      AMD_CTRL_STOP(ctr_idx);
    } else {
      goto AMD_STOP_FAILED;
    }
    break;
  case V3_PMON_TLB_MISS_COUNT:
    if((ctr_idx = amd_find_idx(AMD_L1_DTLB_AND_L2_DTLB_MISS, 0x7)) >= 0) {
      AMD_CTRL_STOP(ctr_idx);
    } else {
      goto AMD_STOP_FAILED;
    }
    break;
  }

  return 0;

 AMD_STOP_FAILED:
  ERROR("ERROR: no more slot remains for pmon events\n");
  return -1;
}


static void amd_pmu_init(void) {

  int i;
  struct msr control;

  
  if ((get_cpu_var(pmu_refcount)++) > 1) {
    put_cpu_var(pmu_refcount);
    // only the first init clears the pmu
    return;
  }
  put_cpu_var(pmu_refcount);
  
  

  // initialize variables
  control.q=0x0;

  /*
   * clean up programmable counter control
   */
  for (i=0; i<AMD_NUM_PMU_CONTROLS; i++) {
    AMD_CTRL_WRITE(control, i);
  }
}

static void amd_pmu_deinit(void) {
  if ((get_cpu_var(pmu_refcount)--)==0) {
    put_cpu_var(pmu_refcount);
    // actually deinit
    return;
  }
  put_cpu_var(pmu_refcount);
}


static struct v3_pmu_iface palacios_pmu_intel = {
  .init = intel_pmu_init,
  .deinit = intel_pmu_deinit,
  .start_tracking = intel_start_tracking,
  .stop_tracking = intel_stop_tracking,
  .get_value = intel_get_value
};

static struct v3_pmu_iface palacios_pmu_amd = {
  .init = amd_pmu_init,
  .deinit = amd_pmu_deinit,
  .start_tracking = amd_start_tracking,
  .stop_tracking = amd_stop_tracking,
  .get_value = amd_get_value
};

static int pmu_init( void ) {
  if (is_intel()) {
    INFO("Intel PMU featureset detected\n");
    V3_Init_PMU(&palacios_pmu_intel);
  } else if (is_amd()) { 
    INFO("AMD PMU featureset detected\n");
    V3_Init_PMU(&palacios_pmu_amd);
  } else {
    ERROR("This is neither an Intel nor AMD machine - No PMU functionality configured\n");
    return -1;
  }

  return 0;
}


static struct linux_ext pmu_ext = {
  .name = "PMU",
  .init = pmu_init,
  .deinit = NULL,
  .guest_init = NULL,
  .guest_deinit = NULL
};

register_extension(&pmu_ext);




