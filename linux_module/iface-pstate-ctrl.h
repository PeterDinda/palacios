#ifndef __IFACE_PSTATE_CTRL_H__
#define __IFACE_PSTATE_CTRL_H__

/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2014, the V3VEE Project <http://www.v3vee.org>
 * all rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *         Shiva Rao <shiva.rao.717@gmail.com>
 *         Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  you are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */



#include <interfaces/vmm_pstate_ctrl.h>

// These functions are available for use within the module
// They affect the current core

void palacios_pstate_ctrl_get_chars(struct v3_cpu_pstate_chars *c);

void palacios_pstate_ctrl_acquire(uint32_t type);
void palacios_pstate_ctrl_release(void);


uint8_t palacios_pstate_ctrl_get_pstate(void);
void    palacios_pstate_ctrl_set_pstate(uint8_t p);

uint64_t palacios_pstate_ctrl_get_freq(void);
void     palacios_pstate_ctrl_set_freq(uint64_t f_khz);


// This structure is how the user space commands us
struct v3_dvfs_ctrl_request {
    enum {V3_DVFS_ACQUIRE,             // Take control over a pcore from host
	  V3_DVFS_RELEASE,             // Release control of a pcore to host
	  V3_DVFS_SETFREQ,             // Set frequency of acquired pcore
	  V3_DVFS_SETPSTATE} cmd;      // Set pstate of acquired pcore
    enum {V3_DVFS_EXTERNAL,
	  V3_DVFS_DIRECT }   acq_type; // External for setting freq using Linux
                                       // Direct for setting  pstate directly using module
    uint32_t                 pcore;    // Which core we mean
    uint64_t                 freq_khz; // for setfreq
    uint8_t                  pstate;   // for setpstate
};

#endif 
