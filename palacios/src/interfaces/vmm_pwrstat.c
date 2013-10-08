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
 * 
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>
#include <palacios/vmm_debug.h>
#include <palacios/vmm_types.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_lowlevel.h>

#include <interfaces/vmm_pwrstat.h>

struct v3_pwrstat_iface * palacios_pwrstat = 0;


void v3_pwrstat_init (void) 
{
    if (palacios_pwrstat && palacios_pwrstat->init) {
        palacios_pwrstat->init();
    }
}


void v3_pwrstat_deinit (void) 
{
    if (palacios_pwrstat && palacios_pwrstat->deinit) {
        palacios_pwrstat->deinit();
    }
}


uint64_t v3_pwrstat_get_value (v3_pwrstat_ctr_t ctr)
{
    if (palacios_pwrstat && palacios_pwrstat->get_value) {
        return palacios_pwrstat->get_value(ctr);
    }

    return -1;
}


int v3_pwrstat_ctr_valid(v3_pwrstat_ctr_t ctr)
{
    if (palacios_pwrstat && palacios_pwrstat->ctr_valid) {
        return palacios_pwrstat->ctr_valid(ctr);
    }

    return -1;
}


void V3_Init_Pwrstat (struct v3_pwrstat_iface * pwrstat_iface) 
{
    palacios_pwrstat = pwrstat_iface;
}


