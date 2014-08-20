#ifndef _V3_USER_DVFS_
#define _V3_USER_DVFS_

#include <stdint.h>
#include "v3_ctrl.h"
#include "iface-pstate-ctrl.h"

// Use these together
int v3_user_dvfs_acquire_direct(uint32_t core); 
int v3_user_dvfs_set_pstate(uint32_t core, uint8_t pstate);

// Use these together
int v3_user_dvfs_acquire_external(uint32_t core); 
int v3_user_dvfs_set_freq(uint32_t core, uint64_t freq_khz);

int v3_user_dvfs_release(uint32_t core); 

#endif

