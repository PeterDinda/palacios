/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2013, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2013, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __RAPL_H__
#define __RAPL_H__

#include <palacios/vmm_types.h>

typedef enum {
	V3_PWRSTAT_PKG_ENERGY,
	V3_PWRSTAT_CORE_ENERGY,
	V3_PWRSTAT_EXT_ENERGY, /* "Power plane 1", e.g. graphics peripheral */
	V3_PWRSTAT_DRAM_ENERGY,
} v3_pwrstat_ctr_t;

/* note that (on Intel at least) RAPL doesn't adhere to PMU semantics */
struct v3_pwrstat_iface {
	int (*init)(void);
	int (*deinit)(void);
	int (*ctr_valid)(v3_pwrstat_ctr_t ctr);
	uint64_t (*get_value)(v3_pwrstat_ctr_t ctr);
};

extern void V3_Init_Pwrstat (struct v3_pwrstat_iface * palacios_pwrstat);

#ifdef __V3VEE__
void v3_pwrstat_init(void);
int v3_pwrstat_ctr_valid(v3_pwrstat_ctr_t ctr);
uint64_t v3_pwrstat_get_value(v3_pwrstat_ctr_t ctr);
void v3_pwrstat_deinit(void);

#endif

#endif
