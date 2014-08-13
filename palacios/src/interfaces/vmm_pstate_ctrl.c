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

#include <palacios/vmm.h>

#include <interfaces/vmm_pstate_ctrl.h>

static struct v3_host_pstate_ctrl_iface *pstate_ctrl_hooks = 0;


void V3_Init_Pstate_Ctrl(struct v3_host_pstate_ctrl_iface *hooks)
{
    pstate_ctrl_hooks = hooks;

    PrintDebug(VM_NONE, VCORE_NONE, "V3 host p-state control interface inited\n");

    return;
}


void v3_get_cpu_pstate_chars(struct v3_cpu_pstate_chars *chars)
{
    if (pstate_ctrl_hooks && pstate_ctrl_hooks->get_chars) { 
	pstate_ctrl_hooks->get_chars(chars);
    }
}


void v3_acquire_pstate_ctrl(uint32_t type)
{
    if (pstate_ctrl_hooks && pstate_ctrl_hooks->acquire) { 
	pstate_ctrl_hooks->acquire(type);
    }
}


uint8_t v3_get_cpu_pstate(void)
{
    if (pstate_ctrl_hooks && pstate_ctrl_hooks->get_pstate) { 
	return pstate_ctrl_hooks->get_pstate();
    } else {
	return 0;
    }
}

void    v3_set_cpu_pstate (uint8_t p)
{
    if (pstate_ctrl_hooks && pstate_ctrl_hooks->set_pstate) { 
	pstate_ctrl_hooks->set_pstate(p);
    }
}

uint64_t v3_get_cpu_freq(void)
{
    if (pstate_ctrl_hooks && pstate_ctrl_hooks->get_freq) { 
	return pstate_ctrl_hooks->get_freq();
    } else {
	return 0;
    }
}

void    v3_set_cpu_freq(uint64_t f)
{
    if (pstate_ctrl_hooks && pstate_ctrl_hooks->set_freq) { 
	pstate_ctrl_hooks->set_freq(f);
    }
}

// If using direct control, relinquish
void v3_release_pstate_control()
{
    if (pstate_ctrl_hooks && pstate_ctrl_hooks->release) { 
	pstate_ctrl_hooks->release();
    }
}
