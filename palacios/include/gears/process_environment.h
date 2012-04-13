/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2011, Kyle C. Hale <kh@u.northwestern.edu> 
 * Copyright (c) 2011, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __PROCESS_ENVIRONMENT_H__
#define __PROCESS_ENVIRONMENT_H__

#ifdef __V3VEE__


#include <palacios/vmm.h>
#include <palacios/vmm_types.h>
#include <palacios/vm_guest.h>


struct v3_execve_varchunk {
	char ** argv;
	char ** envp;
	uint_t argc;
	uint_t envc;
	uint_t bytes;
	int active;
};

int v3_replace_arg (struct guest_info * core, uint_t argnum, const char * newval);
int v3_replace_env (struct guest_info * core, const char * envname, const char * newval);

int v3_inject_strings (struct guest_info * core, const char ** argstrs, const char ** envstrs, uint_t argcnt, uint_t envcnt);

addr_t v3_prepare_guest_stack (struct guest_info * core, uint_t bytes_needed);


#endif

#endif 

