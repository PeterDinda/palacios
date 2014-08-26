#ifndef __VMM_PSTATE_CTRL_H__
#define __VMM_PSTATE_CTRL_H__

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

#include <palacios/vmm_types.h>

struct v3_cpu_pstate_chars {
    // bit mask of features this host implentation has
    uint32_t features;
#define V3_PSTATE_EXTERNAL_CONTROL 1  // host will expose user control to Palacios
#define V3_PSTATE_DIRECT_CONTROL   2  // host will directly manipulate HW pstate if asked by Palacios
#define V3_PSTATE_INTERNAL_CONTROL 4  // host will deactivate its control so Palacios can manipulate hardware itself
    uint32_t cur_mode;      // current mode (0=>NONE - host control), else one of the above
    uint64_t min_freq_khz;  // minimum frequency that can be configed by EXTERANL_CONTROL
    uint64_t max_freq_khz;  // maximum frequency that can be configed by EXTERANL_CONTROL
    uint64_t cur_freq_khz;  // current selected frequency only meaningful under EXTERANL CONTROL
    // Note that "pstate" is an opaque quantity not necessarily the 
    // ACPI p-state model, although on some processors they are the same
    uint64_t min_pstate;    // minimum pstate that can be configed by DIRECT_CONTROL
    uint64_t max_pstate;    // maximum pstate that can be configed by DIRECT_CONTROL
    uint64_t cur_pstate;    // current selected pstate only meaningful under DIRECT_CONTROL
} ;


struct v3_host_pstate_ctrl_iface {
    // get characteristics of the implementation
    void (*get_chars)(struct v3_cpu_pstate_chars *chars);
    // acquire control on the caller's core
    // ttype=V3_PSTATE_DIRECT_CONTROL or V3_PSTATE_INTERNAL_CONTROL or V3_PSTATE_EXTERNAL_CONTROL
    void (*acquire)(uint32_t type);
    void (*release)(void);
    // pstate control applies if we have acquired DIRECT_CONTROL
    void (*set_pstate)(uint64_t pstate);
    uint64_t (*get_pstate)(void);
    // freq control applies if we have acquired EXTERNAL_CONTROL
    void (*set_freq)(uint64_t freq_khz);
    uint64_t (*get_freq)(void);
    // if we have INTERNAL_CONTROL, we do as we please
};

extern void V3_Init_Pstate_Ctrl (struct v3_host_pstate_ctrl_iface * palacios_pstate_ctrl);

#ifdef __V3VEE__

// get characteristics of the calling core
void v3_get_cpu_pstate_chars(struct v3_cpu_pstate_chars *chars);

// Tell the host to acquire control over this core
void v3_acquire_pstate_ctrl(uint32_t type);

// for DIRECT_CONTROL
uint64_t v3_get_cpu_pstate(void);
void     v3_set_cpu_pstate (uint64_t p);

// for EXTERANL_CONTROL
uint64_t v3_get_cpu_freq(void);
void     v3_set_cpu_freq(uint64_t freq_khz);

// Tell the host to release control over this core
void v3_release_pstate_control(); 


#endif 

#endif
